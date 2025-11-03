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

	void Draw(float alpha) {
		context->Color(0.0f, 0.0f, 0.0f, alpha);
		outline.OnLoop(Margin, Margin + context->GetYOffset());
		context->Color(HDR::WhiteLevel, HDR::WhiteLevel, HDR::WhiteLevel, alpha);
		text.OnLoop(Margin, Margin + context->GetYOffset());
	}

	void OnDestroy() {
		text.OnDestroy();
		outline.OnDestroy();
	}

	Vector2i GetSize() {
		auto ret = text.GetSize();
		ret.x += Margin * 2;
		ret.y += Margin * 2 + context->GetYOffset();

		return ret;
	}

private:
	constexpr static int Margin = 12;

	Context *context = nullptr;

	std::chrono::steady_clock::time_point lastFrame = std::chrono::steady_clock::now();

	std::chrono::microseconds timer = 0us;
	std::size_t frames = 0;

	Text text;
	Text outline;
};