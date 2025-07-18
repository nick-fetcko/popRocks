#pragma once

#include <optional>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <functional>
#include <chrono>

#include <SDL_net.h>

#include "MathCPP/Colour.hpp"

#include "Mappings.h"

using namespace std::literals::chrono_literals;

using namespace MathsCPP;

class LightPack {
public:
	// Grab 1 out of every CaptureFreq _frames_
	inline constexpr static uint8_t CaptureFreq = 16;

	// Is there any way we can get this dynamically?
	// The Prismatik API does not provide it
	inline constexpr static uint8_t LightsPerDevice = 10;

	enum class LightType : uint8_t { 
		Intensity, Color, ColorIntensity 
	}; // TODO: Make these flags

	enum class FocusArea : int {
		Bass = 64,
		SubBass = 128,
		BassAndMid = 32, // this was written before "mid" gained a new meaning
		BassMidAndHigh = 16,
		HalfNyquist = 2,
		Nyquist = 1,
		SuperBass = 512, // just referencing a 15-year-old song
	};

	enum class Method {
		RGB,
		HSV
	};

	LightPack();
	~LightPack();
	void OnInit();
	void OnDestroy();

	// This is threadsafe
	void NextSamples(float *samples, std::size_t count);

	// This is NOT threadsafe
	void NextSample(const float &sample);

	void OnLoop(const Colour<float> &color);

	std::optional<std::string> ReadString() const;
	std::optional<std::string> WriteString(std::string str, bool response = true) const;

	std::string GetStringForMassColorChangeCommand(int start, int end, unsigned char r, unsigned char g, unsigned char b) const;

	void SetBufferLength(std::size_t bufferLength);
	void SetLightType(LightType type);
	void SetFocusArea(FocusArea focus);
	void SetMapping(const int *mapping);

	void SetSmooth(uint8_t smooth);
	void SetGamma(float gamma, bool silent = false);

	void SetMethod(Method method);
	void SetSaturationMultiplier(float saturationMultiplier);

private:
	void _OnInit();
	void RetryConnection();
	void _OnLoop();

	bool CanConnect();

	std::atomic<bool> running = false;
	std::thread thread;
	std::queue<std::function<void(LightPack*)>> queue;
	std::mutex mutex;

	uint8_t numberOfLights = 20;

	TCPsocket tcpsock = nullptr;
	SDLNet_SocketSet socketSet = nullptr;

	std::size_t bufferLength = 0;

	// Previous settings
	float previousGamma = 2.0f;
	uint8_t previousSmooth = 100;

	std::size_t binsPerLight = 1;
	double *lightBin = nullptr;
	LightType currentLightType = LightType::Intensity;

	SDL_Color intensityColors[6];
	const int *currentMapping = Mappings::DEFAULT;

	uint8_t captureTimer = 0;
	int currentSample = 0;
	uint8_t currentLight = 1;

	std::vector<double> localMaximums;

	Colour<float> color;

	FocusArea focusArea = FocusArea::BassAndMid;

	Method method = Method::HSV;

	float saturationMultiplier = 1.25f;

	std::chrono::milliseconds sleepTime = 1ms;

	std::condition_variable cond;
	std::mutex condMutex;

	bool firstTry = true;
};