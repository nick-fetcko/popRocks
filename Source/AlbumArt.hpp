#pragma once

#include <filesystem>
#include <array>
#include <map>
#include <mutex>
#include <functional>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <glad/glad.h>
#include <SDL3_image/SDL_image.h>

#include "MathCPP/Colour.hpp"

#include "OpenGL/Context.hpp"
#include "OpenGL/Cube.hpp"
#include "OpenGL/VertexArray.hpp"
#include "OpenGL/Buffer.hpp"
#include "OpenGL/Texture.hpp"
#include "OpenGL/Polyline.hpp"

#include "Utils/Logger.hpp"

#include "Circle.hpp"
#include "ColorChangeListener.hpp"
#include "Settings.hpp"
#include "Text.hpp"

using namespace MathsCPP;
using namespace Fetcko;

class Platform;
class Controls;
class AlbumArt : public Circle<Circles::Textured> {
private:
	constexpr static std::size_t OutlinePoints = 100;

public:
	static constexpr bool IsSupported(const std::string_view &lowercaseExtension) {
		for (const auto &extension : SupportedExtensions)
			if (lowercaseExtension == extension)
				return true;

		return false;
	}

	enum class ColorMethod { Average, Dominant };

	class BlackChangedListener {
	public:
		virtual ~BlackChangedListener() {

		}

		virtual void OnBlackChanged(const float &black) = 0;
	};

	enum class Outline {
		None,
		Art,
		Visualizer
	};

	constexpr static inline float BaseRadius = 200.0f;

	AlbumArt(Controls *const controls, std::unique_ptr<Context> &context, std::unique_ptr<Platform> &platform);
	virtual ~AlbumArt();

	void OnInit(int windowWidth, int windowHeight, float scale = 1.0f);
	void OnResize(int windowWidth, int windowHeight, float scale = 1.0f);
	void OnLoop(const Delta &time, GLfloat x, GLfloat y, float frameCount, float alpha, Context &context, bool playing, bool resizable = true);
	void OnDestroy() override;

	// fileName is the path to the _song_
	//
	// This function finds the album art
	// relative to that path. If the original
	// path is multiple folders up from the
	// song, we can pass that in the parentPath
	// argument. This allows for loading folders
	// structured like:
	//
	// Album
	//   /art
	//     cover.jpg
	//     ...
	//   /cd-1
	//     track1.flac
	//     track2.flac
	//     ...
	//   /cd-2
	//     track1.flac
	//     track2.flac
	//     ...
	bool Load(const std::filesystem::path &fileName, const std::filesystem::path &parentPath = "", bool force = false);

	bool Load(const std::string &mimeType, const void *data, std::size_t length, bool force = false);
	bool Load(const std::string mimeType, std::vector<uint8_t> &&data);
	bool LoadEmbedded(const std::string &mimeType, const void *data, std::size_t length, bool force = false) {
		return Load(mimeType, data, length, force);
	}

	void UpdateParentPath(const std::filesystem::path &parentPath);

	void Reset(const Colour<float> &color, bool fromPlaylist = false);

	void NextBin(bool silent = false);
	void PreviousBin();
	void ResetBin(bool silent = false);

	bool Loaded() const { return albumLoaded; }

	const Colour<float> &GetColor() const { return averageColor; }

	void AddColorChangeListener(ColorChangeListener *listener);
	void RemoveColorChangeListener(ColorChangeListener *listener);

	void AddBlackChangedListener(BlackChangedListener *listener);
	void RemoveBlackChangedListener(BlackChangedListener *listener);

	int DrawSquare(int x, int y, int height, GLfloat alpha, Context &context);

	const float &GetAspectRatio() const { return aspectRatio; }

	void Scale(bool force = false);

	const float GetRadius(bool miniPlayer) const;
	void SetRadius(float radius, bool miniPlayer);

	void ReprocessColors();

	const std::vector<Colour<float>> &GetSelectedColors() const;

	std::unique_lock<std::mutex> Lock() { return std::move(std::unique_lock(histogramMutex)); }

	const std::multimap<int, std::filesystem::path> &GetPreferred() const { return preferred; }
	const std::vector<std::filesystem::path> &GetFound() const { return found; }

	const std::filesystem::path &GetCurrentFile() const { return currentFile; }

	void ClearEmbedded();

	bool HasEmbedded() const { return embeddedData; }
	bool LoadEmbedded();

	const std::filesystem::path &GetSearchFolder() const { return searchFolder; }

	const Colour<float> &GetAverageColor() const { return averageColor; }

	const bool &IsHidden() const { return hidden; }
	void SetHidden(bool hidden) { this->hidden = hidden; }

	const std::unique_ptr<Cube> &GetCube() { return cube; }

	bool OnMouseDown(const Vector2i &mousePos);
	void OnMouseUp(const Vector2i &mousePos);
	bool OnMouseClicked(const Vector2i &mousePos);
	bool OnMouseMoved(const Vector2i &mousePos);
	bool OnMouseDragged(const Vector2i &mousePos);
	void OnMouseLeave();

	const bool IsBlackAndWhite() const { return blackAndWhite; }

	void CalculateChroma();
	const float &GetChromaColor() const { 
		const static float zero = 0.0f;
		return zero; 

		//return chromaColor;
	}
	const float &GetBlackColor() const { return blackColor; }
	void ResetChroma();

	const Colourf &GetTintedBlackColor() const { return tintedBlackColor; }

	const bool HasChromaChanged() { auto ret = chromaChanged.load(); chromaChanged = false; return ret; }

	const bool IsResizing() const { return activeOutline != Outline::None; }
	const Outline GetActiveOutline() const { return activeOutline; }

	void DrawPlaceholder(GLfloat x, GLfloat y, float alpha, Context &context) const;

	void OverrideOutlineAlpha(float overrideOutlineAlpha) { this->overrideOutlineAlpha = overrideOutlineAlpha; }

	const Fetcko::Polyline &GetOutline() const { return outline; }
	const Fetcko::Polyline &GetVisualizerOutline() const { return visualizerOutline; }

	const SDL_SystemCursor GetCursor() const { return cursor; }

	std::pair<uint8_t *, std::size_t> GetEmbedded() { return { embeddedData, embeddedDataLength };}

private:
	constexpr inline static std::array<std::string_view, 3> SupportedExtensions = { ".jpg", ".png", ".webp" };

	std::filesystem::path FindArt(const std::filesystem::path &folder, std::optional<std::filesystem::path> fileName = std::nullopt);

	// This frees the surface once it's done
	void LoadFromSurface(SDL_Surface *surface, std::filesystem::path path = "", std::string extension = "", bool scaled = false);

	void UpdateVertexCoords() override;

	void UpdateBin(bool silent = false);
	void PrintBin();

	inline uint8_t *GetPixels(SDL_Surface *surface);

	inline void UpdateOutline();

	inline void UpdateCursor(const Vector2i &mousePos);

	Outline IsCursorOnOutline(const Vector2i &mousePos);

	inline void UpdateFontSize();

	void CalculateChroma(SDL_Surface *surface, const uint8_t *pixels);

	GLuint album = 0;
	int albumWidth = 0, albumHeight = 0;
	float squareHeight = 0;
	float squareWidth = 0;
	int scaledAlbumWidth = 0, scaledAlbumHeight = 0;
	bool albumLoaded = false;

	float aspectRatio = 1.0f;

	ColorMethod colorMethod = ColorMethod::Dominant;
	Colour<float> averageColor{ 1.0f, 1.0f, 1.0f };
	struct Bin {
		Bin(std::size_t count, float h, float s, float v) : count(count), h(h), s(s), v(v) {}

		std::size_t count;
		float h = 0.0f;
		float s = 0.0;
		float v = 0.0f;
	};

	struct CompareBins {
		bool operator()(const Bin &lhs, const Bin &rhs) const {
			return 
				lhs.count < rhs.count ||
				(lhs.count == rhs.count && lhs.h < rhs.h);

			// The original container is sorted by hue,
			// so we'll never have two bins with the same
			// hue.
		}
	};

	using Histogram = std::multiset<Bin, CompareBins>;

	Histogram histogram;
	Histogram::reverse_iterator binIter;

	std::vector<Colour<float>> selectedColors;

	void ProcessColors(Histogram *destination, SDL_Surface *surface, const uint8_t *pixels, bool initial = false);

	std::vector<Histogram::reverse_iterator> previousBins;

	std::set<ColorChangeListener *> colorChangeListeners;

	std::uint32_t lastHash = 0;
	std::uint32_t lastEmbeddedHash = 0;
	std::size_t lastEmbeddedLength = 0;
	int lastWidth = 0, lastHeight = 0;

	SDL_Surface *lastSurface = nullptr;
	std::atomic<bool> lastSurfaceUpdated = false;
	SDL_Surface *surfaceToLoad = nullptr;

	std::mutex scalingMutex;
	std::mutex histogramMutex;

	float scale = 1.0f;

	std::unique_ptr<VertexArray> squareVao;
	std::unique_ptr<ArrayBuffer> squareVbo;
	std::unique_ptr<ElementBuffer> squareEab;

	std::unique_ptr<Context> &context;
	std::unique_ptr<Platform> &platform;

	std::thread scaleThread;
	bool scaling = false;

	std::thread colorProcessingThread;
	bool processingColors = false;

	std::filesystem::path searchFolder;
	std::multimap<int, std::filesystem::path> preferred;
	std::vector<std::filesystem::path> found;
	std::filesystem::path currentFile;

	uint8_t *embeddedData = nullptr;
	std::size_t embeddedDataLength = 0;
	std::string embeddedDataMimeType;

	bool hidden = false;

	std::unique_ptr<Cube> cube;

	std::filesystem::path lastParentPath;

	int windowWidth = 0;
	int windowHeight = 0;

	bool blackAndWhite = false;

	Circle<Circles::Plain> placeholder;
	Text dragAndDropPrompt;

	float chromaColor = 0.0f;
	std::atomic<bool> chromaChanged = false;

	float blackColor = 0.0f;
	Colourf tintedBlackColor = { blackColor, blackColor, blackColor };

	SDL_Cursor *resizeCursor = nullptr;

	Fetcko::Polyline outline;
	Fetcko::Polyline visualizerOutline;
	float outlineAlpha = 0.0f;
	float targetOutlineAlpha = 0.0f;
	float overrideOutlineAlpha = 0.0f;

	std::set<BlackChangedListener*> blackChangedListeners;

	Outline activeOutline = Outline::None;

	OpenGLFont *font = nullptr;
	OpenGLFont *boldFont = nullptr;
	OpenGLFont *outlineFont = nullptr;
	OpenGLFont *boldOutlineFont = nullptr;

	float radius = Settings::settings.GetRadius();
	float miniPlayerRadius = Settings::settings.GetMiniPlayerRadius();

	std::optional<std::chrono::system_clock::time_point> hoverTimer = std::nullopt;
	bool hovered = false;
	Vector2i mousePos = { 0, 0 };

	Controls * const controls = nullptr;

	SDL_SystemCursor cursor = SDL_SYSTEM_CURSOR_DEFAULT;

	std::thread embeddedLoadThread;
	bool loadingEmbedded = false;
	std::mutex embeddedLoadingMutex;
	std::vector<uint8_t> embeddedDataToLoad;
	SDL_Surface *embeddedArtToLoad = nullptr;

	std::thread externalLoadThread;
	bool loadingExternal = false;
	std::mutex externalLoadingMutex;
	SDL_Surface *externalArtToLoad = nullptr;
	std::filesystem::path externalArtFile;
	std::string externalFileExtension;
};