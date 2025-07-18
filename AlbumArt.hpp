#pragma once

#include <filesystem>
#include <array>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <SDL_image.h>

#include "MathCPP/Colour.hpp"

#include "ColorChangeListener.hpp"

using namespace MathsCPP;

class AlbumArt {
public:
	static constexpr bool IsSupported(const std::string_view &lowercaseExtension) {
		for (const auto &extension : SupportedExtensions)
			if (lowercaseExtension == extension)
				return true;

		return false;
	}

	enum class ColorMethod { Average, Dominant };

	void OnInit(int windowWidth, int windowHeight, float scale = 1.0f);
	void OnResize(int windowWidth, int windowHeight, float scale = 1.0f);
	void OnLoop(GLfloat x, GLfloat y, float frameCount);
	void OnDestroy();

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

	bool Load(const std::string &mimeType, const void *data, std::size_t length);

	void Reset(const Colour<float> &color);

	void NextBin(bool silent = false);
	void PreviousBin();
	void ResetBin(bool silent = false);

	bool Loaded() const { return albumLoaded; }

	const Colour<float> &GetColor() const { return averageColor; }

	void AddColorChangeListener(ColorChangeListener *listener);
	void RemoveColorChangeListener(ColorChangeListener *listener);

	int DrawSquare(int x, int y, int height, GLfloat alpha);

	const float &GetAspectRatio() const { return aspectRatio; }

	void SetRadius(float radius);
	const float &GetRadius() const { return radius; }

	void Scale(bool force = false);

private:
	constexpr inline static std::array<std::string_view, 3> SupportedExtensions = { ".jpg", ".png", ".webp" };

	std::filesystem::path FindArt(const std::filesystem::path &folder) const;

	// This frees the surface once it's done
	void LoadFromSurface(SDL_Surface *surface, bool scaled = false);

	void UpdateVertexCoords();
	void UpdateTextureCoords();

	void UpdateBin(bool silent = false);
	void PrintBin();

	float radius = 200.0f;

	GLuint album = 0;
	int albumWidth = 0, albumHeight = 0;
	int scaledAlbumWidth = 0, scaledAlbumHeight = 0;
	float albumVertexBuffer[361 * 2] = { 0.0f };
	float squareVertexBuffer[8] = { 0 };
	float albumTexCoords[360 * 2] = { 0.0f };
	unsigned short albumIndexBuffer[360 * 3] = { 0 };
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

	std::vector<Histogram::reverse_iterator> previousBins;

	std::set<ColorChangeListener *> colorChangeListeners;

	std::uint32_t lastHash = 0;
	std::uint32_t lastEmbeddedHash = 0;
	int lastWidth = 0, lastHeight = 0;

	SDL_Surface *lastSurface = nullptr;
	std::atomic<bool> lastSurfaceUpdated = false;
	SDL_Surface *surfaceToLoad = nullptr;

	std::mutex mutex;

	float scale = 1.0f;
};