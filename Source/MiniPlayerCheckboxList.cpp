#include "MiniPlayerCheckboxList.hpp"

#include "Controls.hpp"

MiniPlayerCheckboxList::MiniPlayerCheckboxList(Direction direction, AlbumArt *const albumArt, Controls *const controls, const bool &vulkan) : 
	MiniPlayerList(direction, albumArt, vulkan),
	controls(controls) {

}

void MiniPlayerCheckboxList::OnInit(OpenGLFont *font, OpenGLFont *boldFont, OpenGLFont *outlineFont, OpenGLFont *boldOutlineFont, Context *context) {
	MiniPlayerList::OnInit(font, boldFont, outlineFont, boldOutlineFont, context);

	title.OnInit(font, context);
	outline.OnInit(outlineFont, context);

	title.SetText("Quick Toggles:");
	outline.SetText("Quick Toggles:");
}

bool MiniPlayerCheckboxList::OnResize(int windowWidth, int windowHeight, float scale, bool miniPlayer, float maxWidth) {
	const auto ret = MiniPlayerList::OnResize(windowWidth, windowHeight, scale, miniPlayer, maxWidth + GetItemWidth());

	for (auto &setting : checkboxes) {
		setting.checkbox.OnResize(controls->GetIconSize());
	}

	return ret;
}

void MiniPlayerCheckboxList::OnDestroy() {
	MiniPlayerList::OnDestroy();

	title.OnDestroy();
	outline.OnDestroy();
}

const OpenGLFont::Bounds &MiniPlayerCheckboxList::AddItem(const std::string &text, std::function<bool()> &&get, std::function<void(bool)> &&set, std::optional<std::size_t> index, std::string altText) {
	Checkbox checkbox(albumArt);

	checkbox.OnInit(font, outlineFont, controls->GetIconSize(), *context);
	checkbox.SetDarkColor(hoveredColor);
	checkbox.SetChecked(get());

	checkboxes.emplace_back(
		CheckboxSetting{
			std::move(checkbox),
			std::move(get),
			std::move(set)
		}
	);

	return MiniPlayerList::AddItem(text, index, altText);
}

void MiniPlayerCheckboxList::OnColorChanged(const Colour<float> &color, bool silent) {
	MiniPlayerList::OnColorChanged(color, silent);

	for (auto &setting : checkboxes)
		setting.checkbox.SetDarkColor(hoveredColor);
}

bool MiniPlayerCheckboxList::OnMouseClicked(const Vector2i &mousePos, Rectanglei bounds) {
	if (miniPlayer && alpha > 0.0f) {
		if (hoveredOffset > -1 && hoveredOffset < checkboxes.size()) {
			auto &setting = checkboxes.at(hoveredOffset);

			const auto newSetting = !setting.checkbox.GetChecked();

			setting.checkbox.SetChecked(newSetting);
			setting.set(newSetting);
		} else if (hovered && alpha == 1.0f && alpha == targetAlpha) {
			hovered = false;
			hoverTimer = std::nullopt;
			targetAlpha = 0.0f;

			clickTimer = std::chrono::system_clock::now();
		}

		return true;
	}

	return MiniPlayerList::OnMouseClicked(mousePos, bounds);
}

void MiniPlayerCheckboxList::OnLoop(const Delta &time, Vector2i pos, std::optional<std::size_t> currentIndex, const float &alpha) {
	if (!hovered && this->alpha == targetAlpha) return;

	context->Color(0.0f, 0.0f, 0.0f, alpha);
	outline.OnLoop(pos.x - title.GetBounds().width / 2, pos.y - title.GetBounds().height / 2);
	context->Color(1.0f, 1.0f, 1.0f, alpha);
	title.OnLoop(pos.x - title.GetBounds().width / 2, pos.y - title.GetBounds().height / 2);

	MiniPlayerList::OnLoop(time, pos, currentIndex, alpha);
}

float MiniPlayerCheckboxList::GetItemWidth() const {
	if (checkboxes.empty()) return 0.0f;
	else return checkboxes.begin()->checkbox.GetSize().x * 1.25f;
}

float MiniPlayerCheckboxList::GetItemLeading() const {
	if (checkboxes.empty()) return 0.0f;
	else return std::round(checkboxes.begin()->checkbox.GetSize().y / 10.0f);
}

void MiniPlayerCheckboxList::DrawItem(const Delta &time, std::size_t index, int x, int y, int left, int top, bool hovered) {
	auto &setting = checkboxes.at(index);

	setting.checkbox.SetHovered(hovered);
	setting.checkbox.OnLoop(
		x + left - setting.checkbox.GetSize().x * 1.25f,
		y + top + setting.checkbox.GetSize().y / 4,
		time,
		*context,
		&alpha
	);
}
