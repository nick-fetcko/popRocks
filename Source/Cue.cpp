#include "Cue.hpp"

#include "Utils/ShiftJIS.hpp"
#include "Utils/Windows1252.hpp"

#include "Playlist.hpp"

inline std::string Cue::Parse(const std::string &string) {
	if (encoding) {
		if (*encoding == Utils::Encoding::ShiftJis)
			return ShiftJIS::ToUtf8(string);
		else if (*encoding == Utils::Encoding::Windows1252)
			return Utils::ToUTF8(Windows1252::ToUtf16(string));
	}

	return string;
}

std::optional<std::filesystem::path> Cue::OnLoad(const std::filesystem::path &path, bool append) {
	if (!append) {
		tracks.clear();
		files.clear();
		currentTrack = tracks.end();
	} else {
		filePath.clear();
		++discIndex;
	}

	std::ifstream inFile(path, std::ios::in);

	std::vector<std::vector<std::string>> lines;

	auto bom = Fetcko::Utils::GetBom(inFile);
	if (!bom || bom == Fetcko::Utils::BOM::UTF_8) {
		std::filebuf fileBuf;
		fileBuf.open(path, std::ios::in);
		lines = ReadLines(fileBuf);
	} else {
		inFile.close();

		std::basic_filebuf<char16_t> fileBuf;
		fileBuf.open(path, std::ios::in | std::ios::binary);

		fileBuf.pubimbue(
			std::locale(
				fileBuf.getloc(),
				new std::codecvt_utf16<char16_t, 0x10ffff, std::consume_header>
			)
		);

		lines = ReadLines(fileBuf);
	}

	if (!bom || bom != Fetcko::Utils::BOM::UTF_8) {
		std::string combined;
		for (const auto &line : lines) {
			for (const auto &word : line)
				combined += word;
		}
		encoding = Utils::GuessEncoding(combined, 1);
	} else encoding = std::nullopt;
	

	bool inFileSection = false;
	bool inTrackSection = false;

	Track track;
	int firstTrackIndex = 0;
	track.disc = discIndex;
	for (const auto &line : lines) {
		if (line[0] == "FILE") {
			// Finalize the previous track,
			// if there was one, and start
			// a new disc
			if (inFileSection && inTrackSection) {
				firstTrackIndex = track.index;
				tracks.emplace_back(std::move(track));

				track = Track();
				track.disc = ++discIndex;
				track.index = 1;
			}

			// tracks + .cue not supported
			if (!filePath.empty() && tracks.size() == 1) {
				LogWarning("tracks + .cue not supported! Ignoring .cue file and loading individual songs.");
				return std::nullopt;
			}

			std::filesystem::path originalFilePath;
			try {
				originalFilePath = filePath = 
					path.parent_path() / 
#ifdef WIN32
						Utils::ToUTF16(
#endif
							line[1]
#ifdef WIN32
						)
#endif
					;
			}
			catch (const std::exception &e) {
				// As these are single lines, lower threshold to a single match
				auto encoding = Utils::GuessEncoding(line[1], 1);

				if (encoding == Utils::Encoding::Windows1252) {
					originalFilePath = filePath = 
						path.parent_path() / 
#ifndef WIN32
							Utils::ToUTF8(
#endif
								Windows1252::ToUtf16(line[1])
#ifndef WIN32
							)
#endif
						;
				} else if (encoding == Utils::Encoding::ShiftJis) {
					originalFilePath = filePath = 
						path.parent_path() / 
#ifdef WIN32
							Utils::ToUTF16(
#endif
								ShiftJIS::ToUtf8(line[1])
#ifdef WIN32
							)
#endif
						;
				} else if (encoding == Utils::Encoding::Ascii) // Should be treated as UTF-8, but just in case
					originalFilePath = filePath = path.parent_path() / line[1];
			}
			
			// Some .cue files still point to the original .wav
			// and not the compressed version.
			const auto &supportedExtensions = Playlist::GetSupportedExtensions();
			auto iter = supportedExtensions.begin();
			while (!std::filesystem::exists(filePath) && iter != supportedExtensions.end())
				filePath.replace_extension(*(iter++));

			track.filePath = filePath;

			// If we still can't find the audio file,
			// assume the .cue is old / malformed / unneeded
			//
			// Example: A .cue was split into multiple tracks,
			//          but the original, unsplit cue still
			//          exists in the folder.
			if (!std::filesystem::exists(filePath)) {
				LogWarning("Could not find audio file ", originalFilePath, " referenced in .cue file! Ignoring...");
				return std::nullopt;
			}

			// If we've already loaded a .cue for
			// this file, assume we're a duplicate
			if (auto iter = files.find(filePath); iter != files.end()) {
				LogWarning("Found multiple .cue files referring to \"", filePath, "\"! Assuming one is a duplicate and ignoring...");
				return filePath;
			}

			files.emplace(filePath);

			inFileSection = true;
		} else if (inFileSection) {
			if (inTrackSection) {
				if (line[0] == "TRACK") {
					// Finalize the previous track
					if (!track.title.empty()) {
						tracks.emplace_back(std::move(track));
						track = Track();
						track.filePath = filePath;
						track.disc = discIndex;
						track.index = static_cast<uint8_t>(std::stoi(line[1]) - firstTrackIndex);
					}
				} else if (line[0] == "TITLE") {
					track.title = Parse(line[1]);
				} else if (line[0] == "PERFORMER") {
					track.performer = Parse(line[1]);
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
						track.startTime += (std::stoi(*iter) * std::max(1LL, (60LL * (iter - split.rbegin() - 1LL))));
				}
			} else if (line[0] == "TRACK") {
				inTrackSection = true;
				track.index = static_cast<uint8_t>(std::stoi(line[1]));
			}
		} else if (line[0] == "TITLE") {
			title = Parse(line[1]);
		} else if (line[0] == "PERFORMER") {
			performer = Parse(line[1]);
		}
	}

	// If we hit EOF at the end of a track,
	// don't forget it!
	if (inFileSection && inTrackSection)
		tracks.emplace_back(std::move(track));

	currentTrack = tracks.begin();

	return tracks.empty() ? std::nullopt : std::optional<std::filesystem::path>(tracks.begin()->filePath);
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

	if (auto next = currentTrack + 1; next != tracks.end() && next->filePath == currentTrack->filePath)
		return next->startTime;
	else
		return fileLength;
}

const double Cue::GetCurrentTrackLength(double fileLength) const {
	return GetCurrentTrackEnd(fileLength) - currentTrack->startTime;
}