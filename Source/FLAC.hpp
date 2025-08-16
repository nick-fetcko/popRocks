#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>

#include "Utils/Utils.hpp"

#include "ID3V2.hpp"
#include "MetadataReader.hpp"

using namespace Fetcko;

class FLAC : public MetadataReader {
protected:
	struct MetadataBlock {
		uint8_t type = 0;
		uint8_t length[3] = { 0 };

		bool IsLast() const {
			return type & 0x80;
		}

		uint32_t GetLength() const {
			return length[2] | length[1] << 8 | length[0] << 16;
		}
	};

	struct VorbisCommentBlock {
		uint32_t vendorLength = 0;
		std::string vendorString;

		uint32_t commentListLength = 0;
	};

	std::map<std::string, std::string> GetVorbisTags(bool textOnly = true, std::size_t maxLength = 0, std::optional<std::function<std::size_t()>> readHeader = std::nullopt) {
		std::size_t length = 0;

		VorbisCommentBlock commentBlock;

		inFile.read(reinterpret_cast<char *>(&commentBlock.vendorLength), sizeof(uint32_t));
		length += sizeof(uint32_t);
		length += commentBlock.vendorLength;

		char *vendorString = new char[commentBlock.vendorLength];
		inFile.read(vendorString, commentBlock.vendorLength);
		commentBlock.vendorString = std::string(vendorString, vendorString + commentBlock.vendorLength);
		delete[] vendorString;

		inFile.read(reinterpret_cast<char *>(&commentBlock.commentListLength), sizeof(uint32_t));
		length += sizeof(uint32_t);

		std::map<std::string, std::string> ret;
		for (uint32_t i = 0; i < commentBlock.commentListLength; ++i) {
			uint32_t commentLength = 0;
			inFile.read(reinterpret_cast<char *>(&commentLength), sizeof(uint32_t));
			length += sizeof(uint32_t);

			// We're assuming anything > 10,000 bytes is not text data
			if (!textOnly || (textOnly && commentLength < 10000)) {
				char *commentString = new char[commentLength];

				if (maxLength > 0 && length + commentLength > maxLength) {
					auto remaining = maxLength - length;
					inFile.read(commentString, remaining);
					length = 0;
					if (readHeader)
						maxLength = (*readHeader)();
					inFile.read(&commentString[remaining], commentLength - remaining);
					length += commentLength - remaining;
				} else {
					inFile.read(commentString, commentLength);
					length += commentLength;
				}

				if (auto split = Utils::SplitOnce(std::string(commentString, commentString + commentLength), '=');
					split.size() > 1) {
					std::transform(split[0].begin(), split[0].end(), split[0].begin(), tolower);
					if (split[0] == "metadata_block_picture")
						ret["art"] = split[1];
					else
						ret[split[0]] = split[1];
				}

				delete[] commentString;
			} else inFile.seekg(commentLength, std::ios::cur);
		}

		return ret;
	}

public:
	FLAC(const std::filesystem::path &path) : MetadataReader(path) {

	}

	std::map<std::string, std::string> GetTags(bool textOnly = true) override {
		// Make sure first 4 bytes are "fLaC"
		uint32_t fourCC = 0;
		inFile.read(reinterpret_cast<char *>(&fourCC), sizeof(uint32_t));

		// FLACs with ID3 tags are technically out of spec,
		// but Exact Audio Copy will happily generate them.
		//
		// We'll just skip ID3 tags if we find them.
		if ((fourCC & 0x00FFFFFF) == 0x334449) { // ID3 (in Little Endian)
			inFile.seekg(2, std::ios::cur); // revisionNumber + flags

			uint32_t size;
			inFile.read(reinterpret_cast<char *>(&size), sizeof(uint32_t));
			ID3V2::Fix32Bit(&size);

			inFile.seekg(size, std::ios::cur);
			inFile.read(reinterpret_cast<char *>(&fourCC), sizeof(uint32_t));
		}

		if (fourCC == 0x43614C66) { // fLaC (in Little Endian)
			MetadataBlock block;

			while (!block.IsLast()) {
				inFile.read(reinterpret_cast<char *>(&block), sizeof(MetadataBlock));
				if ((block.type & 0x7F) == 4) { // VORBIS_COMMENT
					return GetVorbisTags(textOnly);
				} else {
					auto length = block.GetLength();
					inFile.seekg(length, std::ios::cur);
				}
			}
		}

		return {};
	}
};