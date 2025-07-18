#include "FFTRenderer.hpp"

FFTRenderer::FFTRenderer(
	const DynamicGain<float> *dynamicGain,
	const AlbumArt *albumArt) :
	Renderer(dynamicGain, albumArt),
	indexBuffer(&Buffer::SquareBuffer) {

}

FFTRenderer::FFTRenderer(Renderer &&right) : Renderer(std::move(right)) {
	SetBuffer(buffer, fullBufferLength, true);
	SetBufferLength(bufferLength, true);
}

FFTRenderer::~FFTRenderer() {
	delete[] rects;
	delete[] shrinkDecays;
	delete[] fadeDecays;

	delete[] min;
	delete[] max;
}

bool FFTRenderer::SetBuffer(const uint8_t *const buffer, std::size_t len, bool force) {
	auto changed = Renderer::SetBuffer(buffer, len, force);
	floatBuffer = reinterpret_cast<const float *>(buffer);

	if (changed) {
		fullBufferLength = len;

		delete[] rects;
		delete[] shrinkDecays;
		delete[] fadeDecays;

		delete[] min;
		delete[] max;

		rects = new float[8 * fullBufferLength];
		shrinkDecays = new Decay[fullBufferLength];
		fadeDecays = new Decay[fullBufferLength];

		min = new float[fullBufferLength];
		max = new float[fullBufferLength];

		std::memset(min, 0, sizeof(float) * fullBufferLength);
		std::memset(max, 0, sizeof(float) * fullBufferLength);
	}

	for (auto i = 0; i < fullBufferLength; ++i) {
		shrinkDecays[i].SetTime(decayTime);
		fadeDecays[i].SetTime(fadeDecayTime);

		if (changed) {
			if (dynamicGain->adjustMin)
				min[i] = dynamicGain->minReset;

			if (dynamicGain->adjustMax)
				max[i] = dynamicGain->maxReset;
		}
	}

	return changed;
}

void FFTRenderer::SetBufferLength(std::size_t bufferLength, bool changed) {
	if (changed)
		Renderer::SetBufferLength(bufferLength, changed);
}

void FFTRenderer::OnLoop(
	const Delta &time,
	bool fileLoaded,
	float hStep,
	float maxHeardSample,
	bool resetGain
) {
	std::size_t maxUpdates = 0;

	for (int i = 0; i < fullBufferLength; i++) {
		//rects[i * 4] = i*hStep;
		//rects[i * 4 + 1] = SCREEN_HEIGHT;
		//if(fileLoaded) rects[i * 4 + 3] = rects[i * 4 + 1] - (buffer.floatBuffer[i]*5000);
		//else rects[i * 4 + 3] = -out[0][i]*10.0f;
		//rects[i].h = buffer[i]/10000000;
		//rects[i].w = 10;
		//rects[i * 4 + 2] = rects[i * 4] + 1;

		//auto value = (buffer.floatBuffer[i] * 2500.0f) * gain;
		//auto value = (buffer.floatBuffer[i] * 2500.0f) * (static_cast<float>(i) / bufferLength) * gain;

		auto rawValue = floatBuffer[i];

		if (resetGain) {
			if (dynamicGain->adjustMin)
				min[i] = rawValue;

			if (dynamicGain->adjustMax)
				max[i] = rawValue;
		}

		if (dynamicGain->adjustMin) {
			if (rawValue < min[i])
				min[i] -= dynamicGain->largeStep;
			else
				min[i] += dynamicGain->smallStep;
		}
		if (dynamicGain->adjustMax) {
			if (rawValue > max[i])
				max[i] += dynamicGain->largeStep;
			else
				max[i] -= dynamicGain->smallStep;
		}

		auto height = (std::max(windowWidth, windowHeight) / 2.0f - albumArt->GetRadius() / 2.0f);

		// If we're listening, skip normalization
		auto scaledValue =
			fileLoaded ?
			((rawValue - min[i]) / (max[i] - min[i])) * height :
			(rawValue / maxHeardSample) * height;

		shrinkDecays[i].Update(time);
		if (scaledValue > shrinkDecays[i].Get()) {
			// Ignore higher-frequency bins
			if (i < bufferLength / 2)
				++maxUpdates;

			shrinkDecays[i].Reset(scaledValue);
			fadeDecays[i].Reset(1.0f);
		}
		//values[i] = value; // BYPASS DECAYS

		//if (value > SCREEN_HEIGHT / 4.0f)
		//	value = SCREEN_HEIGHT / 4.0f;

		// Only update the rectangles we're actively rendering
		if (i < bufferLength) {
			float thickness = std::ceil(std::max((albumArt->GetRadius() * Maths::PI<float>) / bufferLength, 1.0f));

			rects[i * 8 + 0] = -thickness;
			rects[i * 8 + 1] = 0;
			rects[i * 8 + 2] = -thickness;
			rects[i * 8 + 3] = shrinkDecays[i].Get();
			rects[i * 8 + 4] = thickness;
			rects[i * 8 + 5] = shrinkDecays[i].Get();
			rects[i * 8 + 6] = thickness;
			rects[i * 8 + 7] = 0;
		}
	}
}

void FFTRenderer::Draw(const Delta &time, float frameCount, const Colour<float> &color) {
	auto brightColor = color.ToHsv();
	brightColor.v = 1.0;
	//brightColor.s = 1.0;
	auto brightRgb = Colour<float>::FromHsv(brightColor.h, brightColor.s, brightColor.v);

	for (int i = 0; i < bufferLength; i++) {
		glVertexPointer(2, GL_FLOAT, 0, &rects[i * 8]);

		glEnableClientState(GL_VERTEX_ARRAY);
		glTranslatef(windowWidth / 2.0f, windowHeight / 2.0f, 0.0f);
		auto angle = ((((static_cast<float>(i) / bufferLength * 360.0f) - frameCount) / distribution) * 360.0f);
		glTranslatef(
			albumArt->GetRadius() * sin(angle * Maths::DEG2RAD<float>),
			albumArt->GetRadius() * cos(angle * Maths::DEG2RAD<float>),
			0.0f
		);
		glRotatef(
			360.0f - angle,
			xRot ? 1.0f : 0.0f,
			yRot ? 1.0f : 0.0f,
			zRot ? 1.0f : 0.0f
		);

		fadeDecays[i].Update(time);

		SetColor(color, fadeDecays[i].Get());
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indexBuffer->data());

		// FIXME: Rendering a _copy_ of the rectangle is not efficient
		if (pulse && fadeDecays[i].WasReset()) {
			SetColor(
				brightRgb,
				static_cast<float>(
					fadeDecays[i].Get() - fadeDecays[i].Get() * (fadeDecays[i].SinceLastReset().AsSeconds() / pulseTime.AsSeconds())
				)
			);
			
			if (fadeDecays[i].SinceLastReset() >= pulseTime)
				fadeDecays[i].HasBeenReset();

			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indexBuffer->data());
		}

		glDisableClientState(GL_VERTEX_ARRAY);

		glLoadIdentity();
	}
}

void FFTRenderer::Reset() {
	for (auto i = 0; i < bufferLength; ++i) {
		min[i] = dynamicGain->minReset;
		max[i] = dynamicGain->maxReset;

		fadeDecays[i].Reset(0.0f);
		shrinkDecays[i].Reset(0.0f);
	}
}

void FFTRenderer::PrintMax() const {
	std::stringstream stream;

	float max = std::numeric_limits<float>::lowest();
	for (auto i = 0; i < bufferLength; ++i) {
		if (this->max[i] > max)
			max = this->max[i];
	}

	stream << "max = " << max;
	CConsole::Console.Print(stream.str(), MSG_DIAG);
}