#pragma once

#include "OpenGL/Context.hpp"
#include "OpenGL/OpenGLFont.hpp"

#include "HDR.hpp"
#include "Settings.hpp"
#include "Text.hpp"

class ExclusiveIndicator {
public:
	void OnInit(OpenGLFont *font, OpenGLFont *outlineFont, Context *context) {
		text.OnInit(font, context);
		outline.OnInit(outlineFont, context);

		SetMiniPlayer(miniPlayer);
	}

	const int32_t GetHeight() const { return text.GetSize().y; }
	const int32_t GetWidth() const { return text.GetSize().x; }

	const int32_t GetOutlineWidth() const { return outline.GetBounds().width; }

	void OnLoop(int x, int y, float alpha, const AlbumArt * const albumArt, Context &context) {
		const auto OutlineColor = miniPlayer ? albumArt->GetBlackColor() : (IsExclusive() ? 0.0f : 0.25f * HDR::WhiteLevel);

		pos.x = x;
		pos.y = y - text.GetSize().y / 2;

		context.Color(OutlineColor, OutlineColor, OutlineColor, alpha);
		outline.OnLoop(x, y - text.GetSize().y / 2);

		if (IsExclusive())
			context.Color(HDR::WhiteLevel, HDR::WhiteLevel, HDR::WhiteLevel, alpha);
		else
			context.Color(0.5f * HDR::WhiteLevel, 0.5f * HDR::WhiteLevel, 0.5f * HDR::WhiteLevel, alpha);

		text.OnLoop(x, y - text.GetSize().y / 2);
	}

	void OnDestroy() {
		text.OnDestroy();
		outline.OnDestroy();
	}

	const bool IsExclusive() const { return Settings::settings.GetExclsuive(); }
	void SetExclusive(bool exclusive) { Settings::settings.SetExclusive(exclusive); }

	bool OnMouseClicked(const Vector2i &mousePos) {
		if (mousePos.x >= pos.x &&
			mousePos.x <= pos.x + text.GetSize().x &&
			mousePos.y >= pos.y &&
			mousePos.y <= pos.y + text.GetSize().y) {

			return true;
		}

		return false;
	}

	void SetMiniPlayer(bool miniPlayer) { 
		this->miniPlayer = miniPlayer;

		const auto *label = miniPlayer ? "Ex" : "Exclusive";

		text.SetText(label);
		outline.SetText(label);
	}

private:
	Vector2i pos {0, 0};

	Text text;
	Text outline;

	bool miniPlayer = Settings::settings.GetMiniPlayer();
};