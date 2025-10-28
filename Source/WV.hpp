#pragma once

#include "APE.hpp"
#include "MP3.hpp"

class WV : public virtual APE, public virtual MP3 {
private:
public:
	WV(const std::filesystem::path &path) : APE(path), MP3(path), MetadataReader(path) {

	}

	std::map<std::string, std::string> GetTags(bool textOnly = true) {
		// Keep ID3v1 around in case we
		// don't have APE tags.
		auto id3 = ReadID3V1();
		if (id3)
			MetadataReader::inFile.seekg(-sizeof(ID3), std::ios::end);

		auto ret = APE::GetTags(textOnly);

		if (auto missing = WhatsMissing(ret); !missing.empty() && id3) {
			for (const auto &tag : missing)
				ret[tag] = id3->FromTag(tag);
		}

		return ret;
	}
};