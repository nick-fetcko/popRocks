#pragma once

#include <bass.h>
#include <bassflac.h>

#include "Utils/Logger.hpp"

#include "AlbumArt.hpp"
#include "ID3V2.hpp"
#include "MP4.hpp"
#include "TagLoader.hpp"

using namespace Fetcko;

class Metadata : public LoggableClass {
public:
	void OnLoad(
		const std::filesystem::path &path,
		const std::string &extension,
		HSTREAM streamHandle,
		TagLoader *tagLoader,
		AlbumArt *albumArt = nullptr
	);

private:
	std::map<std::string, std::string> GetTags(const char *tags) const;
};