#pragma once

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

// BASS_TAG_MP4 does not get iTunes-style metadata
// Embedded album art (the "moov.udta.meta.ilst.covr" atom)
// needs to be fetched manually
//
// Resources used:
// https://dev.to/alfg/a-quick-dive-into-mp4-57fo
// https://developer.apple.com/documentation/quicktime-file-format/user_data_atom
// https://atomicparsley.sourceforge.net/mpeg-4files.html
// https://github.com/google/ExoPlayer/issues/5694
// https://xhelmboyx.tripod.com/formats/mp4-layout.txt
// 
class MP4 {
public:
	class Atom {
	public:
		Atom(std::ifstream &file);
		Atom(Atom &&) noexcept;
		~Atom();

		uint32_t size = 0;
		std::string name;

		uint8_t version = 0;
		uint8_t flags[3] = { 0 };

		uint32_t dataSize = 0;
		uint8_t *data = nullptr;
		std::string mimeType;

		void Read();
		constexpr static int32_t GetExtrasSize();
		void ReadExtras();
		void ReadData();

		bool IsValid() const;
		int32_t GetBytes() const;

	private:
		// Signed so we can easily have
		// negative relative positions
		int32_t bytes = 8;

		std::ifstream &file;
	};

	MP4(const std::filesystem::path &path);

	std::optional<Atom> GetAtomAtPath(const std::vector<std::string> &path);

private:
	std::optional<Atom> SeekToAtom(const std::string &name, const std::optional<Atom> &parent = std::nullopt);

	std::ifstream file;
};