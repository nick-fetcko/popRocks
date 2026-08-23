#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

class CApp;
class Integration {
public:
	Integration(CApp *app);

	virtual void OnDestroy();

	virtual ~Integration();

	virtual bool IsEnabled() const = 0;

	virtual void Enable() = 0;
	virtual void Disable() = 0;

	virtual void SetPosition(int64_t position) { this->position = position; }

	virtual void OnSongChanged(
		const uint64_t &hash,
		const std::string &title,
		const std::string &artist,
		const std::string &album,
		const std::filesystem::path &externalArt,
		const int64_t length, // in microseconds
		bool hasArt = true
	) = 0;

	virtual void OnVisualizerChanged(const std::string &name) = 0;

	virtual void OnPlay() = 0;
	virtual void OnPause() = 0;

protected:
	CApp *_app = nullptr;

	int64_t position = 0;
};