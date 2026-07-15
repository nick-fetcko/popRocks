#pragma once

#include <chrono>

#include <glad/glad.h>

#include "OpenGL/Context.hpp"
#include "OpenGL/OpenGLFont.hpp"

#include "HDR.hpp"
#include "Text.hpp"

using namespace Fetcko;
using namespace std::literals::chrono_literals;

class FPSCounter {
public:
	void OnInit(OpenGLFont *font, OpenGLFont *outlineFont, Context *context) {
		text.OnInit(font, context);
		outline.OnInit(outlineFont, context);
		this->context = context;
		this->font = font;
		this->outlineFont = outlineFont;
	}

	void OnFrame() {
		auto now = std::chrono::steady_clock::now();
		timer += std::chrono::duration_cast<std::chrono::microseconds>(now - lastFrame);

		++frames;

		// Refresh our FPS every second
		if (timer >= 1s) {
			const auto fps = std::to_string(frames) + " FPS";

			outline.SetText(fps);
			text.SetText(fps);

			timer = 0us;
			frames = 0;
		}

		lastFrame = std::move(now);
	}

	void Draw(int x, int y, float alpha) {
		if (!x) x = Margin;
		if (!y) y = Margin + context->GetYOffset();

		context->Color(0.0f, 0.0f, 0.0f, alpha);
		outline.OnLoop(x, y);
		context->Color(HDR::WhiteLevel, HDR::WhiteLevel, HDR::WhiteLevel, alpha);
		text.OnLoop(x, y);
	}

	void OnDestroy() {
		text.OnDestroy();
		outline.OnDestroy();
	}

	Vector2i GetSize() {
		auto ret = text.GetSize();
		ret.x += Margin * 2 + font->GetEm().width / 4.0f;
		ret.y += Margin * 2 + context->GetYOffset() + font->GetEm().height / 4.0f;

		return ret;
	}

	const Text &GetText() { return text; }

private:
	constexpr static int Margin = 12;

	OpenGLFont *font = nullptr;
	OpenGLFont *outlineFont = nullptr;

	Context *context = nullptr;

	std::chrono::steady_clock::time_point lastFrame = std::chrono::steady_clock::now();

	std::chrono::microseconds timer = 0us;
	std::size_t frames = 0;

	Text text;
	Text outline;
};