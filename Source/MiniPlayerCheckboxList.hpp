#pragma once

#include "MiniPlayerList.hpp"

#include "Checkbox.hpp"

class Controls;

class MiniPlayerCheckboxList : public MiniPlayerList {
public:
	MiniPlayerCheckboxList(Direction direction, AlbumArt *const albumArt, Controls *const controls, const bool &vulkan);

	void OnInit(OpenGLFont *font, OpenGLFont *boldFont, OpenGLFont *outlineFont, OpenGLFont *boldOutlineFont, Context *context) override;
	bool OnResize(int windowWidth, int windowHeight, float scale = 1.0f, bool miniPlayer = false, float maxWidth = 0.0f) override;
	void OnDestroy() override;

	const OpenGLFont::Bounds &AddItem(const std::string &text, std::function<bool()> &&get, std::function<void(bool)> &&set, std::optional<std::size_t> index = std::nullopt, std::string altText = "");

	void OnColorChanged(const Colour<float> &color, bool silent = false) override;
	bool OnMouseClicked(const Vector2i &mousePos, Rectanglei bounds) override;

	void OnLoop(const Delta &time, Vector2i pos, std::optional<std::size_t> currentIndex, const float &alpha) override;
	float GetItemWidth() const override;
	float GetItemLeading() const override;
	void DrawItem(const Delta &time, std::size_t index, int x, int y, int left, int top, bool hovered = false) override;

private:
	Controls *const controls = nullptr;

	struct CheckboxSetting {
		Checkbox checkbox;
		std::function<bool()> get;
		std::function<void(bool)> set;
	};

	std::vector<CheckboxSetting> checkboxes;

	Text title;
	Text outline;
};
