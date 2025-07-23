#pragma once

template<typename T>
struct DynamicGain {
	T largeStep;
	T smallStep;

	T minReset;
	T maxReset;

	bool adjustMin;
	bool adjustMax;

	bool reset;
};