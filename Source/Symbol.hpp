#pragma once

#include "OpenGL/Buffer.hpp"
#include "OpenGL/Context.hpp"
#include "OpenGL/VertexArray.hpp"

#include "AlbumArt.hpp"
#include "AutoFader.hpp"
#include "ColorChangeListener.hpp"
#include "HDR.hpp"

class Symbol : public AutoFader<false>, public ColorChangeListener {
private:
	const inline static auto ClickTime = 100ms;
public:
	Symbol(AlbumArt *const albumArt) : albumArt(albumArt) {
		autoFadeSpeed = 5.0f;
		waitTime = 0s;

		alpha = 0.0f;
	}

	virtual void OnInit(float radius) {
		vao = std::make_unique<VertexArray>();
		vbo = std::make_unique<ArrayBuffer>();

		vao->Bind();
		vbo->Bind();
	}

	virtual void OnResize(float radius) {
		this->radius = radius;

		vao->Bind();
		vbo->Bind();
	}

	virtual void OnLoop(float x, float y, const Delta &time, Context &context, const float *alpha = nullptr) {
		if (clicked) {
			if (auto now = std::chrono::system_clock::now(); now - clickTime > ClickTime)
				clicked = false;
		}
	}

	virtual void OnDestroy() {
		vao.reset();
		vbo.reset();
	}

	void OnColorChanged(const Colour<float> &color, bool silent) override {
		auto hsv = color.ToHsv();
		hsv.v = 0.334f;
		hoveredColor = Colour<float>::FromHsv(hsv.h, hsv.s, hsv.v);
		hsv.v = 1.0f;
		this->color = Colour<float>::FromHsv(hsv.h, hsv.s, hsv.v);
		darkColor = color;

		if (HDR::Enabled) {
			this->color.Tone(
				Settings::settings.GetAlbumArtGamma(),
				Settings::settings.GetAlbumArtContrast(),
				Settings::settings.GetAlbumArtBrightness(),
				HDR::WhiteLevel * HDR::Headroom
			);
		}
	}

	void SetHoveredColor(const Colour<float> &hoveredColor) {
		this->hoveredColor = hoveredColor;
	}
	void SetDarkColor(const Colour<float> &darkColor) {
		this->darkColor = darkColor;
	}

	void SetHovered(bool hovered) {
		this->hovered = hovered;
	}

	const bool &IsHovered() const { return hovered; }

	void SetClicked(bool clicked) {
		this->clicked = clicked;
		clickTime = std::chrono::system_clock::now();
	}

	const bool &IsClicked() const { return clicked; }

	virtual const float GetRadius() const { return radius; }

protected:
	const Colour<float> &GetColor() const {
		return ((hovered && !clicked) ? hoveredColor : (clicked ? darkColor : color));
	}

	float radius = 1.0f;

	Colour<float> color = HDR::WhiteColor;
	Colour<float> hoveredColor = Colour<float>::Grey;
	Colour<float> darkColor = Colour<float>::Grey;

	bool hovered = false;
	bool clicked = false;

	std::unique_ptr<VertexArray> vao;
	std::unique_ptr<ArrayBuffer> vbo;

	std::chrono::system_clock::time_point clickTime;

	const AlbumArt *const albumArt = nullptr;
};