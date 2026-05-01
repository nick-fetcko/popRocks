#include "MiniPlayerList.hpp"

#include "Controls.hpp"
#include "HDR.hpp"

MiniPlayerList::MiniPlayerList(Direction direction, AlbumArt * const albumArt) : direction(direction), albumArt(albumArt) {
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

void MiniPlayerList::OnResize(int windowWidth, int windowHeight) {
	this->windowWidth = windowWidth;
	this->windowHeight = windowHeight;
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

	ScrollingText item;
	item.OnInit(font, context);
	item.SetText(text);
	item.SetAltText(altText);
	item.SetMaxWidth(miniPlayer ? maxWidth : windowWidth);

	ScrollingText outline;
	outline.OnInit(outlineFont, context);
	outline.SetColor({ black, black, black });
	outline.SetText(text);
	outline.SetMaxWidth(miniPlayer ? maxWidth : windowWidth);

	this->outlines.emplace_back(std::move(outline));

	if (index)
		indices.emplace(std::make_pair(items.size(), *index));
	else
		indices.emplace(std::make_pair(items.size(), items.size()));

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

void MiniPlayerList::AddToScrollOffset(int offset) {
	if (!miniPlayer || alpha == 0.0f) return;

	scrollOffset += offset;

	if (scrollOffset < 0)
		scrollOffset = 0;
	if (items.size() < numberOfVisibleItems)
		scrollOffset = 0;
	else if (scrollOffset + numberOfVisibleItems > items.size())
		scrollOffset = items.size() - numberOfVisibleItems;

	OnRadiusChanged();
}

void MiniPlayerList::PreLoop() {
	if (hoverTimer && (std::chrono::system_clock::now() - *hoverTimer) >= Settings::settings.GetHoverTime()) {
		hovered = true;
		targetAlpha = 1.0f;
		hoverTimer = std::nullopt;
	}
}

void MiniPlayerList::PostLoop(const Delta &time) {
	if (targetAlpha != this->alpha) {
		if (targetAlpha > this->alpha) {
			this->alpha += time.change.AsSeconds() * targetAlpha * 2;

			if (this->alpha > targetAlpha)
				this->alpha = targetAlpha;
		} else {
			this->alpha += time.change.AsSeconds() * (targetAlpha - 1.0) * 2;

			if (this->alpha < targetAlpha)
				this->alpha = targetAlpha;
		}
	}
}

void MiniPlayerList::OnLoop(const Delta &time, Vector2i pos, std::size_t currentIndex) {
	PreLoop();
	OnLoop(time, pos, currentIndex, alpha);
	PostLoop(time);
}

void MiniPlayerList::OnLoop(const Delta &time, Vector2i pos, std::size_t currentIndex, const float &alpha) {
	if (!hovered && this->alpha == targetAlpha) return;

	this->pos = pos;

	float yOffset = pos.y + font->GetEm().height;
	for (long i = 0; i < numberOfVisibleItems; ++i) {
		const auto index = i + scrollOffset;

		if (index >= items.size())
			continue;
		else if (index < 0)
			continue;

		const auto isCurrent = indices.at(index) == currentIndex;

		if (isCurrent && outlines[index].GetFont() != boldOutlineFont)
			outlines[index].SetFont(boldOutlineFont);
		else if (!isCurrent && outlines[index].GetFont() == boldOutlineFont)
			outlines[index].SetFont(outlineFont);

		context->Color(1.0f, 1.0f, 1.0f, alpha);

		outlines[index].OnLoop(pos.x - outlines[index].GetBounds().width / 2.0f + outlineFont->GetOutlineRadius(), yOffset, time);

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

		items[index].OnLoop(pos.x - items[index].GetBounds().width / 2.0f, yOffset, time);

		yOffset += items[index].GetBounds().height;
	}

	if (miniPlayer && alpha > 0.0f && items.size() > numberOfVisibleItems) {
		context->Use("basic"_hash);
		context->LoadIdentity();
		context->Translate(windowWidth / 2, windowHeight / 2, 0);
		context->Apply();

		context->Color(albumArt->GetBlackColor(), albumArt->GetBlackColor(), albumArt->GetBlackColor(), 0.6f * alpha);
		scrollBarOutline.Draw<false>(*context);

		context->Color(hoveredColor.r, hoveredColor.g, hoveredColor.b, alpha);
		scrollBar.Draw<true>(*context);
	}
}

bool MiniPlayerList::OnMouseMoved(const Vector2i &mousePos, Rectanglei bounds) {
	if (!miniPlayer) return false;

	const auto inTriggerX =
		mousePos.x > bounds.x &&
		mousePos.x < bounds.w;

	const auto inX =
		mousePos.x > pos.x - albumArt->GetRadius(miniPlayer) * Controls::MiniPlayerSeekbarRatio / 2 &&
		mousePos.x < pos.x + albumArt->GetRadius(miniPlayer) * Controls::MiniPlayerSeekbarRatio / 2;

	const auto radius = albumArt->GetRadius(miniPlayer);

	const auto inY = (
		(direction == Direction::Down && mousePos.y > bounds.y && mousePos.y < bounds.h + (bounds.h - bounds.y) + radius) ||
		(direction == Direction::Up && mousePos.y < bounds.h && mousePos.y > bounds.y - (bounds.h - bounds.y) - radius)
	);

	hoveredOffset = -1;

	if (inTriggerX &&
		mousePos.y >= bounds.y && mousePos.y <= bounds.h) {
		hoverTimer = std::chrono::system_clock::now();

		return true;
	} else if (hovered && inX && inY) {
		float yOffset = pos.y + font->GetEm().height;

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
	} else {
		hovered = false;
		hoverTimer = std::nullopt;
		targetAlpha = 0.0f;
	}

	return false;
}

void MiniPlayerList::OnRadiusChanged() {
	const auto radius = albumArt->GetRadius(miniPlayer);

	numberOfVisibleItems = std::lround(radius / font->GetEm().height);

	scrollBar.SetWidth((radius / AlbumArt::BaseRadius) * 8.0f);
	scrollBarOutline.SetWidth((radius / AlbumArt::BaseRadius) * 16.0f);

	if (items.empty() || numberOfVisibleItems >= items.size()) {
		numberOfVisibleItems = items.size();
		return;
	}

	const auto inset = (radius / AlbumArt::BaseRadius) * 12.0f;

	std::vector<Vector2f> points(ArcWidth);

	for (auto i = 0; i < ArcWidth; ++i) {
		auto degInRad = (i + ArcStartAngle) * Maths::DEG2RAD<float>;

		points[i].x = -sin(degInRad) * (radius - inset);
		points[i].y = cos(degInRad) * (radius - inset);
	}

	scrollBarOutline.SetPoints<Polyline::Join::Miter>(points.data(), ArcWidth);

	const auto end = (ArcWidth - 1) * (static_cast<double>(numberOfVisibleItems) / items.size());
	const auto start = ((ArcWidth - 1) - end) / (items.size() - numberOfVisibleItems) * scrollOffset;

	for (auto i = 1; i < end; ++i) {
		auto degInRad = (i + start + ArcStartAngle) * Maths::DEG2RAD<float>;

		points[i - 1].x = -sin(degInRad) * (radius - inset);
		points[i - 1].y = cos(degInRad) * (radius - inset);
	}

	scrollBar.SetPoints<Polyline::Join::Miter>(points.data(), (ArcWidth - 1) * (static_cast<double>(numberOfVisibleItems) / items.size()) - 1);
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
	}
	else this->hoveredColor = { 0.5f, 0.5f, 0.5f };
}

void MiniPlayerList::OnBlackChanged(const float &black) {
	for (auto &outline : outlines) {
		outline.SetColor({ black, black, black });
		outline.SetText(outline.GetText(), true, false);
	}
}