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

#include "CConsole.h"

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
}

LightPack::~LightPack() {
	delete[] lightBin;
	if (socketSet) SDLNet_FreeSocketSet(socketSet);
}

void LightPack::OnInit() {
	running = true;

	thread = std::thread([this] {
		while (running) {
			{
				if (queue.size() > 16)
					CConsole::Console.Print("LightPack is running over a second late!", MSG_ALERT);

				if (queue.size()) {
					if (auto &front = queue.front())
						front(this);

					queue.pop();
				}
			}

			std::unique_lock lock(condMutex);
			cond.wait_for(lock, sleepTime);
			sleepTime = 1ms;
		}

		// Process whatever's left in the queue
		while (queue.size()) {
			queue.front()(this);
			queue.pop();
		}
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
		CConsole::Console.Print("Could not initialize WSA!", MSG_ERROR);
		return false;
	}

	PADDRINFOA addr;

	if (getaddrinfo("localhost", "3636", NULL, &addr)) {
		CConsole::Console.Print("Could not get address info!", MSG_ERROR);
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

	if (SDLNet_Init() != -1) {
		IPaddress ip;
		if (SDLNet_ResolveHost(&ip, "localhost", 3636) != -1) {
			if (CanConnect())
				tcpsock = SDLNet_TCP_Open(&ip);

			if (tcpsock) {
				// Only lock if we manage to open a socket
				// since SDLNet_TCP_Open() blocks until
				// timeout.
				std::unique_lock lock(mutex);

				socketSet = SDLNet_AllocSocketSet(1);
				SDLNet_TCP_AddSocket(socketSet, tcpsock);

				// Flush whatever's in the buffer
				ReadString();

				// Get previous settings first
				auto gammaString = WriteString("getgamma\r\n");
				gammaString->erase(gammaString->size() - 2); // Trim the lazy way
				CConsole::Console.Print(*gammaString, MSG_DIAG);
				previousGamma = std::stof(
					gammaString->substr(gammaString->find(':') + 1)
				);

				auto smoothString = WriteString("getsmooth\r\n");
				smoothString->erase(smoothString->size() - 2); // Trim the lazy way
				CConsole::Console.Print(*smoothString, MSG_DIAG);
				previousSmooth = std::stoi(
					smoothString->substr(smoothString->find(':') + 1)
				);

				auto countLeds = WriteString("getcountleds\r\n");
				countLeds->erase(countLeds->size() - 2); // Trim the lazy way
				CConsole::Console.Print(*countLeds, MSG_DIAG);
				numberOfLights = std::stoi(
					countLeds->substr(countLeds->find(':') + 1)
				);

				lightBin = new double[numberOfLights];
				memset(lightBin, 0, sizeof(double) * numberOfLights);

				WriteString("apikey:\r\n");
				WriteString("lock\r\n");

				// Set our preferred settings right away,
				// let the user change them later
				auto ret = WriteString("setbrightness:100\r\n");
				CConsole::Console.Print("setbrightness:100 = " + ret->substr(0, ret->size() - 2), MSG_DIAG);
				ret = WriteString("setgamma:1\r\n");
				CConsole::Console.Print("setgamma:1 = " + ret->substr(0, ret->size() - 2), MSG_DIAG);
				ret = WriteString("setsmooth:0\r\n");
				CConsole::Console.Print("setsmooth:0 = " + ret->substr(0, ret->size() - 2), MSG_DIAG);

			} else {
				if (firstTry) {
					CConsole::Console.Print(
						"Could not open a socket to LightPack host. Retrying until we can...",
						MSG_ALERT
					);

					firstTry = false;
				}
				if (running)
					RetryConnection();
			}
		} else {
			CConsole::Console.Print(
				std::string("Could not connect to LightPack host. Error: ").append(SDLNet_GetError()) + ". Retrying in 5 seconds...",
				MSG_ALERT
			);
			if (running)
				RetryConnection();
		}
	} else {
		CConsole::Console.Print(
			std::string("Could not initialize SDL_Net! Error: ").append(SDLNet_GetError()),
			MSG_ERROR
		);
	}
}

void LightPack::RetryConnection() {
	std::unique_lock lock(mutex);
	std::unique_lock condLock(condMutex);

	sleepTime = 5s;

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
			SDLNet_TCP_Close(lp->tcpsock);
			SDLNet_Quit();
		});
	}

	{
		std::unique_lock lock(condMutex);
		running = false;
		cond.notify_one();
	}

	if (thread.joinable())
		thread.join();
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

void LightPack::OnLoop(const Colour<float> &color) {
	std::unique_lock lock(mutex);

	if (!lightBin) return;

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

	WriteString(stream.str(), false);

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
}
void LightPack::SetFocusArea(FocusArea focus) {
	std::unique_lock lock(mutex);
	focusArea = focus;
	binsPerLight = bufferLength / static_cast<int>(focus) / numberOfLights;
}
void LightPack::SetMapping(const int *mapping) {
	std::unique_lock lock(mutex);
	currentMapping = mapping;
}

void LightPack::SetSmooth(uint8_t smooth) {
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
	std::unique_lock lock(mutex);

	if (!tcpsock) return;

	queue.emplace([gamma, silent](LightPack *lp) {
		std::unique_lock lock(lp->mutex);

		std::stringstream stream;
		stream << "setgamma:" << std::fixed << std::setprecision(2) << std::setfill('0') << gamma;
		if (!silent) CConsole::Console.Print(stream.str(), MSG_DIAG);
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

std::optional<std::string> LightPack::ReadString() const {
	if (tcpsock && SDLNet_CheckSockets(socketSet, 250) > 0) {
		char msg[128] = { 0 };
		if (SDLNet_TCP_Recv(tcpsock, msg, 127) <= 0)
			return std::nullopt;

		return std::string(msg);
	}

	return std::nullopt;
}

std::optional<std::string> LightPack::WriteString(std::string str, bool response) const {
	if (tcpsock) {
		SDLNet_TCP_Send(tcpsock, str.c_str(), static_cast<int>(str.length()));

		if (response)
			return ReadString();
	}

	return std::nullopt;
}