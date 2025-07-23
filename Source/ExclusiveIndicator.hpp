#pragma once

#include "Settings.hpp"
#include "Text.hpp"

class ExclusiveIndicator {
public:
	void OnInit(TTF_Font *font) {
		text.OnInit(font);
		text.SetText("Exclusive");
	}

	const int32_t GetHeight() const { return text.GetSize().y; }

	void OnLoop(int x, int y, float alpha) {
		pos.x = x;
		pos.y = y - text.GetSize().y / 2;

		if (IsExclusive())
			glColor4f(1.0f, 1.0f, 1.0f, alpha);
		else
			glColor4f(0.5f, 0.5f, 0.5f, alpha);

		text.OnLoop(x, y - text.GetSize().y / 2);
	}

	void OnDestroy() {
		text.OnDestroy();
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

private:
	Vector2i pos {0, 0};

	Text text;
};