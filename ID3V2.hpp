#pragma once

#include <codecvt>
#include <functional>
#include <locale>
#include <map>

#include "Utils.hpp"

class ID3V2 {
public:
	// https://mutagen-specs.readthedocs.io/en/latest/id3/id3v2.4.0-structure.html#id3v2-frame-overview
	enum class Encoding : uint8_t {
		Latin,
		UTF_16,		// has BOM
		UTF_16_BE,	// no BOM
		UTF_8		// no BOM
	};

	class ExtendedHeader {
		// TODO: Does BASS already exclude this by default?
		//       Given there's a BASS_TAG_ID3V2_2	tag, that's
		//       my assumption.
	};

	class Header {
	public:
		char id[3] = { 0 };
		uint8_t majorVersion = 0;
		uint8_t revisionNumber = 0;
		uint8_t flags = 0;
		uint32_t size = 0;

		Header(ID3V2 &parent);

		// Can't memcpy into the struct
		// due to alignment
		ExtendedHeader *Read(const char **tag);

		bool HasExtendedHeader() const;

	private:
		ID3V2 &parent;
	};

	// Forward declare Frame to avoid circular dependency
	class Frame;

	class Art {
	public:
		enum class Type : uint8_t {
			Other				=	0x00,
			FileIcon			=	0x01,
			OtherFileIcon		=	0x02,
			CoverFront			=	0x03,
			CoverBack			=	0x04,
			Leaflet				=	0x05,
			Media				=	0x06,
			LeadArtist			=	0x07,
			Artist				=	0x08,
			Conductor			=	0x09,
			Band				=	0x0A,
			Composer			=	0x0B,
			Lyricist			=	0x0C,
			RecordingLocation	=	0x0D,
			DuringRecording		=	0x0E,
			DuringPerformance	=	0x0F,
			ScreenCapture		=	0x10,
			ABrightColoredFish	=	0x11,
			Illustration		=	0x12,
			BandLogo			=	0x13,
			StudioLogo			=	0x14
		};

		Art(Frame &parent);
		~Art();

		Encoding textEncoding = Encoding::Latin;
		std::string mimeType; // does NOT use textEncoding
		Type type = Type::Other;
		std::string description; // DOES use textEncoding
		uint8_t *data = nullptr;
		std::size_t dataLength = 0;

		bool Read(const char **tag);

	private:
		Frame &parent;
	};

	class Frame {
	friend class Art;
	private:
		static inline const auto Dummy = [](const std::string &str) { 
			return str; 
		};

		// TRK / TRCK can be fractional (e.g. 01/08)
		// but we only care about the numerator
		// for now
		static inline const auto TrackNumber = [](const std::string &str) {
			return Fetcko::Utils::Split(str, '/')[0];
		};

	public:
		Frame(Frame &&) noexcept;
		static inline const std::map<
			std::string,
			std::pair<
				std::string,
				std::function<
					std::string(const std::string &)
				>
			>
		> RelevantFrames = {
			// v2
			{ "TP1", { "artist", Dummy }},				// Lead artist(s)/Lead performer(s)/Soloist(s)/Performing group
			{ "TAL", { "album", Dummy }},				// Album/Movie/Show title
			{ "TT2", { "title", Dummy }},				// Title/Songname/Content description
			{ "PIC", { "art", Dummy }},					// Attached picture
			{ "TPA", { "discnumber", Dummy }},			// Part of a set
			{ "TRK", { "tracknumber", TrackNumber }},	// Track number/Position in set
			// v3
			{ "TALB", { "album", Dummy }},				// [#TALB Album/Movie/Show title]
			{ "TIT2", { "title", Dummy }},				// [#TIT2 Title/songname/content description]
			{ "TPE1", { "artist", Dummy }},			// [#TPE1 Lead performer(s)/Soloist(s)]
			{ "APIC", { "art", Dummy }},				// [#sec4.15 Attached picture]
			{ "TPOS", { "discnumber", Dummy }},		// [#TPOS Part of a set]
			{ "TRCK", { "tracknumber", TrackNumber }} // [#TRCK Track number/Position in set]
		};

		std::string id;
		uint32_t size = 0;
		uint16_t flags = 0;
		uint8_t *data = nullptr;

		std::string textData;
		Art *artData = nullptr;

		Encoding encoding = Encoding::Latin;

		Frame(ID3V2 &parent);

		~Frame();

		bool Read(const char **tag, bool textOnly = false);

	private:
		void ReadEncoding(const char **tag, Encoding *encoding = nullptr);

		ID3V2 &parent;
	};

	Header header;
	std::size_t frameIdBytes = 4;
	std::size_t frameSizeBytes = 4;
	ExtendedHeader *extendedHeader = nullptr;
	std::map<std::string, Frame> frames;
	uint8_t *padding = nullptr;
	uint8_t *footer = nullptr;

	std::size_t currentOffset = 0;

	ID3V2();
	~ID3V2();

	std::map<std::string, std::string> Read(const char **tag, bool textOnly = false);

	const std::map<std::string, Frame> &GetFrames() const { return frames; }

	static void Fix32Bit(uint32_t *num, bool unsynch = true);
private:
	static std::string ToUTF8(const char *tag, uint32_t size, Encoding encoding);
};