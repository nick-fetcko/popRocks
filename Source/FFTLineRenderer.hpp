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
		const Colour<float> &brightColor,
		float frameCount,
		float maxHeardSample = 0.0f,
		bool resetGain = false
	) override {
		minPoint = std::numeric_limits<float>::max();
		maxPoint = std::numeric_limits<float>::lowest();

		for (int i = 0; i < bufferLength; i++) {
			points[i].x = i * hStep;
			points[i].y = (floatBuffer[i] * 2000);

			if (points[i].y > maxPoint)
				maxPoint = points[i].y;
			if (points[i].y < minPoint)
				minPoint = points[i].y;
		}

		auto [newMin, newMax] = CenterPoints();

		for (int i = 0; i < bufferLength; i++)
			points[i].y += (newMax - newMin) / 2.0;
	}

	void Draw(
		const Delta &time,
		float frameCount,
		const Colour<float> &color,
		const Vector<int, 2> &blurOffset,
		Context &context
	) override {
		context.Use("basic"_hash);

		SetColor(color, 1.0f, context);
		context.Translate(-blurOffset.x / 2.0f - (maxDimension - windowWidth) / 2.0f, -blurOffset.y / 2.0f + windowHeight / 2.0f, 0);

		context.Translate(maxDimension / 2.0f, 0, 0);
		context.Rotate(frameCount, 0.0f, 0.0f, 1.0f);
		context.Translate(-maxDimension / 2.0f, 0, 0);

		// TODO: Allow for this to move around the screen (iTunes style)?
		context.Translate(0, (offset / 2.0f / 100.0f) * windowHeight, 0);

		context.Apply();
		line.SetPoints<Polyline::Join::None>(points, bufferLength);
		line.Draw<false>(context);

		context.Translate(0, -(offset / 2.0f / 100.0f) * windowHeight, 0);
		context.Translate(maxDimension / 2.0f, 0, 0);
		context.Rotate(180, 0, 0, 1);
		context.Translate(-maxDimension / 2.0f, 0, 0);
		context.Translate(0, (offset / 2.0f / 100.0f) * windowHeight, 0);
		context.Apply();
		line.Draw<true>(context);

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