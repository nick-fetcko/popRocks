#pragma once

#include <bass.h>
#include <bassflac.h>

#include "AlbumArt.hpp"
#include "CConsole.h"
#include "ID3V2.hpp"
#include "MP4.hpp"
#include "TagLoader.hpp"

class Metadata {
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