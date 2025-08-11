#pragma once

#include <filesystem>
#include <fstream>

class MetadataReader {
public:
	MetadataReader(const std::filesystem::path &path) {
		inFile.open(path, std::ios::in | std::ios::binary);
	}

protected:
	std::ifstream inFile;
};