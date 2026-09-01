#pragma once

#include <cstdint>
#include <map>
#include <string>

#include "MetadataReader.hpp"

// https://mutagen-specs.readthedocs.io/en/latest/apev2/apev2.html
//
// Embedded cover art seems largely undocumented, so I used
// MusicBee to embed art into an APE file and based my
// implementation off of what it did.
class APE : public virtual MetadataReader {
protected:
	struct ApeTagEx {
		uint64_t preamble;
		uint32_t version;
		uint32_t size;
		uint32_t count;
		uint32_t flags;
		uint64_t reserved;

		bool IsValid(bool footer) const {
			return preamble == 0x5845474154455041 &&
				version == 2000 &&
				(footer ? flags ^ 0x20000000 : flags & 0x20000000);
		}
	};

	struct ApeTagItem {
		uint32_t size = 0;
		uint32_t flags = 0;

		std::string key;
		std::string value;

		std::vector<uint8_t> data;
		std::string mimeType;

		ApeTagItem() = default;

		ApeTagItem(ApeTagItem &&other) noexcept {
			size = std::move(other.size);
			flags = std::move(other.flags);
			key = std::move(other.key);
			value = std::move(other.value);
			data = std::move(other.data);
			mimeType = std::move(other.mimeType);
		}

		~ApeTagItem() {
		}
	};

	static inline const std::map<std::string, std::string> RelevantItems = {
		{ "Track", "tracknumber" },
		{ "Title", "title" },
		{ "Artist", "artist" },
		{ "Album", "album" },
		{ "Media", "discnumber" }
	};

	inline void GetKey(std::string &key) {
		char c = 0;
		inFile.read(&c, 1);
		while (c) {
			key += c;
			inFile.read(&c, 1);
		}
	}

	std::map<std::string, ApeTagItem> items;

public:
	APE(const std::filesystem::path &path) : MetadataReader(path) {
		// APE tags are always at the end
		inFile.seekg(0, std::ios::end);
	}

	std::map<std::string, std::string> GetTags(bool textOnly = true) {
		std::map<std::string, std::string> ret;

		inFile.seekg(-sizeof(ApeTagEx), std::ios::cur);

		ApeTagEx footer;

		inFile.read(reinterpret_cast<char *>(&footer), sizeof(ApeTagEx));

		if (footer.IsValid(true)) {
			inFile.seekg(-(footer.size + sizeof(ApeTagEx)), std::ios::cur);

			ApeTagEx header;
			inFile.read(reinterpret_cast<char *>(&header), sizeof(ApeTagEx));

			if (header.IsValid(false)) {
				for (uint32_t i = 0; i < header.count; ++i) {
					ApeTagItem item;
					inFile.read(reinterpret_cast<char *>(&item), sizeof(uint32_t) * 2);

					GetKey(item.key);

					if (!textOnly && item.key.find("Cover Art") != std::string::npos) {
						// Filename is null terminated, just like keys
						GetKey(item.mimeType);
						item.size -= item.mimeType.length();
						item.data.resize(item.size);

						inFile.read(reinterpret_cast<char *>(item.data.data()), item.size);

						auto extension = item.mimeType.substr(item.mimeType.find_last_of('.'));
						std::transform(extension.begin(), extension.end(), extension.begin(), tolower);

						if (extension == ".jpg" || extension == ".jpeg")
							item.mimeType = "image/jpeg";
						else if (extension == ".png")
							item.mimeType = "image/png";

						items.emplace(std::make_pair("art", std::move(item)));
					} else {
						auto *value = new char[item.size];
						inFile.read(value, item.size);
						item.value = std::string(value, value + item.size);
						delete[] value;

						if (auto iter = RelevantItems.find(item.key); iter != RelevantItems.end())
							ret[iter->second] = item.value;
					}
				}
			}
		}

		return ret;
	}

	const std::map<std::string, ApeTagItem> &GetItems() const { return items; }
	std::vector<uint8_t> &&TakeArt() { return std::move(items["art"].data); }
};