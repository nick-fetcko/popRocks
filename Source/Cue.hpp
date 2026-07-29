#pragma once

#include <codecvt>
#include <filesystem>
#include <fstream>
#include <locale>
#include <set>
#include <string>

#include "Utils/Logger.hpp"
#include "Utils/Utils.hpp"

using namespace Fetcko;

// https://wyday.com/cuesharp/specification.php
// https://en.wikipedia.org/wiki/Cue_sheet_%28computing%29

class Cue : public LoggableClass {
private:
	struct Track {
		std::string title;
		std::string performer;

		// TODO: Support for multi-disc cue sheets
		uint8_t disc = 1;

		// It's 0-99, so 1 byte is plenty
		uint8_t index = 0;

		double startTime = 0.0;

		std::filesystem::path filePath;

		friend std::ostream &operator<<(std::ostream &left, const Track &right) {
			left << right.title;
			return left;
		}
	};

	std::vector<Track> tracks;
	std::set<std::filesystem::path> files;
	std::vector<Track>::const_iterator currentTrack = tracks.end();

public:
	std::optional<std::filesystem::path> OnLoad(const std::filesystem::path &path, bool append = false);

	const Track &Next() const;
	const Track &Next();

	const Track &Previous();

	// This function also sets the current
	// track to the track at that index
	const Track &TrackAtIndex(std::size_t index);

	const Track &TrackAtOffset(std::size_t index);

	const Track &TrackAtTime(double time);

	const std::filesystem::path &GetFilePath() const { return filePath; }
	const std::vector<Track> &GetTracks() const { return tracks; }
	const std::vector<Track>::const_iterator &GetCurrentTrack() const { return currentTrack; }
	const std::string &GetTitle() const { return title; }
	const std::string &GetPerformer() const { return performer; }

	const double GetCurrentTrackEnd(double fileLength) const;

	const double GetCurrentTrackLength(double fileLength) const;

private:
	template<typename C>
	std::basic_string<C> GetLine(std::basic_filebuf<C> &buf) {
		std::basic_string<C> ret;

		if (!buf.is_open()) return ret;

		C c = buf.sbumpc();
		while(c != static_cast<C>(EOF) && c != static_cast<C>('\r') && c != static_cast<C>('\n')) {
			ret += c;

			c = buf.sbumpc();
		}

		c = buf.sbumpc();
		while(c != static_cast<C>(EOF) && (c == static_cast<C>('\r') || c == static_cast<C>('\n'))) {
			c = buf.sbumpc();
		}

		// If we found a non-newline character,
		// put it back
		if (c != static_cast<C>(EOF))
			buf.sputbackc(c);

		return ret;
	}

	template<typename C>
	std::vector<std::vector<std::string>> ReadLines(std::basic_filebuf<C> &stream) {
		std::vector<std::vector<std::string>> lines;
		
		for (auto line = GetLine(stream); !line.empty(); line = GetLine(stream)) {
			// Files with Windows newlines cause \r to show
			// up at the end of the line since getline()
			// reads up to \n
			Fetcko::Utils::rtrim(line);

			// Split each line by whitespace
			std::vector<std::basic_string<C>> split;
			std::basic_string<C> token;
			std::optional<C> startQuote = std::nullopt;
			for (const auto &c : line) {
				if ((c >= -1 && c <= 255) && isblank(c) && !startQuote) {
					if (!token.empty()) {
						split.emplace_back(std::move(token));
						token = std::basic_string<C>();
					}
				} else if (c == static_cast<C>('\'') || c == static_cast<C>('\"')) {
					if (startQuote) {
						if (*startQuote == c)
							startQuote = std::nullopt;
						else
							token += c;
					} else startQuote = c;
				} else token += c;
			}

			if (!token.empty())
				split.emplace_back(std::move(token));

			if constexpr (std::is_same<C, wchar_t>::value || std::is_same<C, char16_t>::value) {
				std::vector<std::string> utf8;
				for (const auto &utf16 : split)
					utf8.emplace_back(Utils::ToUTF8(utf16));
				lines.emplace_back(std::move(utf8));
			} else {
				lines.emplace_back(std::move(split));
			}
		}

		return lines;
	}

	inline std::string Parse(const std::string &string);

	std::filesystem::path filePath;

	std::string title;
	std::string performer;

	uint8_t discIndex = 1;

	Utils::Encoding encoding = Utils::Encoding::Ascii;
};