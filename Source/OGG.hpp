#pragma once

#include "FLAC.hpp"

// https://xiph.org/ogg/doc/framing.html
// https://xiph.org/vorbis/doc/Vorbis_I_spec.html
class OGG : public FLAC {
private:
	struct PageHeader {
		PageHeader() = default;

		PageHeader(PageHeader &&other) noexcept {
			capturePattern = std::move(other.capturePattern);
			streamStructureVersion = std::move(other.streamStructureVersion);
			headerTypeFlag = std::move(other.headerTypeFlag);
			absoluteGranulePosition = std::move(other.absoluteGranulePosition);
			streamSerialNumber = std::move(other.streamSerialNumber);
			pageSequenceNumber = std::move(other.pageSequenceNumber);
			pageChecksum = std::move(other.pageChecksum);
			pageSegments = std::move(other.pageSegments);
			segmentTable = std::move(other.segmentTable);
			other.segmentTable = nullptr;
		}

		uint32_t capturePattern = 0;
		uint8_t streamStructureVersion = 0;
		uint8_t headerTypeFlag = 0;
		uint64_t absoluteGranulePosition = 0;
		uint32_t streamSerialNumber = 0;
		uint32_t pageSequenceNumber = 0;
		uint32_t pageChecksum = 0;
		uint8_t pageSegments = 0;
		uint8_t *segmentTable = nullptr;

		~PageHeader() {
			delete[] segmentTable;
		}
	};

	struct VorbisHeader {
		uint8_t type = 0;
		uint8_t vorbis[6] = { 0 };

		bool IsValid() const {
			return vorbis[0] == 'v' &&
				vorbis[1] == 'o' &&
				vorbis[2] == 'r' &&
				vorbis[3] == 'b' &&
				vorbis[4] == 'i' &&
				vorbis[5] == 's';
		}
	};

	struct IdentificationHeader {
		VorbisHeader header;
		uint32_t version;
		uint8_t channels;
		uint32_t samplingRate;
		int32_t maxBitrate;
		int32_t nominalBitrate;
		int32_t minBitrate;
		uint8_t blockSize; // 2 4-bit values
		uint8_t framingFlag; // spec erroneously claims this is 1 _bit_, not 1 _byte_
	};

	std::optional<PageHeader> ReadPageHeader() {
		PageHeader pageHeader;

		inFile.read(reinterpret_cast<char *>(&pageHeader.capturePattern), sizeof(pageHeader.capturePattern));

		if (pageHeader.capturePattern == 0x5367674F) { // "OggS" in Little Endian
			inFile.read(reinterpret_cast<char *>(&pageHeader.streamStructureVersion), sizeof(pageHeader.streamStructureVersion));
			inFile.read(reinterpret_cast<char *>(&pageHeader.headerTypeFlag), sizeof(pageHeader.headerTypeFlag));
			inFile.read(reinterpret_cast<char *>(&pageHeader.absoluteGranulePosition), sizeof(pageHeader.absoluteGranulePosition));
			inFile.read(reinterpret_cast<char *>(&pageHeader.streamSerialNumber), sizeof(pageHeader.streamSerialNumber));
			inFile.read(reinterpret_cast<char *>(&pageHeader.pageSequenceNumber), sizeof(pageHeader.pageSequenceNumber));
			inFile.read(reinterpret_cast<char *>(&pageHeader.pageChecksum), sizeof(pageHeader.pageChecksum));
			inFile.read(reinterpret_cast<char *>(&pageHeader.pageSegments), sizeof(pageHeader.pageSegments));

			pageHeader.segmentTable = new uint8_t[pageHeader.pageSegments];
			inFile.read(reinterpret_cast<char *>(pageHeader.segmentTable), pageHeader.pageSegments);

			return pageHeader;
		}

		return std::nullopt;
	}

public:
	OGG(const std::filesystem::path &path) : FLAC(path) {

	}

	std::map<std::string, std::string> GetTags(bool textOnly = true) override {
		while(ReadPageHeader()) {
			VorbisHeader vorbisHeader;
			inFile.read(reinterpret_cast<char *>(&vorbisHeader), sizeof(VorbisHeader));
			if (vorbisHeader.IsValid()) {
				// the identification header is type 1,
				// the comment header type 3
				// and the setup header type 5 
				if (vorbisHeader.type == 1) {
					IdentificationHeader identificationHeader;
					identificationHeader.header = std::move(vorbisHeader);

					inFile.read(reinterpret_cast<char *>(&identificationHeader.version), sizeof(identificationHeader.version));
					inFile.read(reinterpret_cast<char *>(&identificationHeader.channels), sizeof(identificationHeader.channels));
					inFile.read(reinterpret_cast<char *>(&identificationHeader.samplingRate), sizeof(identificationHeader.samplingRate));
					inFile.read(reinterpret_cast<char *>(&identificationHeader.maxBitrate), sizeof(identificationHeader.maxBitrate));
					inFile.read(reinterpret_cast<char *>(&identificationHeader.nominalBitrate), sizeof(identificationHeader.nominalBitrate));
					inFile.read(reinterpret_cast<char *>(&identificationHeader.minBitrate), sizeof(identificationHeader.minBitrate));
					inFile.read(reinterpret_cast<char *>(&identificationHeader.blockSize), sizeof(identificationHeader.blockSize));
					inFile.read(reinterpret_cast<char *>(&identificationHeader.framingFlag), sizeof(identificationHeader.framingFlag));
				} else if (vorbisHeader.type == 3) {
					return GetVorbisTags(textOnly);
				}
			} else inFile.seekg(-sizeof(VorbisHeader), std::ios::cur);
		}

		return {};
	}
};