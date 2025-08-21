#pragma once

#include <chrono>

#include <glad/glad.h>

#include "OpenGL/Context.hpp"
#include "OpenGL/OpenGLFont.hpp"

#include "Text.hpp"

using namespace Fetcko;
using namespace std::literals::chrono_literals;

class FPSCounter {
public:
	void OnInit(OpenGLFont *font, Context *context) {
		text.OnInit(font, context);
		this->context = context;
	}

	void OnFrame() {
		auto now = std::chrono::steady_clock::now();
		timer += std::chrono::duration_cast<std::chrono::microseconds>(now - lastFrame);

		++frames;

		// Refresh our FPS every second
		if (timer >= 1s) {
			text.SetText(std::to_string(frames) + " FPS");

			timer = 0us;
			frames = 0;
		}

		lastFrame = std::move(now);
	}

	void Draw() {
		text.OnLoop(Margin, Margin + context->GetYOffset());
	}

	void OnDestroy() {
		text.OnDestroy();
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
};