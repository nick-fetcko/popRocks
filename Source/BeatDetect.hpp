#pragma once

#include <mutex>
#include <optional>
#include <thread>

#include <bass.h>

#include "MathCPP/Duration.hpp"

#include "Utils/Logger.hpp"

#include "BeatRootProcessor.h"
#include "Settings.hpp"

using namespace MathsCPP;
using namespace Fetcko;

// In a prior life, these 2 words caused much anxiety
class BeatDetect : public LoggableClass {
public:
	enum class State {
		Idle,
		Loading,
		Loaded
	};

	void OnLoad(
		const std::filesystem::path &path,
		bool cache,
		HSTREAM streamHandle,
		const DWORD freq,
		const DWORD chans,
		std::function<void()> onLoaded,
		std::optional<double> startTime = std::nullopt,
		std::optional<double> endTime = std::nullopt
	);

	bool OnLoop(double elapsed);
	float NextBeatTime() const { return eventListIter == eventList.end() ? 0.0f : eventListIter->time; }

	int SeekTo(double time);
	const int GetNumberOfElapsedBeats() const;

	const bool IsNextBeatCloser(double elapsed) const;

	void SetDetecting(bool detecting);

	bool IsDetecting() const;

	void Cancel();

	void Reset();

	const State GetState() const;

private:
	inline void _OnLoad(
		const std::filesystem::path &path,
		bool cache,
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

	bool detectBpm = Settings::settings.GetDetectBpm();

	EventList eventList;
	EventList::iterator eventListIter = eventList.end();

	std::thread thread;
	std::atomic<bool> canceled = false;
	std::mutex mutex;

	State state = State::Idle;
};