#pragma once

#include "LineRenderer.hpp"

#include "Hash.hpp"

class FFTLineRenderer : public LineRenderer {
public:
	FFTLineRenderer(
		const DynamicGain<float> *dynamicGain,
		const AlbumArt *albumArt
	);

	FFTLineRenderer(Renderer &&right);
	FFTLineRenderer(LineRenderer &&right);
	FFTLineRenderer(FFTLineRenderer &&right) noexcept;

	~FFTLineRenderer() override;

	bool IsFloatingPoint() const override { return true; }

	bool SetBuffer(const uint8_t *const buffer, std::size_t len, bool force = false) override;

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
	const float *floatBuffer = nullptr;

	float *min = nullptr;
	float *max = nullptr;

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
				if (auto lineRenderer = dynamic_cast<LineRenderer *>(oldRenderer)) {
					if (auto fftLineRenderer = dynamic_cast<FFTLineRenderer *>(lineRenderer))
						ret = new FFTLineRenderer(std::move(*fftLineRenderer));
					else
						ret = new FFTLineRenderer(std::move(*lineRenderer));
				} else {
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