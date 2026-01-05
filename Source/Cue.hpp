#pragma once

#include <codecvt>
#include <filesystem>
#include <fstream>
#include <locale>
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
			auto split = Fetcko::Utils::Split(line, isblank);

			std::vector<std::basic_string<C>> merged;
			std::basic_string<C> merger;

			// Find quoted sections and merge them
			std::optional<C> startQuote = std::nullopt;
			for (auto &&item : split) {
				if (!startQuote && (item[0] == static_cast<C>('\'') || item[0] == static_cast<C>(L'\"'))) {
					startQuote = item[0];
					if (auto last = item.find_last_of(*startQuote); last > 1) {
						merger = item.substr(1, last - 1);
						merged.emplace_back(std::move(merger));
						merger = std::basic_string<C>();
						startQuote.reset();
					} else merger = item.substr(1);
				} else if (!merger.empty()) {
					if (item[item.size() - 1] == startQuote) {
						merger += static_cast<C>(' ') + item.substr(0, item.size() - 1);
						merged.emplace_back(std::move(merger));
						merger = std::basic_string<C>();
						startQuote.reset();
					} else merger += static_cast<C>(' ') + item;
				} else {
					merged.emplace_back(std::move(item));
				}
			}

			if constexpr (std::is_same<C, wchar_t>::value || std::is_same<C, char16_t>::value) {
				std::vector<std::string> utf8;
				for (const auto &utf16 : merged)
					utf8.emplace_back(Utils::ToUTF8(utf16));
				lines.emplace_back(std::move(utf8));
			} else {
				lines.emplace_back(std::move(merged));
			}
		}

		return lines;
	}

	inline std::string Parse(const std::string &string);

	std::filesystem::path filePath;

	std::string title;
	std::string performer;

	uint8_t discIndex = 1;
};