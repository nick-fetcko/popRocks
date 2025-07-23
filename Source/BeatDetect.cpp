#include "BeatDetect.hpp"

void BeatDetect::OnLoad(
	HSTREAM streamHandle,
	const DWORD freq,
	const DWORD chans,
	std::function<void()> onLoaded,
	std::optional<double> startTime,
	std::optional<double> endTime
) {
	canceled = false;

	thread = std::thread([this, streamHandle, freq, chans, onLoaded, startTime, endTime] {
		_OnLoad(streamHandle, freq, chans, onLoaded, startTime, endTime);
	});
}

bool BeatDetect::OnLoop(double elapsed) {
	if (mutex.try_lock()) {
		if (detectBpm && eventListIter != eventList.end() && elapsed >= eventListIter->time) {
			++eventListIter;
			mutex.unlock();
			return true;
		}

		mutex.unlock();
	}

	return false;
}

void BeatDetect::SeekTo(double time) {
	for (eventListIter = eventList.begin(); eventListIter != eventList.end(); ++eventListIter) {
		if (eventListIter->time > time)
			return;
	}
}

void BeatDetect::ToggleDetection() {
	std::unique_lock lock(mutex);
	detectBpm = !detectBpm;
}

bool BeatDetect::IsDetecting() const { return detectBpm; }

void BeatDetect::Cancel() {
	if (!canceled && thread.joinable()) {
		CConsole::Console.Print("Canceling beat detection thread", MSG_DIAG);

		// Only set our state to idle
		// if we actually canceled
		state = State::Idle;
	}

	canceled = true;
	if (thread.joinable())
		thread.join();
}

void BeatDetect::Reset() {
	Cancel();

	state = State::Idle;
	eventList.clear();
	eventListIter = eventList.end();
}

const BeatDetect::State BeatDetect::GetState() const { return state; }

inline void BeatDetect::_OnLoad(
	HSTREAM streamHandle,
	DWORD freq,
	DWORD chans,
	std::function<void()> onLoaded,
	std::optional<double> startTime,
	std::optional<double> endTime,
	std::optional<double> hopTime,
	std::optional<AgentParameters> parameters
) {
	if (detectBpm) {
		std::unique_lock lock(mutex);

		state = State::Loading;

		BeatRootProcessor beatRootProcessor(
			static_cast<float>(freq),
			parameters ? *parameters : AgentParameters()
		);

		if (hopTime)
			beatRootProcessor.setHopTime(*hopTime);

		const auto hopBytes = BASS_ChannelSeconds2Bytes(
			streamHandle,
			beatRootProcessor.getHopTime()
		);

		const auto fftBytes = BASS_ChannelSeconds2Bytes(
			streamHandle,
			beatRootProcessor.getFFTTime()
		);

		QWORD offset = 0;
		if (startTime) {
			offset = BASS_ChannelSeconds2Bytes(
				streamHandle,
				*startTime
			);

			BASS_ChannelSetPosition(
				streamHandle,
				offset,
				BASS_POS_BYTE
			);
		}

		DWORD flags = BASS_DATA_FFT_COMPLEX;

		if (beatRootProcessor.getFFTSize() >= 16384)
			flags |= BASS_DATA_FFT16384;
		else if (beatRootProcessor.getFFTSize() >= 8192)
			flags |= BASS_DATA_FFT8192;
		else if (beatRootProcessor.getFFTSize() >= 4096)
			flags |= BASS_DATA_FFT4096;
		else if (beatRootProcessor.getFFTSize() >= 2048)
			flags |= BASS_DATA_FFT2048;
		else if (beatRootProcessor.getFFTSize() >= 1024)
			flags |= BASS_DATA_FFT1024;
		else if (beatRootProcessor.getFFTSize() >= 512)
			flags |= BASS_DATA_FFT512;
		else if (beatRootProcessor.getFFTSize() >= 256)
			flags |= BASS_DATA_FFT256;
		else
			CConsole::Console.Print("Beatroot is asking for an FFT size of " + std::to_string(beatRootProcessor.getFFTSize()) + ", which isn't supported", MSG_ERROR);

		float **bufferWrapper = new float *[1];
		bufferWrapper[0] = new float[beatRootProcessor.getFFTSize() * 2 /* real and imaginary parts */ * chans];

		auto start = std::chrono::system_clock::now();

		int bytes = BASS_ChannelGetData(streamHandle, bufferWrapper[0], flags);

		QWORD totalBytes = 0;
		while (bytes > 0 &&
			!canceled &&
			(!endTime || (endTime && BASS_ChannelBytes2Seconds(streamHandle, totalBytes) < *endTime))
			) {
			beatRootProcessor.processFrame(bufferWrapper);

			totalBytes += hopBytes;
			BASS_ChannelSetPosition(streamHandle, offset + totalBytes, BASS_POS_BYTE);

			bytes = BASS_ChannelGetData(streamHandle, bufferWrapper[0], flags);
		}

		//auto in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * beatRootProcessor.getFFTSize());
		/*
		auto in = (double *)fftw_malloc(sizeof(double) * beatRootProcessor.getFFTSize());
		auto out = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * beatRootProcessor.getFFTSize());
		auto plan = fftw_plan_dft_r2c_1d(beatRootProcessor.getFFTSize(), in, out, FFTW_ESTIMATE);

		int bytes = BASS_ChannelGetData(streamHandle, sbuffer.floatBuffer, BASS_DATA_FLOAT | fftBytes);
		totalBytes = 0;

		while (bytes > 0) {
			for (auto i = 0; i < beatRootProcessor.getFFTSize(); ++i) {
				in[i] = buffer.floatBuffer[i * info.chans]; // only read a single channel for now
			}

			fftw_execute(plan);

			for (auto i = 0; i < beatRootProcessor.getFFTSize(); ++i) {
				buffer.floatBuffer[i * 2] = out[i][0];
				buffer.floatBuffer[i * 2 + 1] = out[i][1];
			}

			beatRootProcessor.processFrame(bufferWrapper);

			totalBytes += hopBytes;
			BASS_ChannelSetPosition(streamHandle, totalBytes, BASS_POS_BYTE);


			bytes = BASS_ChannelGetData(streamHandle, buffer.floatBuffer, BASS_DATA_FLOAT | fftBytes);
		}

		fftw_free(in);
		fftw_free(out);
		fftw_destroy_plan(plan);
		*/

		delete[] bufferWrapper[0];
		delete[] bufferWrapper;

		if (!canceled) {
			auto end = std::chrono::system_clock::now();

			CConsole::Console.Print("Populating BeatRoot took " + std::to_string(Duration<Microseconds>(end - start).AsSeconds()) + " seconds", MSG_DIAG);

			start = end;

			eventList = beatRootProcessor.beatTrack();

			end = std::chrono::system_clock::now();

			CConsole::Console.Print("BeatRoot processing took " + std::to_string(Duration<Microseconds>(end - start).AsSeconds()) + " seconds", MSG_DIAG);

			// Calculate BPM
			if (!eventList.empty()) {
				auto [min, average, max] = GetTimeBetweenBeats();

				CConsole::Console.Print(
					"Song's estimated BPM is " +
					std::to_string(static_cast<int>(1.0 / average * 60.0)) +
					" based on " +
					std::to_string(eventList.size()) +
					" beats",
					MSG_DIAG
				);

				eventListIter = eventList.begin();
			} else if (!hopTime) {
				// I've only encountered one song (Halestorm's "Scream") that can't be processed
				// with a hopTime of fftTime/2, so this is hopefully just an edge case.
				//
				// Curse of Jinxing strikes again: we can't find beats in Nico Vega's "Beast" even
				// after the retry. Testing with BeatRoot in Audacity showed that setting the expiry
				// time to 100 was required. 95 was tested, but produced spurious beats in the silence
				// between the song and the outro.
				//
				// 3Oh!3's "Photofinnish" also requires a higher expiry time, but 50 is adequate here.
				// Should we try 50 before 100? We'd probably waste too much time at that point. The goal
				// here is to detect beats as quickly as possible.
				CConsole::Console.Print("No beats detected. Trying again with a hop time of 10ms...", MSG_ALERT);

				lock.unlock();

				_OnLoad(streamHandle, freq, chans, onLoaded, startTime ? startTime : 0.0, endTime, 0.010);

				// Return so we don't try to free the stream twice
				return;
			} else if (!parameters) {
				CConsole::Console.Print("No beats detected even with a smaller hop size! Increasing expiry time next...", MSG_ALERT);

				lock.unlock();

				AgentParameters newParameters;
				newParameters.expiryTime = 100.0;
				_OnLoad(streamHandle, freq, chans, onLoaded, startTime ? startTime : 0.0, endTime, 0.010, newParameters);

				// Return so we don't try to free the stream twice
				return;
			} else {
				CConsole::Console.Print("No beats detected with a smaller hop size and larger expiry time!", MSG_ERROR);
			}

			state = State::Loaded;
		} else {
			state = State::Idle;
		}
	}

	BASS_StreamFree(streamHandle);

	// Only call the callback if we're
	// actually detecting and weren't
	// canceled
	if (detectBpm && !canceled)
		onLoaded();

	canceled = true;
}

inline std::tuple<double, double, double> BeatDetect::GetTimeBetweenBeats() const {
	//double averageTimeBetweenBeats = 0.0;
	std::tuple<double, double, double> ret = {
		std::numeric_limits<double>::max(),
		0.0,
		std::numeric_limits<double>::lowest()
	};

	EventList::const_iterator lastEvent = eventList.end();
	for (auto iter = eventList.begin(); iter != eventList.end(); ++iter) {
		if (lastEvent != eventList.end()) {
			auto delta = iter->time - lastEvent->time;

			if (delta < std::get<0>(ret))
				std::get<0>(ret) = delta;
			if (delta > std::get<2>(ret))
				std::get<2>(ret) = delta;

			std::get<1>(ret) += delta;
		}

		lastEvent = iter;
	}

	std::get<1>(ret) = std::get<1>(ret) / (eventList.size() - 1);

	return ret;
}