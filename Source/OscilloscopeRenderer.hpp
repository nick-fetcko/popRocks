#pragma once

#include "LineRenderer.hpp"

class OscilloscopeRenderer : public LineRenderer {
public:
	OscilloscopeRenderer(
		const DynamicGain<float> *dynamicGain,
		const AlbumArt *albumArt
	);

	OscilloscopeRenderer(Renderer &&right);
	OscilloscopeRenderer(LineRenderer &&right);

	~OscilloscopeRenderer() override;

	bool IsFloatingPoint() const override;

	bool SetBuffer(const uint8_t *const buffer, std::size_t len, bool force = false) override;

	void SetNumberOfChannels(uint8_t numberOfChannels) override;

	void SetWidth(float width) override;

	void OnLoop(
		const Delta &time,
		bool fileLoaded,
		float hStep,
		Context &context,
		const Colour<float> &color,
		const Colour<float> &brightColor,
		float frameCount,
		float maxHeardSample = 0.0f,
		bool resetGain = false,
		bool miniPlayer = false
	) override;

	void Draw(
		const Delta &time,
		float frameCount,
		const Colour<float> &color,
		const Vector<int, 2> &blurOffset,
		Context &context
	) override;

	void Reset() override;

private:
	const short *shortBuffer = nullptr;

	Fetcko::Polyline *channelLines = nullptr;

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