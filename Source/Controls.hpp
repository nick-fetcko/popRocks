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
#include "Checkbox.hpp"
#include "ExclusiveIndicator.hpp"
#include "FPSCounter.hpp"
#include "Next.hpp"
#include "Pause.hpp"
#include "Play.hpp"
#include "Playlist.hpp"
#include "Preset.hpp"
#include "PresetList.hpp"
#include "Previous.hpp"
#include "TagLoader.hpp"
#include "ScrollingText.hpp"
#include "Volume.hpp"

using namespace MathsCPP;
using namespace Fetcko;

class Controls : public TagLoader, public AutoFader<true>, public LoggableClass, public AlbumArt::BlackChangedListener, public Preset::ChangeListener {
public:
	constexpr static inline float SeekbarSize = 25.0f;
	constexpr static inline float MiniPlayerSeekbarRatio = 1.5f;
	constexpr static inline float MiniPlayerIconRatio = 0.1667f; // ~1/6th

	enum class ControlButton {
		None,
		PlayPause,
		Previous,
		Next,
		CaptureCheckbox
	};

	Controls(AlbumArt *const albumArt, const bool &vulkan);
	~Controls();

	void OnInit(int windowWidth, int windowHeight, Context &context, float scale = 1.0f, GLuint defaultFramebuffer = 0, bool miniPlayer = false);
	void SetMiniPlayer(Context &context, bool miniPlayer);
	void OnResize(int windowWidth, int windowHeight, Context &context, float scale = 1.0f, GLuint defaultFramebuffer = 0, bool miniPlayer = false);
	void OnRadiusChanged(Context &context);
	void OnRadiusChanged(Context &context, bool miniPlayer);

	QWORD OnLoad(HSTREAM streamHandle);
	void LoadFromCue();
	void LoadFromTags(const std::map<std::string, std::string> &tags) override;
	void ClearTags() override;
	void LoadFromID3v1(const TAG_ID3 *id3) override;

	double OnLoop(const Delta &time, HSTREAM streamHandle, Context &context, const Colour<float> &color, bool playing);

	void OnDestroy();

	void SetElapsedSeconds(int elapsedSeconds);

	double GetCurrentFileLength() const;
	double GetCurrentSongLength() const;
	std::optional<double> GetNextSongLength() const;

	bool AreThereEmptyTags() const override { return titleText.Empty() || artistText.Empty() || albumText.Empty(); }
	bool HasTitle() const override { return !titleText.Empty(); }

	void SetTitle(const std::string &title) override;
	const std::string &GetTitle() const { return titleText.GetText(); }

	double GetCurrentPosition() const { return currentPos; }

	FPSCounter &GetFpsCounter() { return fpsCounter; }
	const Playlist &GetPlaylist() const { return playlist; }
	Playlist &GetPlaylist() { return playlist; }
	const PresetList &GetPresetList() const { return presetList; }
	Volume &GetVolume() { return volume; }
	ExclusiveIndicator &GetExclusiveIndicator() { return exclusiveIndicator; }

	OpenGLFont *GetFont() { return font; }
	OpenGLFont *GetBoldFont() { return boldFont; }
	OpenGLFont *GetOutlineFont() { return outlineFont; }
	OpenGLFont *GetBoldOutlineFont() { return boldOutlineFont; }

	Pause &GetPause() { return pause; }
	Play &GetPlay() { return play; }
	Next &GetNext() { return next; }
	Previous &GetPrevious() { return previous; }

	void OnMouseMoved(const Vector2i &mousePos);
	ControlButton OnMouseClicked(const Vector2i &mousePos, std::function<void(float)> seekCallback, bool playing, bool canTakeAction = true);

	void AddToScrollOffset(int offset);

	void OnBlackChanged(const float &black) override;

	void OnPresetChanged(const std::vector<Preset> &presets);

	const float &GetAlpha() const override { return std::max(volume.GetAlpha(), alpha); }

	void OnPresetsChanged(const std::vector<Preset> &presets) override;

	inline const float GetIconSize() const {
		return albumArt->GetRadius(miniPlayer) * (miniPlayer ? Controls::MiniPlayerIconRatio : 1.0f);
	}

	float UpdateFontSize(std::optional<float> radius = std::nullopt);

private:
	inline void OpenFont(Context *context, GLuint defaultFramebuffer);
	std::string FormatSeconds(int seconds) const;
	ControlButton GetButtonAtPos(const Vector2i &pos);

	AlbumArt * const albumArt = nullptr;

	int windowWidth = 0, windowHeight = 0;

	OpenGLFont *font = nullptr;
	OpenGLFont *boldFont = nullptr;
	OpenGLFont *outlineFont = nullptr;
	OpenGLFont *boldOutlineFont = nullptr;
	Text elapsedText;
	Text elapsedOutline;
	Text remainingText;
	Text remainingOutline;
	Text titleText;
	Text titleOutline;
	ScrollingText artistText;
	ScrollingText artistOutline;
	ScrollingText albumText;
	ScrollingText albumOutline;

	ScrollingText presetText;
	ScrollingText presetOutline;
	PresetList presetList;

	FPSCounter fpsCounter;

	double currentFileLength = 0.0;

	double currentPos = 0.0;
	int elapsedSeconds = -1;

	Playlist playlist;

	Volume volume;

	ExclusiveIndicator exclusiveIndicator;

	const std::string FontRoot;

	float scale = 1.0f;

	bool miniPlayer = Settings::settings.GetMiniPlayer();
	float iconY = 0.0f;

	Pause pause;
	Play play;
	Next next;
	Previous previous;
	Checkbox captureCheckbox;

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
