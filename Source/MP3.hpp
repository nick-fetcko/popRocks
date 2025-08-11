#pragma once

#include <filesystem>

#include "ID3V2.hpp"
#include "MetadataReader.hpp"

class MP3 : public MetadataReader {
protected:
	struct ID3 {
		char id[3] = { 0 };
		char title[30] = { 0 };
		char artist[30] = { 0 };
		char album[30] = { 0 };
		char year[4] = { 0 };
		char comment[30] = { 0 };
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

	inline std::optional<ID3> ReadID3V1() {
		ID3 id3;

		inFile.seekg(-sizeof(ID3), std::ios::end);

		inFile.read(reinterpret_cast<char *>(&id3), sizeof(ID3));

		return id3.id[0] == 'T' &&
			id3.id[1] == 'A' &&
			id3.id[2] == 'G' ? 
			id3 : 
			static_cast<std::optional<ID3>>(std::nullopt);
	}

	inline std::set<std::string> WhatsMissing(const std::map<std::string, std::string> &ret) {
		std::set<std::string> missing;
		for (auto &tag : std::vector<std::string>{ "title", "artist", "album" }) {
			if (auto iter = ret.find(tag); iter == ret.end() || iter->second.empty())
				missing.emplace(tag);
		}

		return missing;
	}

public:
	MP3(const std::filesystem::path &path) : MetadataReader(path) {

	}

	std::map<std::string, std::string> GetTags(bool textOnly = true) {
		std::map<std::string, std::string> ret;

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
		
		// Check for ID3v1 if we're missing data
		if (auto missing = WhatsMissing(ret); !ptr || !missing.empty()) {
			auto id3 = ReadID3V1();

			if (id3) {
				for (const auto &tag : missing)
					ret[tag] = id3->FromTag(tag);
			}
		}

		return ret;
	}
};