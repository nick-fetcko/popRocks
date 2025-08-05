#pragma once

#include "LineRenderer.hpp"

#include "Hash.hpp"

class FFTLineRenderer : public LineRenderer {
public:
	FFTLineRenderer(
		const DynamicGain<float> *dynamicGain,
		const AlbumArt *albumArt
	) : LineRenderer(dynamicGain, albumArt) {
	}

	FFTLineRenderer(Renderer &&right) : LineRenderer(std::move(right)) {
		SetBuffer(buffer, fullBufferLength);
		SetBufferLength(bufferLength, true);
	}

	FFTLineRenderer(LineRenderer &&right) : LineRenderer(std::move(right)) {
		SetBuffer(buffer, fullBufferLength);
	}

	bool IsFloatingPoint() const override { return true; }

	bool SetBuffer(const uint8_t *const buffer, std::size_t len, bool force = false) override {
		auto ret = Renderer::SetBuffer(buffer, len, force);
		floatBuffer = reinterpret_cast<const float *>(buffer);

		return ret;
	}

	void OnLoop(
		const Delta &time,
		bool fileLoaded,
		float hStep,
		Context &context,
		const Colour<float> &color,
		float frameCount,
		float maxHeardSample = 0.0f,
		bool resetGain = false
	) override {
		minPoint = std::numeric_limits<float>::max();
		maxPoint = std::numeric_limits<float>::lowest();

		for (int i = 0; i < bufferLength; i++) {
			points[i].x = i * hStep;
			points[i].y = -(floatBuffer[i] * 2000);

			if (points[i].y > maxPoint)
				maxPoint = points[i].y;
			if (points[i].y < minPoint)
				minPoint = points[i].y;
		}

		CenterPoints();
	}

	void Draw(
		const Delta &time,
		float frameCount,
		const Colour<float> &color,
		Context &context
	) override {
		context.Use("basic"_hash);

		SetColor(color, 1.0f, context);
		context.Translate(0, windowHeight / 3.0f * 2.0f, 0);
		// TODO: allow the oscilloscope / fft line to rotate
		/*
		glRotatef(
			360.0f - frameCount,
			0.0f,
			0.0f,
			1.0f
		);
		*/
		context.Apply();
		line.SetPoints<Polyline::Join::None>(points, bufferLength);
		line.Draw(context);

		context.Use("texture"_hash);
	}

	void Reset() override {

	}

private:
	const float *floatBuffer = nullptr;

	static inline bool Register() {
		RendererFactory::Register("fftline", [](
			const DynamicGain<float> *dynamicGain,
			const AlbumArt *albumArt,
			Renderer *oldRenderer = nullptr,
			std::optional<int> windowWidth = std::nullopt,
			std::optional<int> windowHeight = std::nullopt,
			uint8_t *buffer = nullptr,
			std::optional<std::size_t> maxLength = std::nullopt,
			std::optional<std::size_t> bufferLength = std::nullopt) {
			Renderer *ret = nullptr;
			if (oldRenderer) {
				if (auto lineRenderer = dynamic_cast<LineRenderer *>(oldRenderer))
					ret = new FFTLineRenderer(std::move(*lineRenderer));
				else {
					// Clean up the line before moving
					oldRenderer->OnDestroy();
					ret = new FFTLineRenderer(std::move(*oldRenderer));
				}

				delete oldRenderer;
			} else if (windowWidth) {
				ret = new FFTLineRenderer(dynamicGain, albumArt);
				ret->OnInit(*windowWidth, *windowHeight);
				ret->SetBuffer(buffer, *maxLength);
				ret->SetBufferLength(*bufferLength, true);
			} else {
				ret = new FFTLineRenderer(dynamicGain, albumArt);
			}

			return ret;
		});

		return true;
	}
	static inline bool registered = Register();
};