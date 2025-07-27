#include "FFTRenderer.hpp"

#include "Hash.hpp"

FFTRenderer::FFTRenderer(
	const DynamicGain<float> *dynamicGain,
	const AlbumArt *albumArt) :
	Renderer(dynamicGain, albumArt),
	indexBuffer(&Buffers::SquareBuffer) {

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

		rects = new float[28 * fullBufferLength];
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
	Context &context,
	const Colour<float> &color,
	float frameCount,
	float maxHeardSample,
	bool resetGain
) {
	auto brightColor = color.ToHsv();
	brightColor.v = 1.0;
	//brightColor.s = 1.0;
	auto brightRgb = Colour<float>::FromHsv(brightColor.h, brightColor.s, brightColor.v);

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
			const Colour<float> *finalColor = &color;
			float alpha = fadeDecays[i].Get();

			if (pulse && fadeDecays[i].WasReset()) {
				finalColor = &brightRgb;
				alpha = static_cast<float>(
					fadeDecays[i].Get() - fadeDecays[i].Get() * (fadeDecays[i].SinceLastReset().AsSeconds() / pulseTime.AsSeconds())
				);
				if (fadeDecays[i].SinceLastReset() >= pulseTime)
					fadeDecays[i].HasBeenReset();
			}

			fadeDecays[i].Update(time);

			float thickness = std::ceil(std::max((albumArt->GetRadius() * Maths::PI<float>) / bufferLength, 1.0f));
			auto angle = (((((static_cast<float>(i) / bufferLength * 360.0f) - frameCount) / distribution) * 360.0f));
			angle *= Maths::DEG2RAD<float>;

			// Top left
			rects[i * 28 + 0] = -thickness;
			rects[i * 28 + 1] = 0.0f;
			rects[i * 28 + 2] = finalColor->r;
			rects[i * 28 + 3] = finalColor->g;
			rects[i * 28 + 4] = finalColor->b;
			rects[i * 28 + 5] = alpha;
			rects[i * 28 + 6] = angle;

			// Bottom Left
			rects[i * 28 + 7] = -thickness;
			rects[i * 28 + 8] = shrinkDecays[i].Get();
			rects[i * 28 + 9] = finalColor->r;
			rects[i * 28 + 10] = finalColor->g;
			rects[i * 28 + 11] = finalColor->b;
			rects[i * 28 + 12] = alpha;
			rects[i * 28 + 13] = angle;

			// Bottom Right
			rects[i * 28 + 14] = thickness;
			rects[i * 28 + 15] = shrinkDecays[i].Get();
			rects[i * 28 + 16] = finalColor->r;
			rects[i * 28 + 17] = finalColor->g;
			rects[i * 28 + 18] = finalColor->b;
			rects[i * 28 + 19] = alpha;
			rects[i * 28 + 20] = angle;

			// Top Right
			rects[i * 28 + 21] = thickness;
			rects[i * 28 + 22] = 0.0f;
			rects[i * 28 + 23] = finalColor->r;
			rects[i * 28 + 24] = finalColor->g;
			rects[i * 28 + 25] = finalColor->b;
			rects[i * 28 + 26] = alpha;
			rects[i * 28 + 27] = angle;
		}
	}

	if (!vao) {
		vao = std::make_unique<VertexArray>();
		vbo = std::make_unique<ArrayBuffer>();
		eab = std::make_unique<ElementBuffer>();

		vao->Bind();
		vbo->Bind();
		vao->AddAttribute(VertexArray::Attribute(0, 2, 7 * sizeof(float)));
		vao->AddAttribute(VertexArray::Attribute(1, 4, 7 * sizeof(float), 2 * sizeof(float)));
		vao->AddAttribute(VertexArray::Attribute(2, 1, 7 * sizeof(float), 6 * sizeof(float)));
		vbo->Unbind();
		vao->Unbind();

		std::vector<unsigned short> indices(bufferLength * 6);
		for (std::size_t i = 0; i < bufferLength; ++i) {
			indices[i * 6] = Buffers::SquareBuffer[0] + (4 * i);
			indices[i * 6 + 1] = Buffers::SquareBuffer[1] + (4 * i);
			indices[i * 6 + 2] = Buffers::SquareBuffer[2] + (4 * i);
			indices[i * 6 + 3] = Buffers::SquareBuffer[3] + (4 * i);
			indices[i * 6 + 4] = Buffers::SquareBuffer[4] + (4 * i);
			indices[i * 6 + 5] = Buffers::SquareBuffer[5] + (4 * i);
		}

		eab->Bind();
		eab->BufferData(indices);
		eab->Unbind();
	}

	vbo->Bind();
	vbo->BufferData(rects, bufferLength * 28, GL_DYNAMIC_DRAW);
	vbo->Unbind();
}

void FFTRenderer::Draw(const Delta &time, float frameCount, const Colour<float> &color, Context &context) {
	context.Use("rotate"_hash);
	context.LoadIdentity();

	vao->Bind();
	eab->Bind();
	eab->DrawElements(GL_TRIANGLES);
	eab->Unbind();
	vao->Unbind();

	context.Use("texture"_hash);
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