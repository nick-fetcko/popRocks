#include "AlbumArt.hpp"

#include <map>
#include <algorithm>
#include <thread>

#include <lodepng.h>

#include "MathCPP/Duration.hpp"

#include "Bicubic.hpp"
#include "Buffer.hpp"
#include "Gaussian.hpp"
#include "Hash.hpp"
#include "HDR.hpp"
#include "Settings.hpp"
#include "Utils.hpp"

using namespace MathsCPP;

AlbumArt::AlbumArt(std::unique_ptr<Context> &context) : context(context) {
	radius = Settings::settings.GetRadius();
}

AlbumArt::~AlbumArt() {
	delete[] embeddedData;
}

void AlbumArt::OnInit(int windowWidth, int windowHeight, float scale) {
	this->scale = scale;
	radius *= scale;

	if (context) {
		context->With("rotate"_hash, [this](Context::Shader &shader) {
			shader.program.Uniform1f("radius"_hash, radius);
		});
	}

	Circle::OnInit(radius);

	squareVao = std::make_unique<VertexArray>();
	squareVbo = std::make_unique<ArrayBuffer>();
	squareEab = std::make_unique<ElementBuffer>();

	std::vector<float> squareBuffer = {
		0, 0, Buffers::TexCoordBuffer[0], Buffers::TexCoordBuffer[1],
		0, 0, Buffers::TexCoordBuffer[2], Buffers::TexCoordBuffer[3],
		0, 0, Buffers::TexCoordBuffer[4], Buffers::TexCoordBuffer[5],
		0, 0, Buffers::TexCoordBuffer[6], Buffers::TexCoordBuffer[7]
	};

	squareVao->Bind();
	squareVbo->Bind();
	squareVao->AddAttribute(VertexArray::Attribute(0, 2, 4 * sizeof(float)));
	squareVao->AddAttribute(VertexArray::Attribute(1, 2, 4 * sizeof(float), 2 * sizeof(float)));
	squareVbo->BufferData(squareBuffer);
	squareVbo->Unbind();
	squareVao->Unbind();

	squareEab->Bind();
	squareEab->BufferData<std::size(Buffers::SquareBuffer)>(Buffers::SquareBuffer);
	squareEab->Unbind();

	cube = std::make_unique<Cube>(Utils::GetResource(
		std::filesystem::path("LUTs") / Settings::settings.GetLut()
	));
}

void AlbumArt::OnResize(int windowWidth, int windowHeight, float scale) {
	if (this->scale != scale) {
		// FIXME: will floating point precision errors accumulate here?
		radius /= this->scale;
		this->scale = scale;
		radius *= scale;

		LogDebug("Scale changed! New radius is ", radius);

		UpdateVertexCoords();

		Scale(true);
	}
}

void AlbumArt::SetRadius(float radius) {
	Circle::SetRadius(radius);
	Settings::settings.SetRadius(radius);
}

void AlbumArt::UpdateVertexCoords() {
	Circle::UpdateVertexCoords();

	if (context) {
		context->With("rotate"_hash, [this](Context::Shader &shader) {
			shader.program.Uniform1f("radius"_hash, radius);
		});
	}
}

void AlbumArt::OnLoop(GLfloat x, GLfloat y, float frameCount, Context &context) {
	// try_lock so we don't miss a frame or two
	if (scalingMutex.try_lock()) {
		if (surfaceToLoad) {
			LoadFromSurface(surfaceToLoad, true);
			surfaceToLoad = nullptr;

			// We only want to keep lastSurface
			// around for as long as we need
			// to scale it down.
			// 
			// [16Jul2025] We now want to keep the un-scaled surface
			//             around as it might need rescaling when the
			//             DPI changes
			// 
//			SDL_FreeSurface(lastSurface);
//			lastSurface = nullptr;
		}
		scalingMutex.unlock();
	}

	if (albumLoaded && !hidden) {
		context.Color(HDR::WhiteLevel, HDR::WhiteLevel, HDR::WhiteLevel, 1.0f);

		context.GetShaderProgram().Uniform1i("hdr"_hash, HDR::Enabled);

		context.Translate(
			x,
			y,
			0
		);
		context.Rotate(
			frameCount,
			0.0f,
			0.0f,
			1.0f
		);
		context.Apply();

		glActiveTexture(GL_TEXTURE0 + 0);
		glBindTexture(GL_TEXTURE_2D, album);

		glActiveTexture(GL_TEXTURE0 + 1);
		cube->Bind();

		Circle::OnLoop(0, 0, context);

		cube->Unbind();

		glActiveTexture(GL_TEXTURE0 + 0);

		context.GetShaderProgram().Uniform1i("hdr"_hash, 0);

		context.LoadIdentity();
	}
}

int AlbumArt::DrawSquare(int x, int y, int height, GLfloat alpha, Context &context) {
	if (albumLoaded && !hidden) {
		// If our size changed, update the vertex buffer
		if (squareHeight != height ||
			squareWidth != height * aspectRatio) {
			squareHeight = height;
			squareWidth = height * aspectRatio;

			squareVbo->Bind();
			squareVbo->BufferSubData(5, sizeof(float), &squareHeight);
			squareVbo->BufferSubData(8, sizeof(float), &squareWidth);
			squareVbo->BufferSubData(9, sizeof(float), &squareHeight);
			squareVbo->BufferSubData(12, sizeof(float), &squareWidth);
			squareVbo->Unbind();
		}
		
		context.Color(HDR::WhiteLevel, HDR::WhiteLevel, HDR::WhiteLevel, alpha);
		context.GetShaderProgram().Uniform1i("hdr"_hash, HDR::Enabled);

		context.Translate(
			static_cast<GLfloat>(x),
			static_cast<GLfloat>(y),
			0.0f
		);
		context.Apply();
		glBindTexture(GL_TEXTURE_2D, album);

		glActiveTexture(GL_TEXTURE0 + 1);
		cube->Bind();

		squareVao->Bind();
		squareEab->Bind();
		squareEab->DrawElements(GL_TRIANGLES);
		squareEab->Unbind();
		squareVao->Unbind();

		cube->Unbind();

		glActiveTexture(GL_TEXTURE0 + 0);

		context.GetShaderProgram().Uniform1i("hdr"_hash, 0);

		context.LoadIdentity();

		// Return our width
		return static_cast<int>(squareWidth);
	}

	return 0;
}

void AlbumArt::OnDestroy() {
	{
		std::unique_lock lock(scalingMutex);
		scaling = false;
	}

	if (scaleThread.joinable())
		scaleThread.join();
	{
		std::unique_lock lock(histogramMutex);
		processingColors = false;		
	}
	if (colorProcessingThread.joinable())
		colorProcessingThread.join();

	Circle::OnDestroy();

	if (lastSurface)
		SDL_DestroySurface(lastSurface);

	glDeleteTextures(1, &album);
	album = 0;

	squareVao.reset();
	squareVbo.reset();
	squareEab.reset();

	cube.reset();
}

std::filesystem::path AlbumArt::FindArt(const std::filesystem::path &folder) {
	preferred.clear();
	found.clear();

	searchFolder = folder;

	auto find = [&](const std::filesystem::directory_entry &entry, bool breakOnFind = false) {
		auto extension = entry.path().extension().u8string();
		std::transform(extension.begin(), extension.end(), extension.begin(), tolower);
		if (IsSupported(extension)) {
			auto filename = entry.path().filename().u8string();
			std::transform(filename.begin(), filename.end(), filename.begin(), tolower);

			auto cover = filename.find("cover");
			auto front = filename.find("front");
			auto folder = filename.find("folder");

			if (cover == 0 ||
				front != std::string::npos ||
				folder == 0) {
				// Sort by digits in the filename (if there are any), ascending
				//
				// For example, if a folder has:
				//		"Cover 1.jpg"
				//		"Cover 2.jpg"
				//		"Cover 3.jpg"
				//
				// We'll use "Cover 1.jpg"
				auto [success, number] = Utils::ExtractDigitsFromString(filename);
				if (number == std::numeric_limits<int>::max() && (folder == 0 || cover == 0 || front == 0))
					--number;

				preferred.emplace(std::make_pair(number, entry.path()));

				// Optionally break when we find a preferred file
				//
				// This can be used with recursive_directory_iterator
				// to avoid scanning _every_ subfolder.
				//
				// My music folder has a number of unsorted tracks
				// in the root, and I don't want it scannning through
				// 1TB of data when trying to load album art for those
				//
				// In those cases, this will just pick the first (preferred)
				// album art file found in any of the subfolders. It should
				// really just pick _nothing_, but I'm not sure what a good
				// litmus test for this specific case would look like.
				if (breakOnFind)
					return true;
			} else if (found.find(entry.path()) == found.end()) found.emplace(entry.path());
		}

		return false;
	};

	// Try to find in our current folder first
	for (const auto &iter : std::filesystem::directory_iterator(folder))
		find(iter);

	// Then try any subfolders
	for (const auto &iter : std::filesystem::recursive_directory_iterator(folder)) {
		// We don't need to re-check files in the current folder
		if (iter.path().parent_path() == folder) continue;
		if (find(iter, true)) break;
	}

	return preferred.empty() ? found.empty() ? "" : *found.begin() : preferred.begin()->second;
}

void AlbumArt::ReprocessColors() {
	if (lastSurface) {
		{
			std::unique_lock lock(histogramMutex);
			processingColors = false;
		}

		if (colorProcessingThread.joinable())
			colorProcessingThread.join();

		processingColors = true;
		
		colorProcessingThread = std::thread([this] {
			auto destination = new Histogram();

			auto pixels = GetPixels(lastSurface);

			ProcessColors(destination, lastSurface, pixels);

			if (pixels != lastSurface->pixels)
				delete[] pixels;

			{
				std::unique_lock lock(histogramMutex);
				if (processingColors) {
					histogram = *destination;

					selectedColors.clear();
					for (auto iter = histogram.rbegin(); iter != histogram.rend(); ++iter)
						selectedColors.emplace_back(Colour<float>::FromHsv(iter->h, iter->s, iter->v));

					ResetBin(true);
				}
			}

			delete destination;
		});
	}
}

void AlbumArt::ProcessColors(Histogram *destination, SDL_Surface *surface, const uint8_t *pixels) {
	if (colorMethod == ColorMethod::Average) {
		// Go through every pixel to find an "average" color
		uint64_t averageR = 0;
		uint64_t averageG = 0;
		uint64_t averageB = 0;
		for (auto x = 0; x < surface->w && processingColors; ++x) {
			for (auto y = 0; y < surface->h && processingColors; ++y) {
				auto pos = y * SDL_BYTESPERPIXEL(surface->format) * surface->w + x * SDL_BYTESPERPIXEL(surface->format);

				averageR += pixels[pos];
				averageG += pixels[pos + 1];
				averageB += pixels[pos + 2];
			}
		}

		if (processingColors) {
			Colour color(
				(averageR / surface->w * surface->h) / 255.0f,
				(averageG / surface->w * surface->h) / 255.0f,
				(averageB / surface->w * surface->h) / 255.0f
			);

			auto hsv = color.ToHsv();
			hsv.s = 0.9f;
			hsv.v = 0.9f;

			averageColor = Colour<float>::FromHsv(hsv.h, hsv.s, hsv.v);

			for (const auto &listener : colorChangeListeners)
				listener->OnColorChanged(averageColor);
		}
	} else {
		std::map<float, Bin> histogram;

		Colour<float> color;

		// Still using nearest neighbor for this...
		// should we wait until Scale() is done before
		// loading the colors?
		//
		// n = 1, but "VA-11 HALL-A - Second Round"'s
		// album art sets a precedent for still
		// using nearest neighbor. Bicubic ultimately
		// makes the dominant color darker.
		//
		// Until proven otherwise, color selection is
		// wrapped in "if (!scaled)"
		auto hstep = std::max(1, static_cast<int>(surface->w / (radius * 2)));
		auto vstep = std::max(1, static_cast<int>(surface->h / (radius * 2)));

		double minSaturation = Settings::settings.GetColorSelection().minSaturation;
		double minValue = Settings::settings.GetColorSelection().minValue;

		while (histogram.empty()) {
			for (auto x = 0; x < surface->w && processingColors; x += hstep) {
				for (auto y = 0; y < surface->h && processingColors; y += vstep) {
					auto index = (y * surface->w + x) * SDL_BYTESPERPIXEL(surface->format);

					color.r = pixels[index] / 255.0f;
					color.g = pixels[index + 1] / 255.0f;
					color.b = pixels[index + 2] / 255.0f;

					auto hsv = color.ToHsv();

					// Round to the nearest 0.5
					// That gives us 720 possible hues
					//hsv.h = (std::round(hsv.h * 2)) / 2;

					// Round to the nearest _even_ number
					// This only gives us 180 possible hues,
					// but allows for fewer low-count bins
					hsv.h = std::round(std::round(hsv.h) / 2) * 2;

					// Exclude dark / low contrast colors
					if (hsv.s >= minSaturation && hsv.v >= minValue) {
						if (auto iter = histogram.find(hsv.h); iter != histogram.end()) {
							++iter->second.count;
							iter->second.s += hsv.s;

							/*
							if (hsv.s > iter->second.s)
								iter->second.s = hsv.s;
							*/

							iter->second.v += hsv.v;
						} else
							histogram.emplace(std::make_pair(hsv.h, Bin(1, hsv.h, hsv.s, hsv.v)));
					}
				}
			}

			// If we found nothing above the minimums,
			// disable them
			if (histogram.empty() && minSaturation > DBL_EPSILON) {
				minSaturation = 0.0;
				minValue = 0.0;
			} else
				break;
		}

		destination->clear();

		std::size_t maxCount = std::numeric_limits<std::size_t>::min();
		for (const auto &[hue, bin] : histogram) {
			if (!processingColors)
				break;

			if (bin.count > maxCount)
				maxCount = bin.count;
		}

		const auto minPercentage = static_cast<std::size_t>(
			maxCount * Settings::settings.GetColorSelection().minPercentage
		);

		for (auto &[hue, bin] : histogram) {
			if (!processingColors)
				break;

			// Filter out anything < a percentage of our max
			if (bin.count >= minPercentage) {
				destination->emplace(
					Bin(
						bin.count,
						bin.h,
						bin.s / bin.count,
						bin.v / bin.count
					)
				);
			}
		}

		// This compares each bin to _every_ other bin in the histogram
		/*
		for (auto iter = this->histogram.begin(); iter != this->histogram.end();) {
			bool erased = false;
			for (auto compare = this->histogram.begin(); compare != this->histogram.end(); ++compare) {
				// When hues are separated by less than a
				// certain number of degrees, choose the
				// one with the highest count and discard
				// the other.
				if (compare != iter &&
					std::abs(iter->second.h - compare->second.h) < minSeparation &&
					iter->first < compare->first
				) {
					iter = this->histogram.erase(iter);
					erased = true;
					break;
				}
			}
			if (!erased)
				++iter;
		}
		*/

		// This compares each bin to the last bin inserted into the histogram
		/*
		auto tempHistogram = this->histogram;
		this->histogram.clear();
		for (auto iter = tempHistogram.rbegin(); iter != tempHistogram.rend(); ++iter) {
			if (this->histogram.empty()) {
				this->histogram.emplace(*iter);
			} else if (std::abs(this->histogram.begin()->h - iter->h) > 25.0) {
				LogDebug(
					"Placing in histogram because hue is ",
					iter->h,
					" vs last bin's hue of ",
					this->histogram.begin()->h
				);
				this->histogram.emplace(*iter);
			}
		}
		*/

		// This compares each bin to the other, already-selected bins
		//
		// This also doesn't just take hue into account.
		// If we had to remove the minimum saturation / value filters,
		// we also check for value separation.
		auto tempHistogram = *destination;
		destination->clear();
		for (auto iter = tempHistogram.rbegin(); iter != tempHistogram.rend() && processingColors; ++iter) {
			if (destination->empty()) {
				destination->emplace(*iter);
			} else {
				bool found = true;
				for (auto compare = destination->begin(); compare != destination->end(); ++compare) {
					auto rgb = Colour<float>::FromHsv(iter->h, iter->s, iter->v);
					auto rgbComp = Colour<float>::FromHsv(compare->h, compare->s, compare->v);

					auto distance =
						std::sqrt(
							std::pow(rgbComp.r - rgb.r, 2) +
							std::pow(rgbComp.g - rgb.g, 2) +
							std::pow(rgbComp.b - rgb.b, 2)
						);

					// https://gamedev.stackexchange.com/a/4472
					// 360 - 0 (in degrees) needs to be 0, not 360
					if ((180 - abs(abs(iter->h - compare->h) - 180) < Settings::settings.GetColorSelection().minHueSeparation &&
						distance < Settings::settings.GetColorSelection().minRgbSeparation) ||
						(minSaturation <= DBL_EPSILON && std::abs(compare->v - iter->v) < Settings::settings.GetColorSelection().minValueSeparation))
						found = false;

					/*
					if (180 - abs(abs(iter->h - compare->h) - 180) < Settings::settings.GetColorSelection().minHueSeparation ||
						(minSaturation <= DBL_EPSILON && std::abs(compare->v - iter->v) < Settings::settings.GetColorSelection().minValueSeparation))
						found = false;
					*/
				}
				if (found)
					destination->emplace(*iter);
			}
		}

		// FIXME: In This Moment's "Blood"'s red
		//        is more pink right now, but
		//        selecting the max saturation
		//        instead of the average blows out
		//        colors on other albums like 
		//        "talking / Nana Hitsuji"

		// If our histogram contains only a single color,
		// add a darker / lighter variant of that color 
		// for beat detection to toggle through
		if (destination->size() == 1 && processingColors) {
			auto deeperColor = *destination->begin();

			if (deeperColor.v >= 0.5)
				deeperColor.v = std::clamp(deeperColor.v / 1.75f, 0.0f, 1.0f);
			else
				deeperColor.v = std::clamp(deeperColor.v * 1.75f, 0.0f, 1.0f);

			deeperColor.count -= 1;

			destination->emplace(std::move(deeperColor));
		}
	}
}

inline uint8_t *AlbumArt::GetPixels(SDL_Surface *surface) {
	if (surface->pitch == surface->w * SDL_BYTESPERPIXEL(surface->format)) {
		return reinterpret_cast<uint8_t *>(surface->pixels);
	} else {
		auto pixels = new uint8_t[surface->w * surface->h * SDL_BYTESPERPIXEL(surface->format)];
		for (int y = 0; y < surface->h; ++y) {
			memcpy(
				&pixels[y * surface->w * SDL_BYTESPERPIXEL(surface->format)],
				&((reinterpret_cast<uint8_t *>(surface->pixels))[y * surface->pitch]),
				surface->w * SDL_BYTESPERPIXEL(surface->format)
			);
		}
		return pixels;
	}
}

void AlbumArt::LoadFromSurface(SDL_Surface *surface, bool scaled) {
	glDeleteTextures(1, &album);
	glGenTextures(1, &album);
	glBindTexture(GL_TEXTURE_2D, album);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	albumWidth = surface->w;
	albumHeight = surface->h;

	uint8_t *pixels = GetPixels(surface);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, SDL_BITSPERPIXEL(surface->format) == 32 ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, pixels);

	if (!scaled) {
		// lastSurface is the last surface
		// _before_ scaling, so only update
		// it when we aren't scaling
		if (lastSurface)
			SDL_DestroySurface(lastSurface);

		lastSurface = surface;

		lastSurfaceUpdated = true;

		// If we have another thread processing colors,
		// stop it first.
		{
			std::unique_lock lock(histogramMutex);
			processingColors = false;
		}

		if (colorProcessingThread.joinable())
			colorProcessingThread.join();

		processingColors = true;

		ProcessColors(&histogram, surface, pixels);
		ResetBin();
	}

	selectedColors.clear();
	for (auto iter = histogram.rbegin(); iter != histogram.rend(); ++iter)
		selectedColors.emplace_back(Colour<float>::FromHsv(iter->h, iter->s, iter->v));

	if (pixels != surface->pixels)
		delete[] pixels;

	scaledAlbumWidth = surface->w;
	scaledAlbumHeight = surface->h;
	aspectRatio = static_cast<float>(surface->w) / surface->h;

	UpdateTextureCoords(albumWidth, albumHeight, aspectRatio);

	// [16Jul2025] We now want to keep the un-scaled surface
	//             around as it might need rescaling when the
	//             DPI changes
	// 
//	if (lastSurface)
//		SDL_FreeSurface(lastSurface);

	albumLoaded = true;
}

bool AlbumArt::Load(const std::filesystem::path &fileName, const std::filesystem::path &parentPath, bool force) {
	auto extension = fileName.extension().u8string();
	std::transform(extension.begin(), extension.end(), extension.begin(), tolower);

	// Do we have cover art?
	if (IsSupported(extension))
		currentFile = fileName;
	else if (!parentPath.empty())
		currentFile = FindArt(parentPath);
	else
		currentFile = FindArt(fileName.parent_path());

	if (!currentFile.empty()) {
		auto contents = Fetcko::Utils::GetStringFromFile(currentFile);
		auto hash = hash_32_fnv1a_const(contents.c_str(), contents.size());
		if (hash == lastHash) {
			LogDebug("External art has already been loaded for this album");
			albumLoaded = true;
			albumWidth = lastWidth;
			albumHeight = lastHeight;
			std::unique_lock lock(histogramMutex);
			UpdateBin(true);
			return true;
		}

		lastHash = hash;

		auto utf8 = currentFile.u8string();
		auto surface = IMG_Load(utf8.c_str());

		if (!surface) {
			LogError("Could not load external album art from file " + utf8, " error: ", SDL_GetError());
			return false;
		} else if (!force && surface->w < albumWidth && surface->h < albumHeight) {
			LogWarning("External album art is smaller than what's already loaded");
			SDL_DestroySurface(surface);
			return false;
		} else if (albumWidth != 0 && albumHeight != 0) {
			LogDebug("External album art is larger than embedded. Using it instead.");
		}

		LoadFromSurface(surface);
	} else {
		LogWarning("Could not load external album art for " + fileName.u8string());
		return false;
	}

	return true;
}

void AlbumArt::ClearEmbedded() {
	delete[] embeddedData;
	embeddedData = nullptr;
	embeddedDataLength = 0;
	embeddedDataMimeType.clear();
}

bool AlbumArt::LoadEmbedded() {
	return Load(embeddedDataMimeType, embeddedData, embeddedDataLength, true);
}

bool AlbumArt::Load(const std::string &mimeType, const void *data, std::size_t length, bool force) {
	auto hash = hash_32_fnv1a_const(reinterpret_cast<const char *>(data), length);
	if (!force && hash == lastEmbeddedHash) {
		LogDebug("Embedded art has already been loaded for this album");
		albumLoaded = true;
		albumWidth = lastWidth;
		albumHeight = lastHeight;
		std::unique_lock lock(histogramMutex);
		UpdateBin(true);
		return true;
	} else if (hash != lastEmbeddedHash) {
		delete[] embeddedData;
		embeddedData = new uint8_t[length];
		memcpy(embeddedData, data, length);
		embeddedDataLength = length;

		lastEmbeddedHash = hash;
	}

	auto file = SDL_IOFromMem(
		const_cast<void*>(data),
		static_cast<int>(length)
	);
	embeddedDataMimeType = mimeType.substr(mimeType.find('/') + 1);

	auto surface = IMG_LoadTyped_IO(file, 1, embeddedDataMimeType.c_str());
	if (!surface) {
		LogError("Could not load embedded album art!");
		return false;
	} else if (surface->w < albumWidth && surface->h < albumHeight) {
		LogWarning("Embedded album art is smaller than what's already loaded");
		SDL_DestroySurface(surface);
		return false;
	}

	LoadFromSurface(surface);

	currentFile.clear();

	return true;
}

void AlbumArt::Reset(const Colour<float> &color) {
	albumLoaded = false;
	/*
	glDeleteTextures(1, &album);
	album = 0;
	*/
	lastWidth = albumWidth;
	lastHeight = albumHeight;

	albumWidth = 0;
	albumHeight = 0;
	averageColor = color;
	hidden = false;
}

void AlbumArt::NextBin(bool silent) {
	std::unique_lock lock(histogramMutex);

	if (!histogram.empty()) {
		/*
		while (++binIter != histogram.rend()) {
			bool found = true;
			for (const auto &bin : previousBins) {
				if (std::abs(binIter->second.h - bin->second.h) < 20.0f) {
					LogDebug("Skipping bin at hue ", bin->second.h);
					found = false;
					break;
				}
			}

			if (found)
				break;
		}

		if (binIter == histogram.rend()) {
		*/
		if (++binIter == histogram.rend()) {
			ResetBin(silent);
		} else {
			previousBins.emplace_back(binIter);
			UpdateBin(silent);
			if (!silent) PrintBin();
		}
	}
}
void AlbumArt::PreviousBin() {
	std::unique_lock lock(histogramMutex);

	if (!histogram.empty()) {
		previousBins.pop_back();
		if (previousBins.empty()) {
			ResetBin();
		} else {
			binIter = previousBins.back();

			UpdateBin();
			PrintBin();
		}
	}
}
void AlbumArt::ResetBin(bool silent) {
	if (!histogram.empty()) {
		previousBins.clear();
		binIter = histogram.rbegin();
		previousBins.emplace_back(binIter);

		UpdateBin(silent);
		if (!silent) PrintBin();
	}
}

void AlbumArt::UpdateBin(bool silent) {
	/*
	auto s = dominantBin->s;
	auto v = dominantBin->v;

	double maxChange = 0.0;
	constexpr double threshold = 0.5;
	if (s < threshold) {
		maxChange = threshold - dominantBin->s;
	}
	if (v < threshold) {
		maxChange = std::max(maxChange, threshold - dominantBin->v);
	}

	s += maxChange;
	v += maxChange;
	*/

	averageColor = Colour<float>::FromHsv(
		binIter->h,
		binIter->s,
		binIter->v
	);

	for (const auto &listener : colorChangeListeners)
		listener->OnColorChanged(averageColor, silent);

	if (HDR::Enabled) {
		averageColor.Tone(
			Settings::settings.GetAlbumArtGamma(),
			Settings::settings.GetAlbumArtContrast(),
			Settings::settings.GetAlbumArtBrightness(),
			HDR::WhiteLevel * HDR::Headroom
		);
	}
}

void AlbumArt::PrintBin() {
	LogDebug("Setting bin to hue ", binIter->h, ", saturation ", binIter->s, ", value ", binIter->v, " with count of ", binIter->count);
}

void AlbumArt::AddColorChangeListener(ColorChangeListener *listener) { 
	colorChangeListeners.emplace(listener); 
}

void AlbumArt::RemoveColorChangeListener(ColorChangeListener *listener) {
	colorChangeListeners.erase(listener);
}

const std::vector<Colour<float>> &AlbumArt::GetSelectedColors() const {
	return selectedColors;
}

void AlbumArt::Scale(bool force) {
	if (!lastSurface || (!lastSurfaceUpdated && !force)) return;

	lastSurfaceUpdated = false;

	{
		std::unique_lock lock(scalingMutex);
		scaling = false;
	}

	if (scaleThread.joinable())
		scaleThread.join();

	{
		std::unique_lock lock(scalingMutex);
		scaling = true;
	}

	scaleThread = std::thread([&] {
		// Wrap pixels in a new surface. This way, we can
		// free the surface without losing the original
		// pixel data.
		SDL_Surface *resized = SDL_CreateSurfaceFrom(
			lastSurface->w,
			lastSurface->h,
			lastSurface->format,
			lastSurface->pixels,
			lastSurface->pitch
		);

		auto start = std::chrono::system_clock::now();

		// TODO: RGBA support
		if (SDL_BITSPERPIXEL(lastSurface->format) == 24) {
			Gaussian gaussian;

			auto w = lastSurface->w / 2;
			SDL_Surface *next = nullptr;
			while (w > radius * 2 && scaling) {
				auto blurred = gaussian.Blur(resized, &scaling);

				resized = Bicubic::ResizeImage(blurred, static_cast<float>(w) / blurred->w, &scaling);

				w /= 2;
			}

			resized = Bicubic::ResizeImage(resized, (radius * 2) / resized->w, &scaling);

			// [16Jul2025] We now want to keep the un-scaled surface
			//             around as it might need rescaling when the
			//             DPI changes
//			if (resized)
//				lastSurface = nullptr;
		}

		auto end = std::chrono::system_clock::now();

		std::unique_lock lock(scalingMutex);
		if (scaling) {
			surfaceToLoad = resized;

			LogDebug("Image resizing took " + std::to_string(Duration<Microseconds>(end - start).AsSeconds()) + " seconds");
		}
	});
}