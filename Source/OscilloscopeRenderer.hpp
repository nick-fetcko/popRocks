#pragma once

#include "LineRenderer.hpp"

class OscilloscopeRenderer : public LineRenderer {
public:
	OscilloscopeRenderer(
		const DynamicGain<float> *dynamicGain,
		const AlbumArt *albumArt
	) : LineRenderer(dynamicGain, albumArt) {
	}

	OscilloscopeRenderer(Renderer &&right) : LineRenderer(std::move(right)) {
		SetBuffer(buffer, fullBufferLength);
		SetBufferLength(bufferLength, true);
	}

	OscilloscopeRenderer(LineRenderer &&right) : LineRenderer(std::move(right)) {
		SetBuffer(buffer, fullBufferLength);
	}

	bool IsFloatingPoint() const override { return false; }

	bool SetBuffer(const uint8_t *const buffer, std::size_t len, bool force = false) override {
		auto ret = Renderer::SetBuffer(buffer, len, force);
		shortBuffer = reinterpret_cast<const short *>(buffer);

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
			for (auto channel = 0; channel < numberOfChannels; ++channel) {
				auto &point = points[channel * (bufferLength / numberOfChannels) + (i / numberOfChannels)];

				point.x = hStep * channel * (bufferLength / numberOfChannels) + i * (hStep / numberOfChannels);
				point.y = shortBuffer[i * numberOfChannels + channel];

				if (point.y > maxPoint)
					maxPoint = point.y;
				if (point.y < minPoint)
					minPoint = point.y;
			}
		}

		CenterPoints();
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

		context.Apply();
		line.SetPoints<Polyline::Join::None>(points, bufferLength);
		line.Draw<true>(context);

		context.Use("texture"_hash);
	}

	void Reset() override {

	}

private:
	const short *shortBuffer = nullptr;

	static inline bool Register() {
		RendererFactory::Register("osc", [](
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
					ret = new OscilloscopeRenderer(std::move(*lineRenderer));
				else {
					oldRenderer->OnDestroy();
					ret = new OscilloscopeRenderer(std::move(*oldRenderer));
				}

				delete oldRenderer;
			} else if (windowWidth) {
				ret = new OscilloscopeRenderer(dynamicGain, albumArt);
				ret->OnInit(*windowWidth, *windowHeight);
				ret->SetBuffer(buffer, *maxLength);
				ret->SetBufferLength(*bufferLength, true);
			} else {
				ret = new OscilloscopeRenderer(dynamicGain, albumArt);
			}

			return ret;
		});

		return true;
	}
	static inline bool registered = Register();
};