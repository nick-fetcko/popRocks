#pragma once

#include <algorithm>
#include <sstream>

#include <glad/glad.h>

#include "MathCPP/Maths.hpp"
#include "MathCPP/Duration.hpp"

#include "Buffer.hpp"
#include "CConsole.h"
#include "Renderer.hpp"

using namespace MathsCPP;

class FFTRenderer : public Renderer {
public:
	FFTRenderer(
		const DynamicGain<float> *dynamicGain,
		const AlbumArt *albumArt
	);

	FFTRenderer(Renderer &&right);

	void OnDestroy() override;

	virtual ~FFTRenderer();

	bool IsFloatingPoint() const override { return true; }

	bool SetBuffer(const uint8_t *const buffer, std::size_t len, bool force = false) override;

	void SetBufferLength(std::size_t bufferLength, bool changed) override;

	void OnLoop(
		const Delta &time,
		bool fileLoaded,
		float hStep,
		Context &context,
		const Colour<float> &color,
		float frameCount,
		float maxHeardSample = 0.0f,
		bool resetGain = false
	) override;

	void Draw(
		const Delta &time,
		float frameCount,
		const Colour<float> &color,
		Context &context
	) override;

	void Reset() override;

	void PrintMax() const;

	void SetDistribution(float dist) { distribution = dist; }
	void ToggleXRot() { xRot = !xRot; }
	void ToggleYRot() { yRot = !yRot; }
	void ToggleZRot() { zRot = !zRot; }
	void SetIndexBuffer(const std::array<unsigned short, 6> *buffer) {
		this->indexBuffer = buffer;
	}

	void SetDecayTime(Duration<Microseconds> time) {
		decayTime = time;

		for (auto i = 0; i < fullBufferLength; ++i)
			shrinkDecays[i].SetTime(decayTime);
	}

	void SetFadeTime(Duration<Microseconds> time) {
		fadeDecayTime = time;

		for (auto i = 0; i < fullBufferLength; ++i)
			fadeDecays[i].SetTime(fadeDecayTime);
	}

private:
	const std::array<unsigned short, 6> *indexBuffer = &Buffers::SquareBuffer;

	const float *floatBuffer = nullptr;

	// "Percentage over time"
	// Actually, it's LITERALLY a decay
	// "How many seconds does it take to go from 1.0 to 0.0"
	Duration<Microseconds> fadeDecayTime = 0.5s;
	Duration<Microseconds> decayTime = 0.5s;

	float *rects = nullptr;

	Decay *shrinkDecays = nullptr;
	Decay *fadeDecays = nullptr;

	float *min = nullptr;
	float *max = nullptr;

	float distribution = 360.0f;

	bool xRot = false;
	bool yRot = false;
	bool zRot = true;

	std::unique_ptr<VertexArray> vao;
	std::unique_ptr<ArrayBuffer> vbo;
	std::unique_ptr<ElementBuffer> eab;
};