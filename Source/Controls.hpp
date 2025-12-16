#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>

#include <bass.h>
#include <glad/glad.h>

#include <Event.h>

#include "MathCPP/Duration.hpp"

#include "OpenGL/Context.hpp"

#include "Utils/Logger.hpp"

#include "AlbumArt.hpp"
#include "AutoFader.hpp"
#include "Buffer.hpp"
#include "Context.hpp"
#include "ExclusiveIndicator.hpp"
#include "FPSCounter.hpp"
#include "Pause.hpp"
#include "Play.hpp"
#include "Playlist.hpp"
#include "TagLoader.hpp"
#include "Text.hpp"
#include "Volume.hpp"

using namespace MathsCPP;
using namespace Fetcko;

class Controls : public TagLoader, public AutoFader<true>, public LoggableClass {
public:
	constexpr static inline float SeekbarSize = 25.0f;

	Controls(AlbumArt *const albumArt);

	void OnInit(int windowWidth, int windowHeight, Context &context, float scale = 1.0f, GLuint defaultFramebuffer = 0);
	void OnResize(int windowWidth, int windowHeight, Context &context, float scale = 1.0f, GLuint defaultFramebuffer = 0);

	QWORD OnLoad(HSTREAM streamHandle);
	void LoadFromCue();
	void LoadFromTags(const std::map<std::string, std::string> &tags) override;
	void LoadFromID3v1(const TAG_ID3 *id3) override;

	double OnLoop(const Delta &time, HSTREAM streamHandle, Context &context, const Colour<float> &color);

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

	OpenGLFont *GetFont() { return font; }
	OpenGLFont *GetOutlineFont() { return outlineFont; }

	Pause &GetPause() { return pause; }
	Play &GetPlay() { return play; }

private:
	inline void OpenFont(Context *context, GLuint defaultFramebuffer);
	std::string FormatSeconds(int seconds) const;

	AlbumArt * const albumArt = nullptr;

	int windowWidth = 0, windowHeight = 0;

	OpenGLFont *font = nullptr;
	OpenGLFont *outlineFont = nullptr;
	Text elapsedText;
	Text elapsedOutline;
	Text remainingText;
	Text remainingOutline;
	Text titleText;
	Text titleOutline;
	Text artistText;
	Text artistOutline;
	Text albumText;
	Text albumOutline;

	FPSCounter fpsCounter;

	double currentFileLength = 0.0;

	double currentPos = 0.0;
	int elapsedSeconds = -1;

	Playlist playlist;

	Volume volume;

	ExclusiveIndicator exclusiveIndicator;

	const std::string FontRoot;

	float scale = 1.0f;

	Pause pause;
	Play play;

	std::unique_ptr<VertexArray> vao;
	std::unique_ptr<ArrayBuffer> vbo;
	std::unique_ptr<ElementBuffer> eab;

	std::unique_ptr<VertexArray> sepVao;
	std::unique_ptr<ArrayBuffer> sepVbo;
	std::unique_ptr<ElementBuffer> sepEab;

	std::unique_ptr<VertexArray> letterboxVao;
	std::unique_ptr<ArrayBuffer> letterboxVbo;
	std::unique_ptr<ElementBuffer> letterboxEab;
};
