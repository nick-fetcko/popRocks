#include "LightPack.hpp"

#include <iomanip>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")
#endif

#include "Settings.hpp"

LightPack::LightPack() {
	// Purple
	intensityColors[0].r = 255;
	intensityColors[0].g = 0;
	intensityColors[0].b = 255;

	// Blue
	intensityColors[1].r = 0;
	intensityColors[1].g = 0;
	intensityColors[1].b = 255;

	// Green
	intensityColors[2].r = 0;
	intensityColors[2].g = 255;
	intensityColors[2].b = 0;

	// Yellow
	intensityColors[3].r = 255;
	intensityColors[3].g = 255;
	intensityColors[3].b = 0;

	// Orange
	intensityColors[4].r = 255;
	intensityColors[4].g = 165;
	intensityColors[4].b = 0;

	// Red
	intensityColors[5].r = 255;
	intensityColors[5].g = 0;
	intensityColors[5].b = 0;

	const auto &type = Settings::settings.GetLightPackVisualizationType();
	if (type == "intensity")
		currentLightType = LightType::Intensity;
	else if (type == "color")
		currentLightType = LightType::Color;
	else if (type == "colorintensity")
		currentLightType = LightType::ColorIntensity;

	const auto &mapping = Settings::settings.GetLightPackMapping();
	if (mapping == "default")
		currentMapping = Mappings::DEFAULT;
	else if (mapping == "mine")
		currentMapping = Mappings::MINE;
	else if (mapping == "ttb")
		currentMapping = Mappings::TOP_TO_BOTTOM;
	else if (mapping == "btt")
		currentMapping = Mappings::BOTTOM_TO_TOP;

	const auto &focusArea = Settings::settings.GetLightPackFocusArea();
	if (focusArea == "superbass")
		SetFocusArea(FocusArea::SuperBass);
	else if (focusArea == "subbass")
		SetFocusArea(FocusArea::SubBass);
	else if (focusArea == "bass")
		SetFocusArea(FocusArea::Bass);
	else if (focusArea == "bassandmid")
		SetFocusArea(FocusArea::BassAndMid);
	else if (focusArea == "bassmidandalittlehighend")
		SetFocusArea(FocusArea::BassMidAndHigh);
	else if (focusArea == "halfnyquist")
		SetFocusArea(FocusArea::HalfNyquist);
	else if (focusArea == "nyquist")
		SetFocusArea(FocusArea::Nyquist);
}

LightPack::~LightPack() {
	delete[] lightBin;
}

void LightPack::OnInit() {
	// Avoid potential off-by-one-millisecond
	sleepTime = 0s;

	running = true;

	thread = std::thread([this] {
		while (running) {
			{
				if (queue.size() > 16) {
					LogWarning("LightPack is running over a second late! Flushing queue...");
					while (queue.size())
						queue.pop();
				}

				if (queue.size()) {
					if (auto &front = queue.front())
						front(this);

					queue.pop();
				}
			}

			std::unique_lock lock(condMutex);
			if (running)
				cond.wait_for(lock, sleepTime);
			if (lightBin) sleepTime = 1ms;
		}

		// Process whatever's left in the queue
		LogInfo("Processing what's left in the queue (", queue.size(), " items)...");
		while (queue.size()) {
			if (tcpsock) queue.front()(this);
			queue.pop();
		}
		LogInfo("Done processing!");

		delete[] lightBin;
		lightBin = nullptr;

		if (tcpsock) {
			NET_DestroyStreamSocket(tcpsock);
			tcpsock = nullptr;
		}

		NET_Quit();
	});

	std::unique_lock lock(mutex);
	queue.emplace(std::mem_fn(&LightPack::_OnInit));
}

bool LightPack::CanConnect() {
#ifdef _WIN32
	// SDL_net doesn't provide non-blocking sockets,
	// and the timeout on my machine is ~5 seconds
	// 
	// This just sets the timeout on a nonblocking socket
	// to a much more reasonable 0.5 seconds before
	// attempting a connection. If the connection succeeds,
	// then we let SDL_net take over.
	//
	// FIXME: Consider entirely replacing SDL_net with 
	//        ixwebsocket

	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
		LogError("Could not initialize WSA!");
		return false;
	}

	PADDRINFOA addr;

	if (getaddrinfo("localhost", "3636", NULL, &addr)) {
		LogError("Could not get address info!");
		return false;
	}

	fd_set set;
	set.fd_count = 0;
	while (addr) {
		SOCKET s = socket(addr->ai_family, addr->ai_socktype,
			addr->ai_protocol);

		u_long mode = 1;  // 1 to enable non-blocking socket
		ioctlsocket(s, FIONBIO, &mode);

		connect(s, addr->ai_addr, (int)addr->ai_addrlen);

		set.fd_array[set.fd_count++] = s;
		addr = addr->ai_next;
	}

	timeval time;
	time.tv_sec = 0;
	time.tv_usec = 500000;

	auto result = ::select(0, NULL, &set, NULL, &time);
	if (result <= 0) {
		tcpsock = nullptr;

		return false;
	}

	for (u_int i = 0; i < set.fd_count; ++i)
		closesocket(set.fd_array[i]);

#endif
	return true;
}

void LightPack::_OnInit() {
	if (!running) return;

	if (NET_Init()) {
		if (auto ip = NET_ResolveHostname("127.0.0.1")) {
			auto status = NET_WaitUntilResolved(ip, -1);
			if (CanConnect())
				tcpsock = NET_CreateClient(ip, 3636);

			if (status = NET_WaitUntilConnected(tcpsock, -1); status != NET_SUCCESS) {
				NET_DestroyStreamSocket(tcpsock);
				tcpsock = nullptr;
			}

			auto error = SDL_GetError();

			if (tcpsock) {
				// Only lock if we manage to open a socket
				// since SDLNet_TCP_Open() blocks until
				// timeout.
				std::unique_lock lock(mutex);

				// Flush whatever's in the buffer
				ReadString();

				// Get previous settings first
				auto gammaString = WriteString("getgamma\r\n");
				gammaString->erase(gammaString->size() - 2); // Trim the lazy way
				LogDebug(*gammaString);
				previousGamma = std::stof(
					gammaString->substr(gammaString->find(':') + 1)
				);

				auto smoothString = WriteString("getsmooth\r\n");
				smoothString->erase(smoothString->size() - 2); // Trim the lazy way
				LogDebug(*smoothString);
				previousSmooth = std::stoi(
					smoothString->substr(smoothString->find(':') + 1)
				);

				auto countLeds = WriteString("getcountleds\r\n");
				countLeds->erase(countLeds->size() - 2); // Trim the lazy way
				LogDebug(*countLeds);
				numberOfLights = std::stoi(
					countLeds->substr(countLeds->find(':') + 1)
				);

				lightBin = new double[numberOfLights];
				memset(lightBin, 0, sizeof(double) * numberOfLights);

				WriteString("apikey:\r\n");
				WriteString("lock\r\n");

				// Set user-defined settings
				auto ret = WriteString("setbrightness:100\r\n");
				LogDebug("setbrightness:100 = ", ret->substr(0, ret->size() - 2));

				std::stringstream gamma;
				gamma << "setgamma:" << std::fixed << std::setprecision(2) << std::setfill('0') << Settings::settings.GetGamma();
				ret = WriteString(gamma.str() + "\r\n");
				LogDebug(gamma.str() + " = ", ret->substr(0, ret->size() - 2));

				auto smooth = "setsmooth:" + std::to_string(static_cast<int>(Settings::settings.GetSmooth()));
				ret = WriteString(smooth + "\r\n");
				LogDebug(smooth + " = ", ret->substr(0, ret->size() - 2));
			} else {
				if (firstTry) {
					LogWarning(
						"Could not open a socket to LightPack host. Retrying until we can..."
					);

					firstTry = false;
				}
				if (running)
					RetryConnection();
			}
		} else {
			LogWarning(
				"Could not connect to LightPack host. Error: ", SDL_GetError(), ". Retrying in 5 seconds..."
			);
			if (running)
				RetryConnection();
		}
	} else {
		LogError(
			"Could not initialize SDL_Net! Error: ", SDL_GetError()
		);
	}
}

void LightPack::RetryConnection() {
	std::unique_lock lock(mutex);
	std::unique_lock condLock(condMutex);

	if (sleepTime < 10s)
		sleepTime += 1s;

	const auto inSeconds = std::chrono::duration_cast<std::chrono::seconds>(sleepTime).count();
	LogInfo("Retrying in ", inSeconds , " second", inSeconds > 1 ? "s" : "", "...");

	queue.emplace([](LightPack *lp) {
		lp->_OnInit();
	});
}

void LightPack::OnDestroy() {
	{
		std::unique_lock lock(mutex);
		queue.emplace([](LightPack *lp) {
			std::unique_lock lock(lp->mutex);

			std::stringstream stream;

			// Limit to 1000 iterations
			// just in case this gets stuck
			std::size_t iterations = 0;
			while (lp->ReadString() && iterations++ < 1000);

			stream << "setsmooth:" << static_cast<int>(lp->previousSmooth) << "\r\n";
			lp->WriteString(stream.str());
			stream = std::stringstream();
			stream << "setgamma:" << std::setprecision(2) << std::fixed << std::setfill('0') << lp->previousGamma << "\r\n";
			lp->WriteString(stream.str());
			lp->WriteString("unlock\r\n");
			NET_DestroyStreamSocket(lp->tcpsock);
			lp->tcpsock = nullptr;
		});
	}

	LogInfo("Stopping LightPack thread...");

	{
		std::unique_lock lock(condMutex);
		running = false;
		cond.notify_one();
	}

	LogInfo("\tJoining...");

	if (thread.joinable())
		thread.join();

	LogInfo("LightPack thread stopped!");
}

void LightPack::NextSamples(float *samples, std::size_t count) {
	std::unique_lock lock(mutex);

	if (lightBin && captureTimer == CaptureFreq && currentLight <= numberOfLights) {
		for (auto i = 0; i < count; ++i) {
			lightBin[currentLight - 1] += samples[i];
			if (currentSample++ > currentLight * binsPerLight) {
				if (++currentLight > numberOfLights)
					break;
			}
		}
	}
}

void LightPack::NextSample(const float &sample) {
	if (lightBin && captureTimer == CaptureFreq && currentLight <= numberOfLights) {
		lightBin[currentLight - 1] += sample;

		if (currentSample++ > currentLight * binsPerLight) {
			++currentLight;
			//std::cout << currentLight << std::endl;
		}
		//captureTimer = 0;
	}
}

void LightPack::OnDisconnect() {
	LogError("Forcibly disconnecting from LightPack socket!");

	running = false;
}

void LightPack::OnLoop(const Colour<float> &color) {
	std::unique_lock lock(mutex);

	if (!lightBin) {
		// If we've been disconnected,
		// start searching again
		if (!running) {
			lock.unlock();

			if (thread.joinable())
				thread.join();

			OnInit();
		}
		return;
	}

	if (captureTimer++ == CaptureFreq) {
		this->color = color;
		queue.emplace(std::mem_fn(&LightPack::_OnLoop));
		captureTimer = 0;
	}
}

void LightPack::_OnLoop() {
	std::unique_lock lock(mutex);

	for (auto i = 0; i < numberOfLights / LightsPerDevice; ++i) {
		localMaximums[i] = 0;
		for (auto j = 0; j < numberOfLights / localMaximums.size(); ++j) {
			if (lightBin[i * LightsPerDevice + j] > localMaximums[i])
				localMaximums[i] = lightBin[i * LightsPerDevice + j];
		}
	}
	//std::cout << "Max = " << max << std::endl;
	for (int i = 0; i < numberOfLights; i++) {
		auto max = localMaximums[i / LightsPerDevice];

		if (max == 0) continue;

		lightBin[i] = lightBin[i] / max * 255.0;
	}

	std::stringstream stream;
	std::stringstream stream2;

	stream << "setcolor:";

	for (int i = 0; i < numberOfLights; i++) {
		if (currentLightType == LightType::Intensity) {
			if (method == Method::RGB) {
				stream
					<< currentMapping[i]
					<< "-"
					<< static_cast<int>((color.r * 255.0f) * (lightBin[i] / 255.0f))
					<< ","
					<< static_cast<int>((color.g * 255.0f) * (lightBin[i] / 255.0f))
					<< ","
					<< static_cast<int>((color.b * 255.0f) * (lightBin[i] / 255.0f))
					<< ";";
			} else if (method == Method::HSV) {
				auto hsv = color.ToHsv();
				hsv.v = static_cast<float>(lightBin[i] / 255.0);
				hsv.s = std::clamp(hsv.s * saturationMultiplier, 0.0f, 1.0f);
				auto rgb = Colour<float>::FromHsv(hsv.h, hsv.s, hsv.v);

				stream
					<< currentMapping[i]
					<< "-"
					<< static_cast<int>((rgb.r * 255.0f))
					<< ","
					<< static_cast<int>((rgb.g * 255.0f))
					<< ","
					<< static_cast<int>((rgb.b * 255.0f))
					<< ";";
			}
		} else if (currentLightType == LightType::ColorIntensity) {
			int intensity = static_cast<int>(lightBin[i] / 255.0f * 6.0f);
			int binSize = 255 / 6;
			int brightness = static_cast<int>(
				(lightBin[i] / 255.0 * binSize) / binSize * 255.0
			);
			stream
				<< currentMapping[i]
				<< "-"
				<< static_cast<int>(intensityColors[intensity].r * (brightness / 255.0f))
				<< ","
				<< static_cast<int>(intensityColors[intensity].g * (brightness / 255.0f))
				<< ","
				<< static_cast<int>(intensityColors[intensity].b * (brightness / 255.0f))
				<< ";";

			stream2 << brightness << ", ";
		} else if (currentLightType == LightType::Color) {
			int intensity = static_cast<int>(lightBin[i] / 255.0f * 6.0f);
			stream
				<< currentMapping[i]
				<< "-"
				<< static_cast<int>(intensityColors[intensity].r)
				<< ","
				<< static_cast<int>(intensityColors[intensity].g)
				<< ","
				<< static_cast<int>(intensityColors[intensity].b)
				<< ";";
		}

		lightBin[i] = 0;
	}
	stream << "\r\n";

	if (!WriteString(stream.str(), false))
		OnDisconnect();

	currentSample = 0;
	currentLight = 1;
}

void LightPack::SetBufferLength(std::size_t bufferLength) {
	std::unique_lock lock(mutex);

	this->bufferLength = bufferLength;

	binsPerLight = bufferLength / static_cast<int>(focusArea) / numberOfLights;

	// Expectation is that each LightPack has 10 lights
	localMaximums.resize(numberOfLights / LightsPerDevice);
}

void LightPack::SetLightType(LightType type) {
	std::unique_lock lock(mutex);
	currentLightType = type;

	Settings::settings.SetLightPackVisualizationType(
		type == LightType::Intensity ?
			"intensity" :
			type == LightType::Color ?
				"color" :
				"colorintensity"
	);
}
void LightPack::SetFocusArea(FocusArea focus) {
	std::unique_lock lock(mutex);
	focusArea = focus;
	binsPerLight = bufferLength / static_cast<int>(focus) / numberOfLights;

	// These might look like bitwise flags, but they aren't
	// ... I just like powers of 2.
#pragma warning(push)
#pragma warning(disable:26813)
	Settings::settings.SetLightPackFocusArea(
		focus == FocusArea::SuperBass ?
			"superbass" :
			focus == FocusArea::SubBass ?
				"subbass" :
				focus == FocusArea::Bass ?
					"bass" :
					focus == FocusArea::BassAndMid ?
						"bassandmid" :
						focus == FocusArea::BassMidAndHigh ?
							"bassmidandalittlehighend" :
							focus == FocusArea::HalfNyquist ?
								"halfnyquist" :
								"nyquist"
	);
#pragma warning(pop)
}
void LightPack::SetMapping(const int *mapping) {
	std::unique_lock lock(mutex);
	currentMapping = mapping;

	Settings::settings.SetLightPackMapping(
		mapping == Mappings::DEFAULT ?
			"default" :
			mapping == Mappings::MINE ?
				"mine" :
				mapping == Mappings::TOP_TO_BOTTOM ?
					"ttb" :
					"btt"
	);
}

void LightPack::SetSmooth(uint8_t smooth) {
	Settings::settings.SetSmooth(smooth);

	std::unique_lock lock(mutex);

	if (!tcpsock) return;

	queue.emplace([smooth](LightPack *lp) {
		std::unique_lock lock(lp->mutex);

		std::stringstream stream;
		stream << "setsmooth:" << static_cast<int>(smooth) << "\r\n";
		lp->WriteString(stream.str());
	});
}

void LightPack::SetGamma(float gamma, bool silent) {
	if (!silent) Settings::settings.SetGamma(gamma);

	std::unique_lock lock(mutex);

	if (!tcpsock) return;

	queue.emplace([gamma, silent, this](LightPack *lp) {
		std::unique_lock lock(lp->mutex);

		std::stringstream stream;
		stream << "setgamma:" << std::fixed << std::setprecision(2) << std::setfill('0') << gamma;
		if (!silent) LogDebug(stream.str());
		stream << "\r\n";
		lp->WriteString(stream.str());
	});
}

void LightPack::SetMethod(Method method) {
	this->method = method;
}

void LightPack::SetSaturationMultiplier(float saturationMultiplier) {
	this->saturationMultiplier = saturationMultiplier;
}

std::string LightPack::GetStringForMassColorChangeCommand(int start, int end, unsigned char r, unsigned char g, unsigned char b) const {
	//std::string str = "setcolor:";
	std::stringstream stream;
	stream << "setcolor:";
	for (int i = start; i <= end; i++) {
		stream << i << "-" << (int)r << "," << (int)g << "," << (int)b;
		//if(i != numberOfLights)
		stream << ";";

		//str << stream.str();
	}

	stream << "\r\n";

	return stream.str();
}

std::optional<std::string> LightPack::ReadString(bool block) const {
	if (tcpsock) {
		char msg[128] = { 0 };
		if (!block) {
			if (NET_ReadFromStreamSocket(tcpsock, msg, 127) <= 0)
				return std::nullopt;
		} else {
			while (NET_ReadFromStreamSocket(tcpsock, msg, 127) <= 0)
				std::this_thread::sleep_for(1ms);
		}

		return std::string(msg);
	}

	return std::nullopt;
}

std::optional<std::string> LightPack::WriteString(std::string str, bool response) const {
	if (tcpsock) {
		if (!NET_WriteToStreamSocket(tcpsock, str.c_str(), static_cast<int>(str.length()))) {
			LogError("Error writing to LightPack socket: ", SDL_GetError());
			return std::nullopt;
		}

		if (response) {
			while (NET_GetStreamSocketPendingWrites(tcpsock))
				std::this_thread::sleep_for(1ms);

			return ReadString(true);
		}
	}

	return "";
}