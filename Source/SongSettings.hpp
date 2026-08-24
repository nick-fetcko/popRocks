#pragma once

#include <filesystem>
#include <map>
#include <string>

#include "Serial/Json.hpp"

using namespace serial;

class SongSettings {
public:
	struct SongSetting {
		std::string albumArt;
	};

	void Load();

	void SetPreferredAlbumArt(const std::filesystem::path &path, const std::string &art);

	const bool HasSettings(const std::filesystem::path &path) const;
	SongSetting *GetSettings(const std::filesystem::path &path);

	void RemoveSetting(const std::filesystem::path &path);

	friend const Node &operator>>(const Node &node, SongSetting &setting);
	friend Node &operator<<(Node &node, const SongSetting &setting);

private:
	void Save();

	std::map<std::string, SongSetting> settings;
};
