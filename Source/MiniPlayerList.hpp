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
	constexpr static std::array<float, 3> ArcStartAngles = { 233, 239, 239 };

	constexpr static float UpwardsBias = 0.515f;

public:
	enum class Direction : uint8_t {
		Up = 0,
		Down,
		Both
	};

	enum class HoverState : uint8_t {
		None = 0,
		Trigger,
		Hovered
	};

	MiniPlayerList(Direction direction, AlbumArt * const albumArt, const bool &vulkan);
	virtual ~MiniPlayerList();

	virtual void OnInit(OpenGLFont *font, OpenGLFont *boldFont, OpenGLFont *outlineFont, OpenGLFont *boldOutlineFont, Context *context);
	virtual bool OnResize(int windowWidth, int windowHeight, float scale = 1.0f, bool miniPlayer = false, float maxWidth = 0.0f);

	void PreLoop(std::optional<std::size_t> currentIndex = std::nullopt);
	void OnLoop(const Delta &time, Vector2i pos, std::optional<std::size_t> currentIndex);
	virtual void OnLoop(const Delta &time, Vector2i pos, std::optional<std::size_t> currentIndex, const float &alpha);
	virtual float GetItemWidth() const { return 0.0f; }
	virtual float GetItemLeading() const { return 0.0f; }
	virtual bool DrawItem(const Delta &time, std::size_t index, int x, int y, int left, int top, bool hovered, bool clicked, const float &alpha) { return false; }
	void PostLoop(const Delta &time);

	virtual void OnDestroy();

	HoverState OnMouseMoved(const Vector2i &mousePos, Rectanglei bounds, bool justBounds = false);
	const HoverState &GetHoverState() const { return hoverState; }
	virtual bool OnMouseClicked(const Vector2i &mousePos, Rectanglei bounds);
	bool OnMouseDown(const Vector2i &mousePos);
	bool OnMouseDragged(const Vector2i &mousePos);
	virtual void OnMouseUp(const Vector2i &mousePos, bool updateCache);

	void OnRadiusChanged();

	void SetMiniPlayer(bool miniPlayer);
	void SetMaxWidth(float maxWidth);
	void SetAlpha(float alpha);

	const float &GetAlpha() const { return alpha; }

	const OpenGLFont::Bounds &AddItem(const std::string &text, std::optional<std::size_t> index = std::nullopt, std::string altText = "");
	void Clear();

	bool AddToScrollOffset(int offset);

	void OnColorChanged(const Colour<float> &color, bool silent = false) override;
	void OnBlackChanged(const float &black) override;

	const bool &IsHovered() const { return hovered; }
	const bool IsHoveredOrWillBeHovered() const { return hovered || alpha != targetAlpha; }
	const bool IsActive() const { return hovered || hoverTimer || alpha != targetAlpha; }
	void SetHovered(bool hovered, bool sticky, bool ignoreNextTimeDelta, std::function<void()> afterFade = nullptr);
	const Colourf &GetColor() const;
	const Colourf &GetHoveredColor() const { return hoveredColor; }

	const bool Empty() const { return items.empty(); }

	const bool &IsScrolling() const { return scrolling; }
	const bool &IsScrollBarHovered() const { return scrollBarHovered; }

	void PageUp();
	void PageDown();
	void Home();
	void End();

	void DeselectCurrent(std::optional<std::size_t> currentIndex);

	void SetFadeSpeed(float fadeSpeed) { this->fadeSpeed = fadeSpeed; }

protected:
	inline float GetAngle(const Vector2i &mousePos) const;
	inline bool IsMouseOnScrollbar(const Vector2i &mousePos) const;
	inline bool IsMouseOnScrollbarHandle(const Vector2i &mousePos) const;
	inline std::pair<double, double> GetScrollBarRange() const;

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

	float scale = 1.0f;
	bool miniPlayer = Settings::settings.GetMiniPlayer();
	float maxWidth = 0.0f;

	int64_t numberOfVisibleItems = 0;

	int scrollOffset = 0;

	float fadeSpeed = 2.0f;
	bool hovered = false;
	bool isHoverSticky = false;
	bool ignoreNextTimeDelta = false;
	std::optional<std::chrono::system_clock::time_point> hoverTimer = std::nullopt;
	int hoveredOffset = -1;
	Colourf hoveredColor = { 0.5f, 0.5f, 0.5f };
	Colourf darkColor = { 0.25f, 0.25f, 0.25f };

	float alpha = 0.0f;
	float targetAlpha = 0.0f;

	std::vector<ScrollingText> items;
	std::vector<ScrollingText> outlines;

	Fetcko::Polyline scrollBar;
	Fetcko::Polyline scrollBarOutline;

	std::map<std::size_t, std::size_t> indices;
	std::map<std::size_t, int64_t> reverseIndices;

	std::function<void()> afterFade;

	bool scrolling = false;
	float startAngle = 0.0f;
	bool scrollBarHovered = false;
	bool scrollBarHandleHovered = false;

	float baseWidth = 8.0f;

	HoverState hoverState = HoverState::None;
	std::optional<std::chrono::system_clock::time_point> clickTimer = std::nullopt;

	const bool &vulkan;
};