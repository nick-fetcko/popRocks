#pragma once

#include "Utils/Base64.hpp"

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
			segmentLength = std::move(other.segmentLength);
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
		std::size_t segmentLength = 0;

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
		uint32_t version = 0;
		uint8_t channels = 0;
		uint32_t samplingRate = 0;
		int32_t maxBitrate = 0;
		int32_t nominalBitrate = 0;
		int32_t minBitrate = 0;
		uint8_t blockSize = 0; // 2 4-bit values
		uint8_t framingFlag = 0; // spec erroneously claims this is 1 _bit_, not 1 _byte_
	};

	struct MetadataBlockPicture {
		uint32_t type = 0;
		uint32_t mediaTypeLength = 0;
		std::string mediaType;
		uint32_t descriptionStringLength = 0;
		std::string description;
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t depth = 0;
		uint32_t numColors = 0;
		uint32_t dataLength = 0;
		std::vector<uint8_t> data;
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

			pageHeader.segmentLength = 0;
			for (uint8_t i = 0; i < pageHeader.pageSegments; ++i)
				pageHeader.segmentLength += pageHeader.segmentTable[i];

			return pageHeader;
		}

		return std::nullopt;
	}

public:
	OGG(const std::filesystem::path &path) : FLAC(path) {

	}

	std::map<std::string, std::string> GetTags(bool textOnly = true) override {
		while(auto header = ReadPageHeader()) {
			VorbisHeader vorbisHeader;
			inFile.read(reinterpret_cast<char *>(&vorbisHeader), sizeof(VorbisHeader));
			if (vorbisHeader.IsValid()) {
				header->segmentLength -= sizeof(VorbisHeader);

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
					return GetVorbisTags(textOnly, header->segmentLength, [this] { 
						auto header = ReadPageHeader();
						return header ? header->segmentLength : 0; 
					});
				}
			} else inFile.seekg(-sizeof(VorbisHeader), std::ios::cur);
		}

		return {};
	}

	std::pair<std::string, std::vector<uint8_t>> GetArt(const std::string &base64) {
		auto decoded = Base64::Decode(base64);

		uint8_t *ptr = decoded.data();
		MetadataBlockPicture picture;

		memcpy(&picture.type, ptr, sizeof(picture.type));
		picture.type = Utils::LittleEndian(picture.type);
		ptr += sizeof(picture.type);

		memcpy(&picture.mediaTypeLength, ptr, sizeof(picture.mediaTypeLength));
		picture.mediaTypeLength = Utils::LittleEndian(picture.mediaTypeLength);
		ptr += sizeof(picture.mediaTypeLength);

		char *mediaType = new char[picture.mediaTypeLength];
		memcpy(mediaType, ptr, picture.mediaTypeLength);
		picture.mediaType = std::string(mediaType, mediaType + picture.mediaTypeLength);
		ptr += picture.mediaTypeLength;
		delete[] mediaType;

		memcpy(&picture.descriptionStringLength, ptr, sizeof(picture.descriptionStringLength));
		picture.descriptionStringLength = Utils::LittleEndian(picture.descriptionStringLength);
		ptr += sizeof(picture.descriptionStringLength);

		char *description = new char[picture.descriptionStringLength];
		memcpy(description, ptr, picture.descriptionStringLength);
		picture.description = std::string(description, description + picture.descriptionStringLength);
		ptr += picture.descriptionStringLength;
		delete[] description;

		memcpy(&picture.width, ptr, sizeof(picture.width));
		picture.width = Utils::LittleEndian(picture.width);
		ptr += sizeof(picture.width);

		memcpy(&picture.height, ptr, sizeof(picture.height));
		picture.height = Utils::LittleEndian(picture.height);
		ptr += sizeof(picture.height);

		memcpy(&picture.depth, ptr, sizeof(picture.depth));
		picture.depth = Utils::LittleEndian(picture.depth);
		ptr += sizeof(picture.depth);

		memcpy(&picture.numColors, ptr, sizeof(picture.numColors));
		picture.numColors = Utils::LittleEndian(picture.numColors);
		ptr += sizeof(picture.numColors);

		memcpy(&picture.dataLength, ptr, sizeof(picture.dataLength));
		picture.dataLength = Utils::LittleEndian(picture.dataLength);
		ptr += sizeof(picture.dataLength);

		picture.data.resize(picture.dataLength);
		memcpy(picture.data.data(), ptr, picture.dataLength);

		return { picture.mediaType, picture.data };
	}
};