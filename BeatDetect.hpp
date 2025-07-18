#pragma once

#include <mutex>
#include <thread>

#include <bass.h>

#include "MathCPP/Duration.hpp"

#include "BeatRootProcessor.h"
#include "CConsole.h"

using namespace MathsCPP;

// In a prior life, these 2 words caused much anxiety
class BeatDetect {
public:
	enum class State {
		Idle,
		Loading,
		Loaded
	};

	void OnLoad(
		HSTREAM streamHandle,
		const DWORD freq,
		const DWORD chans,
		std::function<void()> onLoaded,
		std::optional<double> startTime = std::nullopt,
		std::optional<double> endTime = std::nullopt
	);

	bool OnLoop(double elapsed);

	void SeekTo(double time);

	void ToggleDetection();

	bool IsDetecting() const;

	void Cancel();

	void Reset();

	const State GetState() const;

private:
	inline void _OnLoad(
		HSTREAM streamHandle,
		DWORD freq,
		DWORD chans,
		std::function<void()> onLoaded,
		std::optional<double> startTime = std::nullopt,
		std::optional<double> endTime = std::nullopt,
		std::optional<double> hopTime = std::nullopt,
		std::optional<AgentParameters> parameters = std::nullopt
	);

	inline std::tuple<double, double, double> GetTimeBetweenBeats() const;

	bool detectBpm = false;

	EventList eventList;
	EventList::iterator eventListIter = eventList.end();

	std::thread thread;
	std::atomic<bool> canceled = false;
	std::mutex mutex;

	State state = State::Idle;
};