#pragma once

#include <filesystem>
#include <fstream>
#include <map>
#include <string>

class MetadataReader {
public:
	MetadataReader(const std::filesystem::path &path) {
		inFile.open(path, std::ios::in | std::ios::binary);
	}

	virtual std::map<std::string, std::string> GetTags(bool textOnly = true) = 0;

protected:
	std::ifstream inFile;
};