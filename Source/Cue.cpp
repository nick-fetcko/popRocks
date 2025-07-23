#include "Cue.hpp"

#include "Playlist.hpp"

std::optional<std::filesystem::path> Cue::OnLoad(const std::filesystem::path &path) {
	tracks.clear();
	currentTrack = tracks.end();

	std::ifstream inFile(path, std::ios::in);

	std::vector<std::vector<std::string>> lines;

	auto bom = Fetcko::Utils::GetBom(inFile);
	if (!bom || bom == Fetcko::Utils::BOM::UTF_8) {
		lines = ReadLines(inFile);
	} else {
		inFile.close();

		std::wifstream wInFile(path, std::ios::in | std::ios::binary);

		wInFile.imbue(
			std::locale(
				wInFile.getloc(),
				new std::codecvt_utf16<wchar_t, 0x10ffff, std::consume_header>
			)
		);

		lines = ReadLines(wInFile);
	}

	bool inFileSection = false;
	bool inTrackSection = false;

	Track track;
	for (const auto &line : lines) {
		if (line[0] == "FILE") {
			// tracks + .cue not supported
			if (!filePath.empty()) {
				CConsole::Console.Print("tracks + .cue not supported! Ignoring .cue file", MSG_ALERT);
				return std::nullopt;
			}

			filePath = path.parent_path() / line[1];

			// Some .cue files still point to the original .wav
			// and not the compressed version.
			const auto &supportedExtensions = Playlist::GetSupportedExtensions();
			auto iter = supportedExtensions.begin();
			while (!std::filesystem::exists(filePath) && iter != supportedExtensions.end())
				filePath.replace_extension(*(iter++));

			inFileSection = true;
		} else if (inFileSection) {
			if (inTrackSection) {
				if (line[0] == "TRACK") {
					// Finalize the previous track
					tracks.emplace_back(std::move(track));
					track = Track();
					track.index = static_cast<uint8_t>(std::stoi(line[1]));
				} else if (line[0] == "TITLE") {
					track.title = line[1];
				} else if (line[0] == "PERFORMER") {
					track.performer = line[1];
				} else if (line[0] == "INDEX") {
					// a representation of time in the form "m:s:f". 
					// 
					// "m" is minutes,
					// "s" is seconds,
					// and "f" is frames.
					// 
					// Fields may be zero padded
					auto split = Fetcko::Utils::Split(line[2], ':');

					// There are 75 frames per second of audio.
					// In the context of cue sheets, "frames" refer to CD sectors
					track.startTime = std::stoi(*split.rbegin()) / 75.0;

					for (auto iter = split.rbegin() + 1; iter != split.rend(); ++iter)
						track.startTime += (std::stoi(*iter) * std::max(1LL, (60 * (iter - split.rbegin() - 1))));
				}
			} else if (line[0] == "TRACK") {
				inTrackSection = true;
				track.index = static_cast<uint8_t>(std::stoi(line[1]));
			}
		} else if (line[0] == "TITLE") {
			title = line[1];
		} else if (line[0] == "PERFORMER") {
			performer = line[1];
		}
	}

	// If we hit EOF at the end of a track,
	// don't forget it!
	if (inFileSection && inTrackSection)
		tracks.emplace_back(std::move(track));

	currentTrack = tracks.begin();

	return filePath.empty() ? std::nullopt : std::optional<std::filesystem::path>(filePath);
}

const Cue::Track &Cue::Next() const {
	if (currentTrack == tracks.end() ||
		currentTrack + 1 == tracks.end())
		return *tracks.begin();

	return *(currentTrack + 1);
}

const Cue::Track &Cue::Next() {
	if (currentTrack == tracks.end())
		currentTrack = tracks.begin();
	else if (++currentTrack == tracks.end())
		currentTrack = tracks.begin();

	return *currentTrack;
}

const Cue::Track &Cue::Previous() {
	if (currentTrack == tracks.begin())
		currentTrack = tracks.end();

	return *(--currentTrack);
}

// This function also sets the current
// track to the track at that index
const Cue::Track &Cue::TrackAtIndex(std::size_t index) {
	currentTrack = tracks.begin() + index;
	return *currentTrack;
}

const Cue::Track &Cue::TrackAtOffset(std::size_t index) {
	currentTrack += index;
	return *currentTrack;
}

const Cue::Track &Cue::TrackAtTime(double time) {
	for (currentTrack = tracks.begin(); currentTrack != tracks.end(); ++currentTrack) {
		if (currentTrack->startTime > time)
			break;
	}

	return *(--currentTrack);
}

const double Cue::GetCurrentTrackEnd(double fileLength) const {
	if (currentTrack == tracks.end() || tracks.empty())
		return 0.0;

	if (auto next = currentTrack + 1; next != tracks.end())
		return next->startTime;
	else
		return fileLength;
}

const double Cue::GetCurrentTrackLength(double fileLength) const {
	return GetCurrentTrackEnd(fileLength) - currentTrack->startTime;
}