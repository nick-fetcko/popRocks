#include "Playlist.hpp"

#include "APE.hpp"
#include "FLAC.hpp"
#include "HDR.hpp"
#include "MP3.hpp"
#include "MP4.hpp"
#include "OGG.hpp"
#include "WV.hpp"

Playlist::Sorter::iterator Playlist::GuessDisc(Sorter &sorter, std::optional<std::size_t> &index) {
	std::size_t discGuess = 1;

	// If we already have a track with
	// the same index on another disc,
	// make a new one.
	if (index) {
		for (const auto &disc : sorter) {
			if (auto track = disc.second.find(*index); track != disc.second.end())
				++discGuess;
		}
	}

	auto discSorter = sorter.find(discGuess);

	if (discSorter == sorter.end()) {
		discSorter = sorter.emplace(
			std::make_pair(
				discGuess,
				std::map<std::size_t, std::pair<std::string, std::filesystem::path>>()
			)
		).first;
	}

	// If we don't have an index, make it the last
	// track on the last disc.
	if (!index)
		index = sorter.rbegin()->second.size() + 1;

	return discSorter;
}

std::optional<Playlist::Track> Playlist::OnLoad(
	const std::filesystem::path &path,
	const std::string_view &extension,
	std::function<HSTREAM(const std::filesystem::path &, const std::string &, DWORD)> openWithFlags
) {
	Clear();

	if (auto files = FindCue(path); !files.empty() || IsCue(extension)) {
		cue = std::make_unique<Cue>();

		bool loaded = true;
		if (!files.empty()) {
			for (const auto &[i, file] : Utils::Enumerate(files)) {
				loaded = loaded && cue->OnLoad(file, i != 0);
			}
		} else {
			loaded = cue->OnLoad(path).has_value();
		}

		if (loaded) {
			LoadTitles(cue->GetTracks());
			this->path = path;

			if (cue->GetTracks().empty())
				return Track{ cue->GetFilePath(), cue->GetFilePath().stem().u8string(), 0.0};
			else
				return Track{ cue->GetTracks().begin()->filePath, cue->GetTracks().begin()->title, cue->GetTracks().begin()->startTime };
		} else {
			cue.reset();
		}
	}

	Metadata metadata;

	class Loader : public TagLoader, public LoggableClass {
	public:
		void ClearTags() override {
			// Not needed here
		}

		void LoadFromTags(const std::map<std::string, std::string> &tags) override {
			if (auto title = tags.find("title"); title != tags.end())
				SetTitle(title->second);
			auto track = tags.find("tracknumber");
			// MP4 tags can use "track" instead of "tracknumber"
			if (track == tags.end()) track = tags.find("track");
			if (track != tags.end()) {
				try {
					index = std::stoll(Fetcko::Utils::Split(track->second, '/')[0]);
				}
				catch (std::exception &e) {
					LogWarning("Track number '", track->second, "' is not a number: ", e.what());
				}
			}
			auto disc = tags.find("discnumber");
			// MP4 tags can use "disc" instead of "discnumber"
			if (disc == tags.end()) disc = tags.find("disc");
			if (disc != tags.end()) {
				try {
					this->disc = std::stoll(Fetcko::Utils::Split(disc->second, '/')[0]);
				}
				catch (std::exception &e) {
					LogWarning("Disc number '", disc->second + "' is not a number: ", e.what());
				}
			}
		}
		bool AreThereEmptyTags() const override {
			return title.empty();
		}
		bool HasTitle() const override {
			return !title.empty();
		}

		void LoadFromID3v1(const TAG_ID3 *id3) override {
			if (!HasTitle() && id3->title[0] != '\0')
				title = std::string(id3->title, id3->title + 30);
		}

		void SetTitle(const std::string &title) override {
			this->title = title;
		}

		const std::string &GetTitle() const {
			return title;
		}

		const std::optional<std::size_t> &GetIndex() const {
			return index;
		}

		const std::optional<std::size_t> &GetDisc() const {
			return disc;
		}

	private:
		std::string title;
		std::optional<std::size_t> disc = std::nullopt;
		std::optional<std::size_t> index = std::nullopt;
	};

	std::vector<Title> titles;

	Sorter sorter;
	for (const auto &iter : std::filesystem::recursive_directory_iterator(path)) {
		auto extension = iter.path().extension().u8string();
		std::transform(extension.begin(), extension.end(), extension.begin(), tolower);

		if (auto filename = iter.path().filename().u8string();
			IsSupported(extension) &&
			// Ignore HFS attribute files (filenames that start with "._")
			(filename.size() <= 1 || filename[0] != '.' || filename[1] != '_')
		) {
			Loader loader;

			if (extension == ".flac") {
				FLAC flac(iter.path());

				loader.LoadFromTags(flac.GetTags());
			} else if (extension == ".mp3") {
				MP3 mp3(iter.path());

				loader.LoadFromTags(mp3.GetTags());
			} else if (extension == ".mp4" || extension == ".m4a") {
				MP4 mp4(iter.path());

				loader.LoadFromTags(mp4.GetTags());
			} else if (extension == ".ape" || extension == ".tta" /* TTA files can use APE tags */) {
				APE ape(iter.path());

				loader.LoadFromTags(ape.GetTags());

				// TTA files can use ID3 tags, too
				if (extension == ".tta" && loader.AreThereEmptyTags()) {
					MP3 tta(iter.path());

					loader.LoadFromTags(tta.GetTags());
				}
			} else if (extension == ".wv") {
				WV wv(iter.path());

				loader.LoadFromTags(wv.GetTags());
			} else if (extension == ".ogg") {
				OGG ogg(iter.path());

				loader.LoadFromTags(ogg.GetTags());
			} else {
				auto streamHandle = openWithFlags(iter.path(), extension, 0);
				metadata.OnLoad(iter.path(), extension, streamHandle, &loader);
				BASS_StreamFree(streamHandle);
			}

			// If title is STILL empty, use the filename
			if (!loader.HasTitle())
				loader.SetTitle(iter.path().stem().u8string());

			Sorter::iterator discSorter = sorter.end();

			// We _do_ want to make a copy here,
			// since we may need to modify it
			auto index = loader.GetIndex();
			if (index) {
				if (auto &disc = loader.GetDisc()) {
					discSorter = sorter.find(*disc);

					if (discSorter == sorter.end()) {
						discSorter = sorter.emplace(
							std::make_pair(
								*disc,
								std::map<std::size_t, std::pair<std::string, std::filesystem::path>>()
							)
						).first;
					}
				} else discSorter = GuessDisc(sorter, index);
			} else {
				if (auto [success, number] = Utils::ExtractDigitsFromString(iter.path().stem().u8string(), true); success)
					index = number;

				discSorter = GuessDisc(sorter, index);
			}

			discSorter->second.emplace(
				std::make_pair(
					*index,
					std::make_pair(
						loader.GetTitle(),
						iter.path()
					)
				)
			);
		}
	}

	for (auto &&[number, disc] : sorter) {
		for (auto &&track : disc) {
			titles.emplace_back(Title{ number, track.first, std::move(track.second.first) });
			files.emplace_back(std::move(track.second.second));
		}
	}

	LoadTitles(titles);

	currentFile = files.end();

	if (auto next = Next()) {
		this->path = path;
		return next;
	}

	return std::nullopt;
}

inline void Playlist::UpdateSize() {
	height = size.y + font->GetEm().height / 2;
	std::vector<float> rect = {
		// Top half
		-font->GetEm().width / 2.0f,
		-font->GetEm().height / 2.0f,
		0.0f,
		0.0f,
		0.0f,
		0.75f,
		-font->GetEm().width / 2.0f,
		height - font->GetEm().height,
		0.0f,
		0.0f,
		0.0f,
		0.0f,
		size.x + font->GetEm().width / 2.0f,
		height - font->GetEm().height,
		0.0f,
		0.0f,
		0.0f,
		0.0f,
		size.x + font->GetEm().width / 2.0f,
		-font->GetEm().height / 2.0f,
		0.0f,
		0.0f,
		0.0f,
		0.75f,
		// Bottom half
		-font->GetEm().width / 2.0f,
		height - font->GetEm().height,
		0.0f,
		0.0f,
		0.0f,
		0.0f,
		-font->GetEm().width / 2.0f,
		height,
		0.0f,
		0.0f,
		0.0f,
		0.0f,
		size.x + font->GetEm().width / 2.0f,
		height,
		0.0f,
		0.0f,
		0.0f,
		0.0f,
		size.x + font->GetEm().width / 2.0f,
		height -font->GetEm().height,
		0.0f,
		0.0f,
		0.0f,
		0.0f
	};

	vbo->Bind();
	vbo->BufferData(rect, GL_DYNAMIC_DRAW);
	vbo->Unbind();
}

void Playlist::OnInit(int windowWidth, int windowHeight, OpenGLFont *font, OpenGLFont *outlineFont, Context *context, float scale) {
	this->windowWidth = windowWidth;
	this->windowHeight = windowHeight;
	this->font = font;
	this->outlineFont = outlineFont;
	this->context = context;
	this->scale = scale;

	outline.OnInit(outlineFont, context);

	vao = std::make_unique<VertexArray>();
	vbo = std::make_unique<ArrayBuffer>();
	eab = std::make_unique<ElementBuffer>();

	vao->Bind();
	vbo->Bind();
	vao->AddAttribute(VertexArray::Attribute(0, 2, 6 * sizeof(float)));
	vao->AddAttribute(VertexArray::Attribute(1, 4, 6 * sizeof(float), 2 * sizeof(float)));
	vbo->Unbind();
	vao->Unbind();

	eab->Bind();
	auto squareBuffer = std::vector(Buffers::SquareBuffer.begin(), Buffers::SquareBuffer.end());
	//std::vector<unsigned short> squareBuffer;
	for (std::size_t i = 0; i < Buffers::SquareBuffer.size(); ++i)
		squareBuffer.emplace_back(Buffers::SquareBuffer[i] + 4);
	eab->BufferData(squareBuffer);
	eab->Unbind();
}

void Playlist::OnResize(int windowWidth, int windowHeight, float scale) {
	this->windowWidth = windowWidth;
	this->windowHeight = windowHeight;
	if (this->scale != scale && !titles.empty()) {
		size = { 0, 0 };
		for (auto &title : titles) {
			title.OnInit(font, context);

			size.y += title.GetBounds().height;
			if (title.GetSize().x > size.x)
				size.x = title.GetSize().x;
		}
		outline.OnInit(outlineFont, context);

		this->scale = scale;
	}
	UpdateSize();
}

void Playlist::OnDestroy() {
	vao.reset();
	vbo.reset();
	eab.reset();
}

void Playlist::Clear() {
	path.clear();
	files.clear();
	currentFile = files.end();

	for (auto &title : titles)
		title.OnDestroy();

	titles.clear();

	cue.reset();
}

std::optional<Playlist::Track> Playlist::Current() {
	if (currentFile != files.end())
		return Track{ *currentFile };

	return std::nullopt;
}

std::optional<Playlist::Track> Playlist::Previous() {
	if (files.empty()) {
		if (cue) {
			const auto &previous = cue->Previous();

			return Track{ previous.filePath, previous.title, previous.startTime };
		}

		return std::nullopt;
	}

	if (currentFile == files.begin())
		currentFile = files.end();

	return Track{ *(--currentFile) };
}

std::optional<Playlist::Track> Playlist::Next() {
	if (files.empty()) {
		if (cue) {
			const auto &next = cue->Next();

			return Track{ next.filePath, next.title, next.startTime };
		}

		return std::nullopt;
	}

	if (currentFile == files.end())
		currentFile = files.begin();
	else if (++currentFile == files.end())
		currentFile = files.begin();

	return Track{ *currentFile };
}

const std::optional<Playlist::Track> Playlist::GetNext() const {
	if (files.empty()) {
		if (cue) {
			const auto &next = const_cast<const Cue *>(cue.get())->Next();

			return Track{ next.filePath, next.title, next.startTime };
		}
	}

	if (currentFile == files.end() || currentFile + 1 == files.end())
		return std::nullopt;

	return Track{ *(currentFile + 1) };
}

void Playlist::OnLoop(Vector2i pos, float maxHeight, float alpha, Context &context) {
	// We want to store its _origin_
	this->pos = pos;

	if (!files.empty())
		OnLoop(files, currentFile, pos, maxHeight, alpha, context);
	else if (cue)
		OnLoop(cue->GetTracks(), cue->GetCurrentTrack(), pos, maxHeight, alpha, context);
}

std::optional<Playlist::Track> Playlist::OnMouseClicked(const Vector2i &mousePos) {
	if (!titles.empty() && mousePos.y >= pos.y && mousePos.y <= maxHeight) {
		const auto offset = 
			cue ?
				std::distance(cue->GetTracks().begin(), cue->GetCurrentTrack()) :
				std::distance(files.begin(), currentFile);

		const auto distance = 
			cue ?
				std::distance(cue->GetCurrentTrack(), cue->GetTracks().end()) :
				std::distance(currentFile, files.end());

		auto &bounds = titles.begin()->GetBounds();

		// If our offset is not 0, we need to account for the previous track
		if (auto index = (mousePos.y - pos.y + bounds.y / 2) / bounds.height - (offset != 0 ? 1 : 0);
			index < distance) {
			if (mousePos.x >= pos.x && mousePos.x <= pos.x + titles[offset + index].GetSize().x) {
				if (!files.empty()) {
					currentFile += index;
					return currentFile == files.end() ? Track{ *(--currentFile) } : Track{ *currentFile };
				} else if (cue) {
					const auto &track = cue->TrackAtOffset(index);

					return Track{ track.filePath, track.title, track.startTime };
				} else return std::nullopt;
			}
		}
	}

	return std::nullopt;
}

const std::unique_ptr<Cue> &Playlist::GetCue() const { return cue; }
const std::filesystem::path &Playlist::GetPath() const { return path; }

std::vector<std::filesystem::path> Playlist::FindCue(const std::filesystem::path &path) {
	std::vector<std::filesystem::path> ret;

	if (!std::filesystem::is_directory(path))
		return ret;

	for (const auto &iter : std::filesystem::recursive_directory_iterator(path)) {
		auto extension = iter.path().extension().u8string();
		std::transform(extension.begin(), extension.end(), extension.begin(), tolower);
		if (IsCue(extension))
			ret.emplace_back(iter.path());
	}

	return ret;
}