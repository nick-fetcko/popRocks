#pragma once

#include <filesystem>

#include "ID3V2.hpp"

class MP3 {
private:
	struct ID3 {
		char id[3];
		char title[30];
		char artist[30];
		char album[30];
		char year[4];
		char comment[30];
		uint8_t genre;

		std::string FromTag(const std::string &tag) const {
			return (tag == "title" ?
				title :
				(
					tag == "artist" ?
					artist :
					album
					)
				);
		}
	};

public:
	std::map<std::string, std::string> GetTags(const std::filesystem::path &path, bool textOnly = true) {
		std::map<std::string, std::string> ret;

		std::ifstream inFile(path, std::ios::in | std::ios::binary);

		ID3V2 id3v2;

		char header[10];
		char *ptr = header;
		inFile.read(header, 10);

		id3v2.header.Read(const_cast<const char**>(&ptr));

		// Is ID3 prepended?
		if (id3v2.header.IsValid()) {
			inFile.seekg(0);
		}
		// Is ID3 postpended?
		else {
			inFile.seekg(-10, std::ios::end);

			inFile.read(header, 10);

			id3v2.header.Read(const_cast<const char **>(&ptr));

			if (id3v2.header.IsFooter()) {
				inFile.seekg(id3v2.header.size, std::ios::end);
			} else {
				ptr = nullptr;
			}
		}

		if (ptr) {
			char *tag = new char[id3v2.header.size];
			ptr = tag;
			inFile.read(tag, id3v2.header.size);
			ret = id3v2.Read(const_cast<const char **>(&ptr), textOnly);
			delete[] tag;
		}

		std::set<std::string> missing;
		for (auto &tag : std::vector<std::string>{ "title", "artist", "album" }) {
			if (auto iter = ret.find(tag); iter == ret.end() || iter->second.empty())
				missing.emplace(tag);
		}
		
		// Check for ID3v1 if we're missing data
		if (!ptr || !missing.empty()) {
			ID3 id3;

			inFile.seekg(-sizeof(ID3), std::ios::end);

			auto whereAreWe = inFile.tellg();

			inFile.read(reinterpret_cast<char *>(&id3), sizeof(ID3));

			if (id3.id[0] == 'T' &&
				id3.id[1] == 'A' &&
				id3.id[2] == 'G') {
				for (const auto &tag : missing)
					ret[tag] = id3.FromTag(tag);
			}
		}

		return ret;
	}
};