#include "Utils.hpp"

#include <iostream>
#include <string>
#include <sstream>

namespace Fetcko {
std::string Utils::GetStringFromFile(const std::filesystem::path & path) {
	std::ifstream inFile(path, std::ios_base::in | std::ios_base::binary);

	if (!inFile) {
		// FIXME: While CConsole allows for console _input_,
		//        we really need to harmonize it with a
		//        general-purpose logger.
		/*
		LoggableClass errorLog(typeid(Utils).name());
		errorLog.LogError("File ", path.string(), " not found");
		*/
		return "";
	}

	std::stringstream buffer;
	buffer << inFile.rdbuf();

	return buffer.str();
}

std::filesystem::path Utils::GetResource(const std::filesystem::path &path) {
#ifdef _DEBUG
	return std::filesystem::path("..") / ".." / ".." / ".." / path;
#else
	return std::filesystem::path(".") / path;
#endif
}
}