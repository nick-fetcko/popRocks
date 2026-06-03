#include "BeatDetect.hpp"

#include "Utils/Filesystem.hpp"
#include "Utils/Hash.hpp"
#include "Utils/Utils.hpp"

void BeatDetect::OnLoad(
	const std::filesystem::path &path,
	bool cache,
	HSTREAM streamHandle,
	const DWORD freq,
	const DWORD chans,
	std::function<void()> onLoaded,
	std::optional<double> startTime,
	std::optional<double> endTime,
	std::optional<uint8_t> index
) {
	canceled = false;

	thread = std::thread([this, path, cache, streamHandle, freq, chans, onLoaded, startTime, endTime, index] {
		_OnLoad(path, cache, streamHandle, freq, chans, onLoaded, startTime, endTime, index);
	});
}

bool BeatDetect::OnLoop(double elapsed) {
	if (mutex.try_lock()) {
		if (detectBpm && eventListIter != eventList.end() && elapsed >= eventListIter->time) {
			++eventListIter;
			if (Settings::settings.GetHalveBpm()) ++eventListIter;
			mutex.unlock();
			return true;
		}

		mutex.unlock();
	}

	return false;
}

int BeatDetect::SeekTo(double time) {
	int ret = 0;
	for (eventListIter = eventList.begin(); eventListIter != eventList.end(); ++eventListIter, ++ret) {
		if (eventListIter->time > time)
			return ret;
	}

	return ret;
}

const int BeatDetect::GetNumberOfElapsedBeats() const {
	return std::distance(eventList.begin(), EventList::const_iterator(eventListIter));
}

const bool BeatDetect::IsNextBeatCloser(double elapsed) const {
	if (eventListIter == eventList.end()) return true;

	auto tempIter = eventListIter;
	if (tempIter != eventList.begin())
		std::advance(tempIter, -1);

	return eventListIter->time - elapsed < elapsed - tempIter->time;
}

void BeatDetect::SetDetecting(bool detecting) {
	if (!detecting) Cancel();

	std::unique_lock lock(mutex);
	detectBpm = detecting;
}

bool BeatDetect::IsDetecting() const { return detectBpm; }

void BeatDetect::Cancel() {
	if (!canceled && thread.joinable()) {
		LogDebug("Canceling beat detection thread");

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
	const std::filesystem::path &path,
	bool cache,
	HSTREAM streamHandle,
	DWORD freq,
	DWORD chans,
	std::function<void()> onLoaded,
	std::optional<double> startTime,
	std::optional<double> endTime,
	std::optional<uint8_t> index,
	std::optional<double> hopTime,
	std::optional<AgentParameters> parameters
) {
	std::filesystem::path cachePath;
	bool collision = false;
	if (detectBpm) {
		std::unique_lock lock(mutex);

		// Do we have cached results?
		if (cache) {
			auto start = std::chrono::system_clock::now();

			auto data = Utils::GetStringFromFile(path);
			hash = hash_64_fnv1a_const(data.data(), data.size());
			std::stringstream stream;
			stream << std::setw(sizeof(hash) * 2) << std::setfill('0') << std::uppercase << std::hex << hash;
			if (index) stream << std::dec << "-" << static_cast<int>(*index);
			if (auto cacheFolder = Filesystem::GetPath("cache/"); !std::filesystem::exists(cacheFolder))
				std::filesystem::create_directory(cacheFolder);

			cachePath = Filesystem::GetPath("cache/" + stream.str());
			if (std::filesystem::exists(cachePath)) {
				std::ifstream inFile(cachePath, std::ios::in | std::ios::binary);
				eventList.clear();

				double nan = 0.0;
				inFile.read(reinterpret_cast<char *>(&nan), sizeof(double));

				if (std::isnan(nan)) {
					std::string::size_type size = 0;
					inFile.read(reinterpret_cast<char *>(&size), sizeof(std::string::size_type));

					char *filenameBytes = new char[size];
					inFile.read(filenameBytes, size);

					std::string filename(filenameBytes, filenameBytes + size);

					delete[] filenameBytes;

					if (auto current = path.filename().u8string(); current != filename) {
						LogWarning(
							"Collision detected in cache! Hash matches filename of \"",
							filename,
							"\" while current filename is \"",
							current,
							"\""
						);

						collision = true;
					}

				} else {
					inFile.seekg(-sizeof(double), std::ios::cur);
				}

				// If we have a collision, we want
				// to ignore what's in the cache.
				if (!collision) {
					while (inFile) {
						Event event;
						inFile.read(reinterpret_cast<char *>(&event), sizeof(Event));
						eventList.emplace_back(std::move(event));
					}
					eventListIter = eventList.begin();
					state = State::Loaded;
					canceled = true;

					auto end = std::chrono::system_clock::now();

					LogDebug("Loading BeatRoot cache took ", Duration<Microseconds>(end - start).AsSeconds(), " seconds");

					onLoaded();

					return;
				}
			}
		}

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
			LogError("Beatroot is asking for an FFT size of ", beatRootProcessor.getFFTSize(), ", which isn't supported");

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

			LogDebug("Populating BeatRoot took ", Duration<Microseconds>(end - start).AsSeconds(), " seconds");

			start = end;

			eventList = beatRootProcessor.beatTrack(canceled);

			if (eventList.size() % 2) {
				eventList.erase(eventList.begin());
				LogDebug("Song had an odd number of beats! Removing the first one...");
			}

			end = std::chrono::system_clock::now();

			LogDebug("BeatRoot processing took ", Duration<Microseconds>(end - start).AsSeconds(), " seconds");

			// Calculate BPM
			if (!eventList.empty()) {
				auto [min, average, max] = GetTimeBetweenBeats();

				LogDebug(
					"Song's estimated BPM is ",
					static_cast<int>(1.0 / average * 60.0) / (Settings::settings.GetHalveBpm() ? 2.0 : 1.0),
					" based on ",
					eventList.size(),
					" beats",
					Settings::settings.GetHalveBpm() ? " (divided by 2)" : ""
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
				LogWarning("No beats detected. Trying again with a hop time of 10ms...");

				lock.unlock();

				_OnLoad(path, cache, streamHandle, freq, chans, onLoaded, startTime ? startTime : 0.0, endTime, index, 0.010);

				// Return so we don't try to free the stream twice
				return;
			} else if (!parameters) {
				LogWarning("No beats detected even with a smaller hop size! Increasing expiry time next...");

				lock.unlock();

				AgentParameters newParameters;
				newParameters.expiryTime = 100.0;
				_OnLoad(path, cache, streamHandle, freq, chans, onLoaded, startTime ? startTime : 0.0, endTime, index, 0.010, newParameters);

				// Return so we don't try to free the stream twice
				return;
			} else {
				LogError("No beats detected with a smaller hop size and larger expiry time!");
			}

			state = State::Loaded;
		} else {
			state = State::Idle;
		}
	}

	BASS_StreamFree(streamHandle);

	// Only call the callback if we're
	// actually detecting, weren't
	// canceled, and aren't colliding
	// with an already-existing file
	if (detectBpm && !canceled && !collision) {
		if (!cachePath.empty()) {
			std::ofstream outFile(cachePath, std::ios::out | std::ios::binary);

			// Use NaN to signal we have additional data
			constexpr double nan = std::numeric_limits<double>::quiet_NaN();
			outFile.write(reinterpret_cast<const char *>(&nan), sizeof(double));

			const auto filename = path.filename().u8string();
			const auto size = filename.size();

			// Leave length of filename
			outFile.write(reinterpret_cast<const char *>(&size), sizeof(std::string::size_type));

			// Leave filename
			outFile.write(reinterpret_cast<const char *>(filename.c_str()), size);

			for (const auto &event : eventList)
				outFile.write(reinterpret_cast<const char *>(&event), sizeof(Event));
		}
		onLoaded();
	}

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