#pragma once

#include "PRNG.hpp"

#include <limits>

#include <bass.h>

template<typename T>
class SampleRNG : public PRNG<T> {
public:
	SampleRNG(HSTREAM *streamHandle) : streamHandle(streamHandle) {

	}

	T Next() override {
		BASS_ChannelGetData(*streamHandle, samples, sizeof(samples[0]));

		return static_cast<T>(samples[0]);
	}

	std::pair<T, T> NextTwo() override {
		BASS_ChannelGetData(*streamHandle, samples, sizeof(samples));

		// Left channel, right channel
		return { static_cast<T>(samples[0]), static_cast<T>(samples[1]) };
	}

	T Max() override {
		return static_cast<T>(std::numeric_limits<short>::max());
	}

private:
	static bool registered;

	short samples[2];

	HSTREAM *streamHandle = nullptr;
};