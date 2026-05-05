#pragma once

#include <iostream>
#include <optional>
#include <vector>

#include "OpenGL/Polyline.hpp"

#include "AlbumArt.hpp"
#include "Context.hpp"
#include "ScrollingText.hpp"

class MiniPlayerList : public ColorChangeListener, public AlbumArt::BlackChangedListener {
private:
	constexpr static std::size_t ArcWidth = 75;
	constexpr static std::array<float, 2> ArcStartAngles = { 233, 239 };

	constexpr static float UpwardsBias = 0.515f;

public:
	enum class Direction : uint8_t {
		Up = 0,
		Down
	};

	MiniPlayerList(Direction direction, AlbumArt * const albumArt, const bool &vulkan);
	virtual ~MiniPlayerList();

	void OnInit(OpenGLFont *font, OpenGLFont *boldFont, OpenGLFont *outlineFont, OpenGLFont *boldOutlineFont, Context *context);
	void OnResize(int windowWidth, int windowHeight);

	void PreLoop();
	void OnLoop(const Delta &time, Vector2i pos, std::size_t currentIndex);
	void OnLoop(const Delta &time, Vector2i pos, std::size_t currentIndex, const float &alpha);
	void PostLoop(const Delta &time);

	bool OnMouseMoved(const Vector2i &mousePos, Rectanglei bounds);

	void OnRadiusChanged();

	void SetMiniPlayer(bool miniPlayer);
	void SetMaxWidth(float maxWidth);
	void SetAlpha(float alpha);

	const float &GetAlpha() const { return alpha; }

	const OpenGLFont::Bounds &AddItem(const std::string &text, std::optional<std::size_t> index = std::nullopt, std::string altText = "");
	void Clear();

	void AddToScrollOffset(int offset);

	void OnColorChanged(const Colour<float> &color, bool silent = false) override;
	void OnBlackChanged(const float &black) override;

	const bool &IsHovered() const { return hovered; }

	const bool Empty() const { return items.empty(); }

protected:
	Direction direction = Direction::Down;

	AlbumArt * const albumArt = nullptr;
	OpenGLFont *font = nullptr;
	OpenGLFont *boldFont = nullptr;
	OpenGLFont *outlineFont = nullptr;
	OpenGLFont *boldOutlineFont = nullptr;
	Context *context = nullptr;

	int windowWidth = Settings::settings.GetMiniPlayerWidth();
	int windowHeight = Settings::settings.GetMiniPlayerHeight();

	Vector2i pos{ 0, 0 };

	float maxWidth = 0.0f;

	bool miniPlayer = Settings::settings.GetMiniPlayer();

	long numberOfVisibleItems = 0;

	int scrollOffset = 0;

	bool hovered = false;
	std::optional<std::chrono::system_clock::time_point> hoverTimer = std::nullopt;
	int hoveredOffset = -1;
	Colourf hoveredColor;

	float alpha = 0.0f;
	float targetAlpha = 0.0f;

	std::vector<ScrollingText> items;
	std::vector<ScrollingText> outlines;

	Fetcko::Polyline scrollBar;
	Fetcko::Polyline scrollBarOutline;

	std::map<std::size_t, std::size_t> indices;

	const bool &vulkan;
};