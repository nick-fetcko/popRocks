#include "MiniPlayerList.hpp"

#include "Controls.hpp"
#include "HDR.hpp"

MiniPlayerList::MiniPlayerList(Direction direction, AlbumArt * const albumArt, const bool &vulkan) : direction(direction), albumArt(albumArt), vulkan(vulkan) {
	albumArt->AddColorChangeListener(this);
}

MiniPlayerList::~MiniPlayerList() {
	albumArt->RemoveColorChangeListener(this);
}

void MiniPlayerList::OnInit(OpenGLFont *font, OpenGLFont *boldFont, OpenGLFont *outlineFont, OpenGLFont *boldOutlineFont, Context *context) {
	this->font = font;
	this->boldFont = boldFont;
	this->outlineFont = outlineFont;
	this->boldOutlineFont = boldOutlineFont;
	this->context = context;
}

bool MiniPlayerList::OnResize(int windowWidth, int windowHeight, float scale, bool miniPlayer, float maxWidth) {
	this->windowWidth = windowWidth;
	this->windowHeight = windowHeight;

	for (auto &item : items)
		item.OnResize(windowWidth, windowHeight);

	for (auto &outline : outlines)
		outline.OnResize(windowWidth, windowHeight);

	if ((this->scale != scale || this->miniPlayer != miniPlayer)) {
		for (auto &title : items)
			title.OnInit(font, context);

		for (auto &outline : outlines)
			outline.OnInit(outlineFont, context);

		this->scale = scale;
		this->miniPlayer = miniPlayer;

		MiniPlayerList::SetMiniPlayer(miniPlayer);

		return true;
	}

	return false;
}

void MiniPlayerList::OnDestroy() {
	for (auto &item : items)
		item.OnDestroy();
	for (auto &outline : outlines)
		outline.OnDestroy();

	scrollBar.OnDestroy();
	scrollBarOutline.OnDestroy();
}

void MiniPlayerList::SetMiniPlayer(bool miniPlayer) {
	this->miniPlayer = miniPlayer;
}

void MiniPlayerList::SetMaxWidth(float maxWidth) {
	this->maxWidth = maxWidth;

	for (auto &text : items)
		text.SetMaxWidth(miniPlayer ? maxWidth : windowWidth);

	for (auto &outline : outlines)
		outline.SetMaxWidth(miniPlayer ? maxWidth : windowWidth);
}

void MiniPlayerList::SetAlpha(float alpha) {
	this->alpha = alpha;
}

const OpenGLFont::Bounds &MiniPlayerList::AddItem(const std::string &text, std::optional<std::size_t> index, std::string altText) {
	const auto &black = albumArt->GetBlackColor();

	ScrollingText item(vulkan, true);
	item.OnInit(font, context);
	item.OnResize(windowWidth, windowHeight);
	item.SetText(text);
	item.SetAltText(altText);
	item.SetMaxWidth(miniPlayer ? maxWidth : windowWidth);

	ScrollingText outline(vulkan, true);
	outline.OnInit(outlineFont, context);
	outline.OnResize(windowWidth, windowHeight);
	outline.SetColor({ black, black, black });
	outline.SetText(text);
	outline.SetMaxWidth(miniPlayer ? maxWidth : windowWidth);

	this->outlines.emplace_back(std::move(outline));

	if (index) {
		indices.emplace(std::make_pair(items.size(), *index));
		reverseIndices.emplace(std::make_pair(*index, items.size()));
	} else {
		indices.emplace(std::make_pair(items.size(), items.size()));
		reverseIndices.emplace(std::make_pair(items.size(), items.size()));
	}

	return items.emplace_back(std::move(item)).GetBounds();
}

void MiniPlayerList::Clear() {
	for (auto &title : items)
		title.OnDestroy();

	items.clear();

	for (auto &outline : outlines)
		outline.OnDestroy();

	outlines.clear();
}

bool MiniPlayerList::AddToScrollOffset(int offset) {
	if (!miniPlayer || alpha == 0.0f) return false;

	scrollOffset += offset;

	if (scrollOffset < 0)
		scrollOffset = 0;
	if (items.size() < numberOfVisibleItems)
		scrollOffset = 0;
	else if (scrollOffset + numberOfVisibleItems > items.size())
		scrollOffset = items.size() - numberOfVisibleItems;

	OnRadiusChanged();

	return true;
}

void MiniPlayerList::SetHovered(bool hovered, bool sticky, bool ignoreNextTimeDelta, std::function<void()> afterFade) {
	this->hovered = hovered;
	targetAlpha = hovered ? 1.0f : 0.0f;
	hoverTimer = std::nullopt;

	isHoverSticky = sticky;
	this->ignoreNextTimeDelta = ignoreNextTimeDelta;
	this->afterFade = afterFade;
}

void MiniPlayerList::PreLoop(std::optional<std::size_t> currentIndex) {
	if (!isHoverSticky && hoverTimer && (std::chrono::system_clock::now() - *hoverTimer) >= Settings::settings.GetHoverTime()) {
		// Scroll to currently selected item
		if (!hovered && currentIndex) {
			scrollOffset = std::clamp(reverseIndices[*currentIndex] - numberOfVisibleItems / 2, static_cast<int64_t>(0), static_cast<int64_t>(items.size()) - numberOfVisibleItems);
			OnRadiusChanged();
		}

		hovered = true;
		targetAlpha = 1.0f;
		hoverTimer = std::nullopt;
	}
}

void MiniPlayerList::PostLoop(const Delta &time) {
	if (ignoreNextTimeDelta) {
		ignoreNextTimeDelta = false;
		return;
	}

	if (targetAlpha != this->alpha) {
		if (targetAlpha > this->alpha) {
			this->alpha += time.change.AsSeconds() * targetAlpha * fadeSpeed;

			if (this->alpha > targetAlpha)
				this->alpha = targetAlpha;
		} else {
			this->alpha += time.change.AsSeconds() * (targetAlpha - 1.0) * fadeSpeed;

			if (this->alpha < targetAlpha)
				this->alpha = targetAlpha;
		}
	} else if (afterFade) {
		afterFade();
		afterFade = nullptr;
	}
}

void MiniPlayerList::OnLoop(const Delta &time, Vector2i pos, std::optional<std::size_t> currentIndex) {
	PreLoop(currentIndex);
	OnLoop(time, pos, currentIndex, alpha);
	PostLoop(time);
}

void MiniPlayerList::DeselectCurrent(std::optional<std::size_t> currentIndex) {
	if (currentIndex) {
		items[reverseIndices[*currentIndex]].SetFont(font);
		outlines[reverseIndices[*currentIndex]].SetFont(outlineFont);
	}
}

void MiniPlayerList::OnLoop(const Delta &time, Vector2i pos, std::optional<std::size_t> currentIndex, const float &alpha) {
	if (!hovered && this->alpha == targetAlpha) return;

	this->pos = pos;

	// Bias upwards-facing lists toward the top
	float yOffset = pos.y + (font->GetEm().height * (direction == Direction::Up ? UpwardsBias : 1));
	for (long i = 0; i < numberOfVisibleItems; ++i) {
		const auto index = i + scrollOffset;

		if (index >= items.size())
			continue;
		else if (index < 0)
			continue;

		const auto isCurrent = currentIndex && indices.at(index) == *currentIndex;

		if (isCurrent && outlines[index].GetFont() != boldOutlineFont)
			outlines[index].SetFont(boldOutlineFont);
		else if (!isCurrent && outlines[index].GetFont() == boldOutlineFont)
			outlines[index].SetFont(outlineFont);

		context->Color(1.0f, 1.0f, 1.0f, alpha);

		outlines[index].OnLoop(
			pos.x - outlines[index].GetBounds().width / 2.0f + outlineFont->GetOutlineRadius(),
			yOffset - std::floor(outlines[index].GetBounds().overhang / 3.0f),
			time
		);

		if (isCurrent) {
			if (items[index].GetFont() != boldFont)
				items[index].SetFont(boldFont);
		} else {
			if (items[index].GetFont() == boldFont)
				items[index].SetFont(font);
		}

		if (hovered && i == hoveredOffset)
			context->Color(hoveredColor.r, hoveredColor.g, hoveredColor.b, alpha);
		else
			context->Color(HDR::WhiteLevel, HDR::WhiteLevel, HDR::WhiteLevel, alpha);

		items[index].OnLoop(
			pos.x - items[index].GetBounds().width / 2.0f,
			yOffset - std::floor(items[index].GetBounds().overhang / 3.0f),
			time
		);

		yOffset += items[index].GetBounds().height;
	}

	if (miniPlayer && alpha > 0.0f && items.size() > numberOfVisibleItems) {
		context->Use("basic"_hash);
		context->LoadIdentity();
		context->Translate(windowWidth / 2 - (scrollBarHovered ? baseWidth : 0), windowHeight / 2, 0);
		context->Apply();

		context->Color(albumArt->GetBlackColor(), albumArt->GetBlackColor(), albumArt->GetBlackColor(), 0.6f * alpha);

		context->Blend(true, [this, &alpha] {
			scrollBarOutline.Draw<false>(*context);

			if (scrollBarHandleHovered)
				context->Color(darkColor.r, darkColor.g, darkColor.b, alpha);
			else
				context->Color(hoveredColor.r, hoveredColor.g, hoveredColor.b, alpha);

			scrollBar.Draw<true>(*context);
		});
	}
}

bool MiniPlayerList::OnMouseMoved(const Vector2i &mousePos, Rectanglei bounds, bool justBounds) {
	if (!miniPlayer) return false;

	bool wasScrollBarHovered = scrollBarHovered;

	if (alpha > 0.0f) {
		scrollBarHovered = IsMouseOnScrollbar(mousePos);
		scrollBarHandleHovered = scrollBarHovered && IsMouseOnScrollbarHandle(mousePos);
	} else {
		scrollBarHovered = false;
		scrollBarHandleHovered = false;
	}

	if (scrollBarHovered != wasScrollBarHovered)
		OnRadiusChanged();

	const auto inTriggerX =
		mousePos.x > bounds.x - font->GetEm().width / 2 &&
		mousePos.x < bounds.w + font->GetEm().width / 2;

	const auto inX = justBounds ? inTriggerX : 
		mousePos.x > pos.x - albumArt->GetRadius(miniPlayer) &&
		mousePos.x < pos.x + albumArt->GetRadius(miniPlayer);

	const auto radius = items.size() > numberOfVisibleItems ? 
		albumArt->GetRadius(miniPlayer) :
		// If we have fewer items than can fit,
		// restrict us to the size of said items
		numberOfVisibleItems * font->GetEm().height;

	const auto inY = justBounds ? (mousePos.y > bounds.y && mousePos.y < bounds.h) : (
		(direction == Direction::Down && mousePos.y > bounds.y && mousePos.y < bounds.h + (bounds.h - bounds.y) + radius) ||
		(direction == Direction::Up && mousePos.y < bounds.h && mousePos.y > bounds.y - (bounds.h - bounds.y) - radius)
	);

	hoveredOffset = -1;

	if (inTriggerX &&
		mousePos.y >= bounds.y && mousePos.y <= bounds.h + font->GetEm().height / 4) {
		hoverTimer = std::chrono::system_clock::now();

		return true;
	} else if (hovered && !scrollBarHovered && inX && inY) {
		float yOffset = pos.y + (font->GetEm().height * (direction == Direction::Up ? UpwardsBias : 1));

		for (long i = 0; i < numberOfVisibleItems; ++i) {
			const auto &title = items[i + scrollOffset];

			if (mousePos.x >= pos.x - title.GetBounds().width / 2 && mousePos.x <= pos.x + title.GetBounds().width / 2 &&
				mousePos.y >= yOffset - title.GetBounds().height / 2 && mousePos.y <= yOffset + title.GetBounds().height / 2 + title.GetBounds().overhang) {
				hoveredOffset = i;
				break;
			}

			yOffset += title.GetBounds().height;
		}

		return true;
	} else if (!isHoverSticky && !scrollBarHovered) {
		hovered = false;
		hoverTimer = std::nullopt;
		targetAlpha = 0.0f;
	}

	return false;
}

bool MiniPlayerList::OnMouseClicked(const Vector2i &mousePos, Rectanglei bounds) {
	if (!hovered && mousePos.x >= bounds.x - font->GetEm().width / 2 && mousePos.x <= bounds.w + font->GetEm().width / 2 && mousePos.y >= bounds.y && mousePos.y <= bounds.h + font->GetEm().height / 4) {
		hovered = true;
		targetAlpha = 1.0f;
		hoverTimer = std::nullopt;

		return true;
	}

	return false;
}

inline float MiniPlayerList::GetAngle(const Vector2i &mousePos) const {
	const auto diff = Vector2i{ windowWidth / 2, windowHeight / 2 } - mousePos;
	auto angle = std::atan2(diff.y, diff.x) / Maths::DEG2RAD<float>;

	if (angle < 0) angle = 360 + angle;

	return angle + 90;
}

inline bool MiniPlayerList::IsMouseOnScrollbar(const Vector2i &mousePos) const {
	const auto angle = GetAngle(mousePos);

	const auto distance = Vector2i{ windowWidth / 2, windowHeight / 2 }.Distance(mousePos);

	const auto radius = albumArt->GetRadius(miniPlayer);
	const auto inset = (radius / AlbumArt::BaseRadius) * 12.0f + scrollBarOutline.GetWidth();

	return (angle >= ArcStartAngles[static_cast<uint8_t>(direction)] &&
		angle <= ArcStartAngles[static_cast<uint8_t>(direction)] + ArcWidth &&
		distance >= radius - inset &&
		distance <= radius);
}

inline bool MiniPlayerList::IsMouseOnScrollbarHandle(const Vector2i &mousePos) const {
	const auto [start, end] = GetScrollBarRange();
	const auto angle = GetAngle(mousePos);

	return angle >= ArcStartAngles[static_cast<uint8_t>(direction)] + start && angle <= ArcStartAngles[static_cast<uint8_t>(direction)] + start + end;
}

inline std::pair<double, double> MiniPlayerList::GetScrollBarRange() const {
	const auto end = std::max((ArcWidth - 1) * (static_cast<double>(numberOfVisibleItems) / items.size()), 3.0);

	return {
		((ArcWidth - 1) - end) / (items.size() - numberOfVisibleItems) * scrollOffset,
		end
	};
}

bool MiniPlayerList::OnMouseDown(const Vector2i &mousePos) {
	if (!miniPlayer || alpha == 0.0f) return false;

	if (IsMouseOnScrollbar(mousePos)) {
		const auto angle = GetAngle(mousePos);

		if (IsMouseOnScrollbarHandle(mousePos)) {
			scrolling = true;
			startAngle = angle;
		} else {
			scrollOffset =
				std::clamp(
					static_cast<int>(((angle - ArcStartAngles[static_cast<uint8_t>(direction)]) / ArcWidth) * items.size() - (numberOfVisibleItems / 2 - 1)),
					0,
					static_cast<int>(items.size() - numberOfVisibleItems)
				);

			scrolling = true;
			startAngle = angle;
			scrollBarHandleHovered = true;

			OnRadiusChanged();
		}

		return true;
	}

	return false;
}

bool MiniPlayerList::OnMouseDragged(const Vector2i &mousePos) {
	if (!miniPlayer || alpha == 0.0f) return false;

	if (scrolling) {
		const auto angle = GetAngle(mousePos);
		const auto itemWidth = static_cast<float>(ArcWidth) / items.size();

		const auto delta = angle - startAngle;

		if (delta >= itemWidth || delta <= -itemWidth) {
			const int offset = delta / itemWidth;

			startAngle += itemWidth * offset;
			AddToScrollOffset(offset);
		}

		return true;
	}

	return false;
}

void MiniPlayerList::OnMouseUp(const Vector2i &mousePos, bool updateCache) {
	scrolling = false;

	if (updateCache) {
		for (auto &item : items)
			item.SetText(item.GetText(), true);

		for (auto &outline : outlines)
			outline.SetText(outline.GetText(), true);
	}
}

void MiniPlayerList::PageUp() {
	if (!miniPlayer || alpha == 0.0f) return;

	AddToScrollOffset(numberOfVisibleItems * -1);
}

void MiniPlayerList::PageDown() {
	if (!miniPlayer || alpha == 0.0f) return;

	AddToScrollOffset(numberOfVisibleItems);
}

void MiniPlayerList::Home() {
	if (!miniPlayer || alpha == 0.0f) return;

	scrollOffset = 0;
	OnRadiusChanged();
}

void MiniPlayerList::End() {
	if (!miniPlayer || alpha == 0.0f) return;

	scrollOffset = items.size() - numberOfVisibleItems;
	OnRadiusChanged();
}

void MiniPlayerList::OnRadiusChanged() {
	const auto radius = albumArt->GetRadius(miniPlayer);

	numberOfVisibleItems = std::lround(radius / font->GetEm().height);

	baseWidth = (radius / AlbumArt::BaseRadius) * 8.0f;

	scrollBar.SetWidth(baseWidth * (scrollBarHovered ? 2 : 1));
	scrollBarOutline.SetWidth(baseWidth * (scrollBarHovered ? 4 : 2));

	if (items.empty() || numberOfVisibleItems >= items.size()) {
		numberOfVisibleItems = items.size();
		return;
	}

	const auto inset = (radius / AlbumArt::BaseRadius) * 12.0f;

	std::vector<Vector2f> points(ArcWidth);

	for (auto i = 0; i < ArcWidth; ++i) {
		auto degInRad = (i + ArcStartAngles[static_cast<uint8_t>(direction)]) * Maths::DEG2RAD<float>;

		points[i].x = -sin(degInRad) * (radius - inset);
		points[i].y = cos(degInRad) * (radius - inset);
	}

	scrollBarOutline.SetPoints<Polyline::Join::Miter>(points.data(), ArcWidth);

	const auto [start, end] = GetScrollBarRange();

	for (auto i = 1; i < end; ++i) {
		auto degInRad = (i + start + ArcStartAngles[static_cast<uint8_t>(direction)]) * Maths::DEG2RAD<float>;

		points[i - 1].x = -sin(degInRad) * (radius - inset);
		points[i - 1].y = cos(degInRad) * (radius - inset);
	}

	scrollBar.SetPoints<Polyline::Join::Miter>(
		points.data(),
		end - 1
	);
}

void MiniPlayerList::OnColorChanged(const Colour<float> &color, bool silent) {
	auto hsv = color.ToHsv();

	hsv.v = 1.0f;

	auto hoveredColor = Colour<float>::FromHsv(hsv.h, hsv.s, hsv.v);

	// How close are we to white?
	auto distance =
		std::sqrt(
			std::pow(Colour<float>::White.r - hoveredColor.r, 2) +
			std::pow(Colour<float>::White.g - hoveredColor.g, 2) +
			std::pow(Colour<float>::White.b - hoveredColor.b, 2)
		);

	if (distance > 0.3) {
		if (HDR::Enabled) {
			hoveredColor.Tone(
				Settings::settings.GetAlbumArtGamma(),
				Settings::settings.GetAlbumArtContrast(),
				Settings::settings.GetAlbumArtBrightness(),
				HDR::WhiteLevel * HDR::Headroom
			);
		}

		this->hoveredColor = hoveredColor;

		hsv = this->hoveredColor.ToHsv();
		hsv.v = 0.5f;
		darkColor = Colour<float>::FromHsv(hsv.h, hsv.s, hsv.v);
	} else {
		// If we're too close to white, use grays
		this->hoveredColor = { 0.5f, 0.5f, 0.5f };
		this->darkColor = { 0.25f, 0.25f, 0.25f };
	}
}

void MiniPlayerList::OnBlackChanged(const float &black) {
	for (auto &outline : outlines) {
		outline.SetColor({ black, black, black });
		outline.SetText(outline.GetText(), true, false);
	}
}