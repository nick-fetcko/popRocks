#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>

#include <bass.h>
#include <glad/glad.h>
#include <SDL_ttf.h>

#include <event.h>

#include "MathCPP/Duration.hpp"

#include "AlbumArt.hpp"
#include "AutoFader.hpp"
#include "Buffer.hpp"
#include "ExclusiveIndicator.hpp"
#include "FPSCounter.hpp"
#include "Playlist.hpp"
#include "TagLoader.hpp"
#include "Text.hpp"
#include "Volume.hpp"

using namespace MathsCPP;

class Controls : public TagLoader, public AutoFader {
public:
	constexpr static inline float SeekbarSize = 25.0f;

	Controls(AlbumArt *const albumArt);

	void OnInit(int windowWidth, int windowHeight, float scale = 1.0f);
	void OnResize(int windowWidth, int windowHeight, float scale = 1.0f);

	QWORD OnLoad(HSTREAM streamHandle);
	void LoadFromCue();
	void LoadFromTags(const std::map<std::string, std::string> &tags) override;
	void LoadFromID3v1(const TAG_ID3 *id3) override;

	double OnLoop(const Delta &time, HSTREAM streamHandle, std::function<void(float)> setColor);

	void OnDestroy();

	void SetElapsedSeconds(int elapsedSeconds);

	double GetCurrentFileLength() const;
	double GetCurrentSongLength() const;
	std::optional<double> GetNextSongLength() const;

	bool AreThereEmptyTags() const override { return titleText.Empty() || artistText.Empty() || albumText.Empty(); }
	bool HasTitle() const override { return !titleText.Empty(); }

	void SetTitle(const std::string &title) override;

	double GetCurrentPosition() const { return currentPos; }

	FPSCounter &GetFpsCounter() { return fpsCounter; }
	Playlist &GetPlaylist() { return playlist; }
	Volume &GetVolume() { return volume; }
	ExclusiveIndicator &GetExclusiveIndicator() { return exclusiveIndicator; }

private:
	inline void OpenFont();
	std::string FormatSeconds(int seconds) const;

	AlbumArt * const albumArt = nullptr;

	int windowWidth = 0, windowHeight = 0;

	TTF_Font *font = nullptr;
	Text elapsedText;
	Text remainingText;
	Text titleText;
	Text artistText;
	Text albumText;

	FPSCounter fpsCounter;

	double currentFileLength = 0.0;
	float posRect[8] = { 0 };

	double currentPos = 0.0;
	int elapsedSeconds = -1;

	Playlist playlist;

	Volume volume;

	ExclusiveIndicator exclusiveIndicator;

	const std::filesystem::path FontFile;

	float scale = 1.0f;
};