#include "Playlist.hpp"

Playlist::Sorter::iterator Playlist::GuessDisc(Sorter &sorter, std::size_t index) {
	std::size_t discGuess = 1;

	// If we already have a track with
	// the same index on another disc,
	// make a new one.
	for (const auto &disc : sorter) {
		if (auto track = disc.second.find(index); track != disc.second.end())
			++discGuess;
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

	return discSorter;
}

std::optional<Playlist::Track> Playlist::OnLoad(
	const std::filesystem::path &path,
	const std::string_view &extension,
	std::function<HSTREAM(const std::filesystem::path &, const std::string &, DWORD)> openWithFlags
) {
	Clear();

	if (auto file = FindCue(path); file || IsCue(extension)) {
		cue = std::make_unique<Cue>();

		if (cue->OnLoad(file ? *file : path)) {
			LoadTitles(cue->GetTracks());
			this->path = path;

			return Track{ cue->GetFilePath(), cue->GetTracks().empty() ? 0.0 : cue->GetTracks().begin()->startTime };
		} else {
			cue.reset();
		}
	}

	Metadata metadata;

	class Loader : public TagLoader {
	public:
		void LoadFromTags(const std::map<std::string, std::string> &tags) override {
			if (auto title = tags.find("title"); title != tags.end())
				SetTitle(title->second);
			if (auto track = tags.find("tracknumber"); track != tags.end()) {
				try {
					index = std::stoll(track->second);
				}
				catch (std::exception &e) {
					CConsole::Console.Print("Track number '" + track->second + "' is not a number: " + e.what(), MSG_ALERT);
				}
			}
			if (auto disc = tags.find("discnumber"); disc != tags.end()) {
				try {
					this->disc = std::stoll(disc->second);
				}
				catch (std::exception &e) {
					CConsole::Console.Print("Disc number '" + disc->second + "' is not a number: " + e.what(), MSG_ALERT);
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
			auto streamHandle = openWithFlags(iter.path(), extension, BASS_STREAM_PRESCAN);

			Loader loader;
			metadata.OnLoad(iter.path(), extension, streamHandle, &loader);

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
				} else discSorter = GuessDisc(sorter, *index);
			} else {
				index = titles.size() + 1;
				discSorter = GuessDisc(sorter, *index);
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

			BASS_StreamFree(streamHandle);
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
	em = this->titles.begin()->MeasureText("M");

	rect[0] = -em.x / 2;
	rect[1] = -em.y / 2;
	rect[2] = -em.x / 2;
	rect[3] = size.y + em.y / 2;
	rect[4] = size.x + em.x / 2;
	rect[5] = size.y + em.y / 2;
	rect[6] = size.x + em.x / 2;
	rect[7] = -em.x / 2;
}

void Playlist::OnInit(int windowWidth, int windowHeight, TTF_Font *font, float scale) {
	this->windowWidth = windowWidth;
	this->windowHeight = windowHeight;
	this->font = font;
	this->scale = scale;
}

void Playlist::OnResize(int windowWidth, int windowHeight, float scale) {
	this->windowWidth = windowWidth;
	this->windowHeight = windowHeight;
	if (this->scale != scale && !titles.empty()) {
		size = { 0, 0 };
		for (auto &title : titles) {
			title.OnInit(font);

			size.y += title.GetSize().y;
			if (title.GetSize().x > size.x)
				size.x = title.GetSize().x;
		}

		UpdateSize();
	}
	this->scale = scale;
}

void Playlist::Clear() {
	path.clear();
	files.clear();

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
		if (cue)
			return Track{ cue->GetFilePath(), cue->Previous().startTime };

		return std::nullopt;
	}

	if (currentFile == files.begin())
		currentFile = files.end();

	return Track{ *(--currentFile) };
}

std::optional<Playlist::Track> Playlist::Next() {
	if (files.empty()) {
		if (cue)
			return Track{ cue->GetFilePath(), cue->Next().startTime };

		return std::nullopt;
	}

	if (currentFile == files.end())
		currentFile = files.begin();
	else if (++currentFile == files.end())
		currentFile = files.begin();

	return Track{ *currentFile };
}

const std::optional<Playlist::Track> Playlist::Next() const {
	if (files.empty()) {
		if (cue)
			return Track{ cue->GetFilePath(), const_cast<const Cue *>(cue.get())->Next().startTime };
	}

	if (currentFile == files.end() || currentFile + 1 == files.end())
		return std::nullopt;

	return Track{ *(currentFile + 1) };
}

void Playlist::OnLoop(Vector2i pos, float maxHeight, float alpha) {
	// We want to store its _origin_
	this->pos = pos;

	if (!files.empty())
		OnLoop(files, currentFile, pos, maxHeight, alpha);
	else if (cue)
		OnLoop(cue->GetTracks(), cue->GetCurrentTrack(), pos, maxHeight, alpha);
}

std::optional<Playlist::Track> Playlist::OnMouseClicked(const Vector2i &mousePos) {
	if (!titles.empty() && mousePos.y >= pos.y) {
		const auto offset = 
			cue ?
				std::distance(cue->GetTracks().begin(), cue->GetCurrentTrack()) :
				std::distance(files.begin(), currentFile);

		const auto distance = 
			cue ?
				std::distance(cue->GetCurrentTrack(), cue->GetTracks().end()) :
				std::distance(currentFile, files.end());

		if (auto index = (mousePos.y - pos.y) / titles.begin()->GetSize().y; index < distance) {
			if (mousePos.x >= pos.x && mousePos.x <= pos.x + titles[offset + index].GetSize().x) {
				if (!files.empty()) {
					currentFile += index;
					return currentFile == files.end() ? Track{ *(--currentFile) } : Track{ *currentFile };
				} else if (cue) {
					return Track{ cue->GetFilePath(), cue->TrackAtOffset(index).startTime };
				} else return std::nullopt;
			}
		}
	}

	return std::nullopt;
}

const std::unique_ptr<Cue> &Playlist::GetCue() const { return cue; }
const std::filesystem::path &Playlist::GetPath() const { return path; }

std::optional<std::filesystem::path> Playlist::FindCue(const std::filesystem::path &path) {
	if (!std::filesystem::is_directory(path))
		return std::nullopt;

	for (const auto &iter : std::filesystem::directory_iterator(path)) {
		auto extension = iter.path().extension().u8string();
		std::transform(extension.begin(), extension.end(), extension.begin(), tolower);
		if (IsCue(extension))
			return iter.path();
	}

	return std::nullopt;
}