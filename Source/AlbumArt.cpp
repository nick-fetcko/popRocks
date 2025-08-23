#include "AlbumArt.hpp"

#include <map>
#include <algorithm>
#include <thread>

#include "MathCPP/Duration.hpp"

#include "Bicubic.hpp"
#include "Buffer.hpp"
#include "Gaussian.hpp"
#include "Hash.hpp"
#include "Settings.hpp"
#include "Utils.hpp"

using namespace MathsCPP;

AlbumArt::AlbumArt(std::unique_ptr<Context> &context) : context(context) {
	radius = Settings::settings.GetRadius();
}

void AlbumArt::OnInit(int windowWidth, int windowHeight, float scale) {
	this->scale = scale;
	radius *= scale;

	if (context) {
		context->With("rotate"_hash, [this](Context::Shader &shader) {
			shader.program.Uniform1f("radius", radius);
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
}

void AlbumArt::OnResize(int windowWidth, int windowHeight, float scale) {
	if (this->scale != scale) {
		// FIXME: will floating point precision errors accumulate here?
		radius /= this->scale;
		this->scale = scale;
		radius *= scale;

		logger.LogDebug("Scale changed! New radius is ", radius);

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
			shader.program.Uniform1f("radius", radius);
		});
	}
}

void AlbumArt::OnLoop(GLfloat x, GLfloat y, float frameCount, Context &context) {
	// try_lock so we don't miss a frame or two
	if (mutex.try_lock()) {
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
		mutex.unlock();
	}

	if (albumLoaded) {
		context.Color(1.0f, 1.0f, 1.0f, 1.0f);

		context.Translate(
			x,
			y,
			0
		);
		context.Rotate(
			360.0f + frameCount,
			0.0f,
			0.0f,
			1.0f
		);
		context.Apply();

		glBindTexture(GL_TEXTURE_2D, album);

		Circle::OnLoop(0, 0, context);

		context.LoadIdentity();
	}
}

int AlbumArt::DrawSquare(int x, int y, int height, GLfloat alpha, Context &context) {
	if (albumLoaded) {
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
		
		context.Color(1.0f, 1.0f, 1.0f, alpha);

		context.Translate(
			static_cast<GLfloat>(x),
			static_cast<GLfloat>(y),
			0.0f
		);
		context.Apply();
		glBindTexture(GL_TEXTURE_2D, album);

		squareVao->Bind();
		squareEab->Bind();
		squareEab->DrawElements(GL_TRIANGLES);
		squareEab->Unbind();
		squareVao->Unbind();

		context.LoadIdentity();

		// Return our width
		return static_cast<int>(squareWidth);
	}

	return 0;
}

void AlbumArt::OnDestroy() {
	Circle::OnDestroy();

	if (lastSurface)
		SDL_FreeSurface(lastSurface);

	glDeleteTextures(1, &album);
	album = 0;

	squareVao.reset();
	squareVbo.reset();
	squareEab.reset();
}

std::filesystem::path AlbumArt::FindArt(const std::filesystem::path &folder) const {
	std::filesystem::path found;
	bool foundPreferred = false;

	auto find = [&](const std::filesystem::directory_entry &entry, bool breakOnFind = false) {
		auto extension = entry.path().extension().u8string();
		std::transform(extension.begin(), extension.end(), extension.begin(), tolower);
		if (IsSupported(extension)) {
			if (found.empty())
				found = entry.path();

			auto filename = entry.path().filename().u8string();
			std::transform(filename.begin(), filename.end(), filename.begin(), tolower);

			if ((filename.find("cover") == 0 ||
				filename.find("front") != std::string::npos ||
				filename.find("folder") == 0) &&
				!foundPreferred) {
				found = entry.path();
				foundPreferred = true;

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
			}
		}

		return false;
	};

	// Try to find in our current folder first
	for (const auto &iter : std::filesystem::directory_iterator(folder))
		find(iter);

	// Then try any subfolders
	// FIXME: we don't need to re-check files in the current folder
	if (found.empty() && (albumWidth == 0 || albumHeight == 0) /* only scan subfolders if we don't already have embedded art */) {
		for (const auto &iter : std::filesystem::recursive_directory_iterator(folder))
			if (find(iter, true)) break;
	}

	return found;
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

	uint8_t *pixels = nullptr;
	if (surface->pitch == surface->w * surface->format->BytesPerPixel) {
		pixels = reinterpret_cast<uint8_t *>(surface->pixels);
	} else {
		pixels = new uint8_t[surface->w * surface->h * surface->format->BytesPerPixel];
		for (int y = 0; y < surface->h; ++y) {
			memcpy(
				&pixels[y * surface->w * surface->format->BytesPerPixel],
				&(reinterpret_cast<uint8_t*>(surface->pixels))[y * surface->pitch],
				surface->w * surface->format->BytesPerPixel
			);
		}
	}

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, surface->format->BitsPerPixel == 32 ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, pixels);

	if (!scaled) {
		// lastSurface is the last surface
		// _before_ scaling, so only update
		// it when we aren't scaling
		if (lastSurface)
			SDL_FreeSurface(lastSurface);

		lastSurface = surface;
		lastSurfaceUpdated = true;

		if (colorMethod == ColorMethod::Average) {
			// Go through every pixel to find an "average" color
			uint64_t averageR = 0;
			uint64_t averageG = 0;
			uint64_t averageB = 0;
			for (auto x = 0; x < surface->w; ++x) {
				for (auto y = 0; y < surface->h; ++y) {
					auto pos = y * surface->format->BytesPerPixel * surface->w + x * surface->format->BytesPerPixel;

					averageR += pixels[pos];
					averageG += pixels[pos + 1];
					averageB += pixels[pos + 2];
				}
			}

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
				for (auto x = 0; x < surface->w; x += hstep) {
					for (auto y = 0; y < surface->h; y += vstep) {
						auto index = y * surface->w + x * surface->format->BytesPerPixel;

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

			this->histogram.clear();

			std::size_t maxCount = std::numeric_limits<std::size_t>::min();
			for (const auto &[hue, bin] : histogram) {
				if (bin.count > maxCount)
					maxCount = bin.count;
			}

			const auto minPercentage = static_cast<std::size_t>(
				maxCount * Settings::settings.GetColorSelection().minPercentage
			);

			for (auto &[hue, bin] : histogram) {
				// Filter out anything < a percentage of our max
				if (bin.count >= minPercentage) {
					this->histogram.emplace(
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
					logger.LogDebug(
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
			auto tempHistogram = this->histogram;
			this->histogram.clear();
			for (auto iter = tempHistogram.rbegin(); iter != tempHistogram.rend(); ++iter) {
				if (this->histogram.empty()) {
					this->histogram.emplace(*iter);
				} else {
					bool found = true;
					for (auto compare = this->histogram.begin(); compare != this->histogram.end(); ++compare) {
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
						this->histogram.emplace(*iter);
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
			if (this->histogram.size() == 1) {
				auto deeperColor = *this->histogram.begin();

				if (deeperColor.v >= 0.5)
					deeperColor.v = std::clamp(deeperColor.v / 1.75f, 0.0f, 1.0f);
				else
					deeperColor.v = std::clamp(deeperColor.v * 1.75f, 0.0f, 1.0f);

				deeperColor.count -= 1;

				this->histogram.emplace(std::move(deeperColor));
			}

			ResetBin();
		}
	}

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

	std::filesystem::path found;

	// Do we have cover art?
	if (IsSupported(extension))
		found = fileName;
	else if (!parentPath.empty())
		found = FindArt(parentPath);
	else
		found = FindArt(fileName.parent_path());

	if (!found.empty()) {
		auto contents = Fetcko::Utils::GetStringFromFile(found);
		auto hash = hash_32_fnv1a_const(contents.c_str(), contents.size());
		if (hash == lastHash) {
			logger.LogDebug("External art has already been loaded for this album");
			albumLoaded = true;
			albumWidth = lastWidth;
			albumHeight = lastHeight;
			UpdateBin(true);
			return true;
		}

		lastHash = hash;

		auto utf8 = found.u8string();
		auto surface = IMG_Load(utf8.c_str());

		if (!surface) {
			logger.LogError("Could not load external album art from file " + utf8);
			return false;
		} else if (!force && surface->w < albumWidth && surface->h < albumHeight) {
			logger.LogWarning("External album art is smaller than what's already loaded");
			SDL_FreeSurface(surface);
			return false;
		} else if (albumWidth != 0 && albumHeight != 0) {
			logger.LogDebug("External album art is larger than embedded. Using it instead.");
		}

		LoadFromSurface(surface);
	} else {
		logger.LogWarning("Could not load external album art for " + fileName.u8string());
		return false;
	}

	return true;
}

bool AlbumArt::Load(const std::string &mimeType, const void *data, std::size_t length) {
	auto hash = hash_32_fnv1a_const(reinterpret_cast<const char *>(data), length);
	if (hash == lastEmbeddedHash) {
		logger.LogDebug("Embedded art has already been loaded for this album");
		albumLoaded = true;
		albumWidth = lastWidth;
		albumHeight = lastHeight;
		UpdateBin(true);
		return true;
	}

	lastEmbeddedHash = hash;

	auto file = SDL_RWFromMem(
		const_cast<void*>(data),
		static_cast<int>(length)
	);
	auto type = mimeType.substr(mimeType.find('/') + 1);

	auto surface = IMG_LoadTyped_RW(file, 1, type.c_str());
	if (!surface) {
		logger.LogError("Could not load embedded album art!");
		return false;
	} else if (surface->w < albumWidth && surface->h < albumHeight) {
		logger.LogWarning("Embedded album art is smaller than what's already loaded");
		SDL_FreeSurface(surface);
		return false;
	}

	LoadFromSurface(surface);

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
}

void AlbumArt::NextBin(bool silent) {
	if (!histogram.empty()) {
		/*
		while (++binIter != histogram.rend()) {
			bool found = true;
			for (const auto &bin : previousBins) {
				if (std::abs(binIter->second.h - bin->second.h) < 20.0f) {
					logger.LogDebug("Skipping bin at hue ", bin->second.h);
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
}

void AlbumArt::PrintBin() {
	logger.LogDebug("Setting bin to hue ", binIter->h, ", saturation ", binIter->s, ", value ", binIter->v, " with count of ", binIter->count);
}

void AlbumArt::AddColorChangeListener(ColorChangeListener *listener) { 
	colorChangeListeners.emplace(listener); 
}

void AlbumArt::RemoveColorChangeListener(ColorChangeListener *listener) {
	colorChangeListeners.erase(listener);
}

void AlbumArt::Scale(bool force) {
	if ((!lastSurface || !lastSurfaceUpdated) && !force) return;

	lastSurfaceUpdated = false;

	std::thread([&] {
		// Wrap pixels in a new surface. This way, we can
		// free the surface without losing the original
		// pixel data.
		SDL_Surface *resized = SDL_CreateRGBSurfaceWithFormatFrom(
			lastSurface->pixels,
			lastSurface->w,
			lastSurface->h,
			lastSurface->format->BytesPerPixel,
			lastSurface->pitch,
			lastSurface->format->format
		);

		auto start = std::chrono::system_clock::now();

		// TODO: RGBA support
		if (lastSurface->format->BitsPerPixel == 24) {
			Gaussian gaussian;

			auto w = lastSurface->w / 2;
			SDL_Surface *next = nullptr;
			while (w > radius * 2) {
				auto blurred = gaussian.Blur(resized);

				resized = Bicubic::ResizeImage(blurred, static_cast<float>(w) / blurred->w);

				w /= 2;
			}

			resized = Bicubic::ResizeImage(resized, (radius * 2) / resized->w);

			// [16Jul2025] We now want to keep the un-scaled surface
			//             around as it might need rescaling when the
			//             DPI changes
//			if (resized)
//				lastSurface = nullptr;
		}

		auto end = std::chrono::system_clock::now();

		std::unique_lock lock(mutex);
		surfaceToLoad = resized;

		logger.LogDebug("Image resizing took " + std::to_string(Duration<Microseconds>(end - start).AsSeconds()) + " seconds");
	}).detach();
}