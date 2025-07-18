#include "CApp.h"

#include <atlstr.h>
#include <filesystem>
#include <map>
#include <math.h>
#include <sstream>
#include <vector>
#include <fstream>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>

#include <bassape.h>
#include <basswv.h>

#include <basswasapi.h>

#include "MathCPP/Colour.hpp"

#include "CConsole.h"
#include "Buffer.hpp"
#include "FFTLineRenderer.hpp"
#include "FFTRenderer.hpp"
#include "ID3V2.hpp"
#include "MP4.hpp"
#include "OscilloscopeRenderer.hpp"
#include "RecordAudioStream.h"

using namespace MathsCPP;

// =====================================================
// ===================== Callbacks =====================
// =====================================================
const std::map<DWORD, SDL_KeyCode> KeyMap = {
	{ VK_VOLUME_DOWN, SDLK_VOLUMEDOWN },
	{ VK_VOLUME_UP, SDLK_VOLUMEUP },
	{ VK_MEDIA_NEXT_TRACK, SDLK_AUDIONEXT },
	{ VK_MEDIA_PREV_TRACK, SDLK_AUDIOPREV },
	{ VK_MEDIA_PLAY_PAUSE, SDLK_AUDIOPLAY }
};

HHOOK keyboardHook = nullptr;
LRESULT CALLBACK LowLevelKeyboardProc(
	_In_ int    nCode,
	_In_ WPARAM wParam,
	_In_ LPARAM lParam
) {
	if (nCode < 0) return CallNextHookEx(keyboardHook, nCode, wParam, lParam);

	auto hookStruct = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);

	// From https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelkeyboardproc
	// 
	// If the hook procedure processed the message, it may return
	// a nonzero value to prevent the system from passing the message
	// to the rest of the hook chain or the target window procedure.
	if (KeyMap.find(hookStruct->vkCode) != KeyMap.end()) {
		if (wParam == WM_KEYDOWN) {
			SDL_Event event;
			event.type = SDL_KEYDOWN;
			event.key.keysym.sym = KeyMap.at(hookStruct->vkCode);
			SDL_PushEvent(&event);
		}
		return 1;
	} else return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}

// WASAPI input processing function
DWORD CALLBACK InWasapiProc(void *buffer, DWORD length, void *user) {
	//BASS_StreamPutData(CApp::App->GetStreamHandle(), buffer, length); // feed the data to the input stream
	return 1; // continue recording
}

DWORD CALLBACK OutputWasapiProc(void *buffer, DWORD length, void *user) {
	const auto app = reinterpret_cast<CApp *>(user);

	int c = BASS_ChannelGetData(app->GetStreamHandle(), buffer, length);
	if (c < 0) { // at the end of the current stream, but not the _buffer_
		auto code = BASS_ErrorGetCode();
		if (auto next = app->GetControls().GetPlaylist().Next()) {
			auto extension = next->path.extension().u8string();
			std::transform(extension.begin(), extension.end(), extension.begin(), tolower);
			app->Open(next->path, extension, true);

			// Immediately start adding new samples to the buffer
			// for gapless playback
			c = BASS_ChannelGetData(app->GetStreamHandle(), buffer, length);

			// Update the UI on the next loop
			app->AdvanceToNextTrack();
		} else {
			// Can't kill WASAPI from inside WASAPI,
			// so we also have to do this on the next loop
			app->StopExclusive();

			return 0;
		}
	}

	if (app->GetControls().GetVolume().GetVolumeControl()) {
		auto floatBuffer = reinterpret_cast<float *>(buffer);
		for (auto i = 0; i < c / sizeof(float); ++i)
			floatBuffer[i] *= app->GetControls().GetVolume().GetScaledVolume();
	}

	return c;
}

// =====================================================
// ======================= CApp ========================
// =====================================================
CApp::CApp() : controls(&albumArt), circleLine(12.0f) {
	renderer = new FFTRenderer(&dynamicGain, &albumArt);

	// If we close the console, make sure to
	// clean up before exit
	CConsole::Console.SetOnClose([this] {
		OnDestroy();
	});

	// Add our console commands
	AddCommands();
}

void CApp::UpdateMaxBufferLength() {
	auto length = std::max(fftLength, bufferLength);

	if (length != maxLength) {
		delete[] buffer;
		buffer = new uint8_t[length * sizeof(float)];
		memset(buffer, 0, length * sizeof(float));
		floatBuffer = reinterpret_cast<float *>(buffer);
		shortBuffer = reinterpret_cast<short *>(buffer);

		renderer->SetBuffer(buffer, length);
		resetGain = dynamicGain.reset;

		maxLength = length;
	}
}

void CApp::SetBufferLength(std::size_t bufferLength) {
	auto changed = bufferLength != this->bufferLength;
	if (changed) {
		this->bufferLength = bufferLength;

		hStep = static_cast<float>(windowWidth) / bufferLength;
		
		lightPack.SetBufferLength(bufferLength);

		UpdateMaxBufferLength();
	}

	renderer->SetBufferLength(bufferLength, changed);

	if (listening && changed)
		Listen();
}

void CApp::SetFftLength(std::size_t length) {
	/*
	
	#define BASS_DATA_FFT256	0x80000000	// 256 sample FFT
	#define BASS_DATA_FFT512	0x80000001	// 512 FFT
	#define BASS_DATA_FFT1024	0x80000002	// 1024 FFT
	#define BASS_DATA_FFT2048	0x80000003	// 2048 FFT
	#define BASS_DATA_FFT4096	0x80000004	// 4096 FFT
	#define BASS_DATA_FFT8192	0x80000005	// 8192 FFT
	#define BASS_DATA_FFT16384	0x80000006	// 16384 FFT
	
	*/

	// Use the fftLength to hold 
	// the number of samples actually
	// returned by the FFT
	if (length <= 256) {
		fftLength = 128;
		fftFlag = BASS_DATA_FFT256;
	} else if (length <= 512) {
		fftLength = 256;
		fftFlag = BASS_DATA_FFT512;
	} else if (length <= 1024) {
		fftLength = 512;
		fftFlag = BASS_DATA_FFT1024;
	} else if (length <= 2048) {
		fftLength = 1024;
		fftFlag = BASS_DATA_FFT2048;
	} else if (length <= 4096) {
		fftLength = 2048;
		fftFlag = BASS_DATA_FFT4096;
	} else if (length <= 8192) {
		fftLength = 4096;
		fftFlag = BASS_DATA_FFT8192;
	} else {
		fftLength = 8192;
		fftFlag = BASS_DATA_FFT16384;
	}

	UpdateMaxBufferLength();
}

void CApp::PrepareFile(std::wstring file) {
	savedFile = file;
}

float CApp::GetScale(SDL_Window *window, int *w, int *h) {
	int virtualW = 0, virtualH = 0;
	SDL_GetWindowSize(window, &virtualW, &virtualH);

	int localW = 0, localH = 0;

	// Use local variables if no pointers
	// were provided
	if (!w) w = &localW;
	if (!h) h = &localH;

	SDL_GetWindowSizeInPixels(window, w, h);

	scale = (virtualW == 0 ? 1.0f : static_cast<float>(*w) / virtualW);

	return scale;
}

void CApp::OnInit() {
	CConsole::Console.OnInit();

	SetBufferLength(2048);

	// We only sample halfway to the
	// Nyquist at start. This gives
	// us 4096 samples but our buffer
	// length is only 2048. The highest
	// bins are largely empty, anyway.
	SetFftLength(8192);

	// https://tgui.eu/tutorials/latest-stable/dpi-scaling/
	SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");

	SDL_Init(SDL_INIT_EVERYTHING);
	auto ret = IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_WEBP);

	std::stringstream stream;
	stream << "IMG_Init loaded ";
	if (ret & IMG_INIT_JPG) stream << "JPG ";
	if (ret & IMG_INIT_PNG) stream << "PNG ";
	if (ret & IMG_INIT_WEBP) stream << "WEBP";
	CConsole::Console.Print(stream.str(), MSG_DIAG);

	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
	sdlWindow = SDL_CreateWindow(
		"popRocks",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		windowWidth,
		windowHeight,
		SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
	);
	SDL_GLContext Context = SDL_GL_CreateContext(sdlWindow);
	SDL_GL_MakeCurrent(sdlWindow, Context);

	gladLoadGL();

	sdlRenderer = SDL_CreateRenderer(sdlWindow, 0, 0);

	// Prefer adaptive sync over regular vsync
	if (SDL_GL_SetSwapInterval(-1) == -1)
		SDL_GL_SetSwapInterval(1);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	OnResize(windowWidth, windowHeight);

	if (!BASS_PluginLoad("bassflac.dll", 0))
		CConsole::Console.Print("Could not load FLAC plugin! Error code " + std::to_string(BASS_ErrorGetCode()), MSG_ERROR);
	if (!BASS_PluginLoad("bassape.dll", 0))
		CConsole::Console.Print("Could not load APE plugin! Error code " + std::to_string(BASS_ErrorGetCode()), MSG_ERROR);
	if (!BASS_PluginLoad("basswv.dll", 0))
		CConsole::Console.Print("Could not load WavPack plugin! Error code " + std::to_string(BASS_ErrorGetCode()), MSG_ERROR);

	if (BASS_Init(device, freq, 0, 0, nullptr) != TRUE)
		CConsole::Console.Print("Could not initialize audio device!", MSG_ERROR);
	
	// This updates the scale variable for us
	GetScale(sdlWindow);

	lightPack.OnInit();
	albumArt.OnInit(windowWidth, windowHeight, scale);
	renderer->OnInit(windowWidth, windowHeight);
	albumArt.AddColorChangeListener(this);
	controls.OnInit(windowWidth, windowHeight, scale);

	controls.SetFadeCallback([this](bool in) {
		if (SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_FULLSCREEN_DESKTOP)
			SDL_ShowCursor(in ? SDL_ENABLE : SDL_DISABLE);
	});
}

 CApp::~CApp() {
	 delete[] buffer;
	 delete audioSink;

	 if(in) fftw_free(in);
	 if(out) fftw_free(out);
	 if (plan) fftw_destroy_plan(plan);
}

void CApp::Listen() {
	if (listening) {
		audioSink->done = true;
		if (listenThread.joinable())
			listenThread.join();

		fftw_free(in);
		fftw_free(out);
		fftw_destroy_plan(plan);
	}
	//audioSink = new MyAudioSink();
	//CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RecordAudioStream, audioSink, 0, NULL);

	SetGain(1.0f);

	in = reinterpret_cast<double*>(fftw_malloc(sizeof(double) * bufferLength * 2));
    out = reinterpret_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex) * bufferLength * 2));
	plan = fftw_plan_dft_r2c_1d(static_cast<int>(bufferLength * 2), in, out, FFTW_ESTIMATE);

	//streamHandle = BASS_StreamCreate(48000, 2, BASS_SAMPLE_FLOAT, STREAMPROC_PUSH, NULL);
	//listening = true;
	//WASAPIPROC *proc = CApp::App.InWasapiProc;
	//bool success = BASS_WASAPI_Init(-2, 48000, 2, BASS_WASAPI_EVENT, 1024, 0, InWasapiProc, NULL);
	//std::cout << success << std::endl;
	//BASS_ChannelSetAttribute(streamHandle, BASS_ATTRIB_MUSIC_VOL_GLOBAL, 0);
	//BASS_ChannelSetAttribute(streamHandle, BASS_ATTRIB_VOL, 0);
	//BASS_ChannelPlay(streamHandle, false);
	audioSink = new MyAudioSink(bufferLength * 4 /* we're assuming stereo, for now */);
	//audioSink->streamHandle = streamHandle;
	//BASS_WASAPI_Start();

	listenThread = std::thread(RecordAudioStream, audioSink);

	listening = true;
}

HSTREAM CApp::GetStreamHandle() const {
	return streamHandle;
}

void CApp::OnResize(int width, int height, float scale) {
	windowWidth = width;
	windowHeight = height;
	hStep = static_cast<float>(windowWidth) / bufferLength;

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, windowWidth, windowHeight, 0, -100.0f, 100.0f);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glViewport(0, 0, windowWidth, windowHeight);

	glClearAccum(0.0, 0.0, 0.0, 1.0);
	glClear(GL_ACCUM_BUFFER_BIT);

	renderer->OnResize(windowWidth, windowHeight);
	controls.OnResize(windowWidth, windowHeight, scale);
}

const Colour<float> &CApp::GetColor() const {
	if (albumArt.Loaded() && !overrideColor)
		return albumArt.GetColor();
	else
		return visColor;
}

void CApp::SetColor(float alpha) const {
	const auto &color = GetColor();

	glColor4f(color.r, color.g, color.b, alpha);
}

inline void CApp::AdvanceToNextTrack() {
	advanceOnNextLoop = true;
}

void CApp::OnLoop(const Delta &time) {
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	if (!fileLoaded && !listening) {
		SwapBuffers();
		return;
	}

	if(fileLoaded) {
		if (renderer->IsFloatingPoint()) {
			if (controls.GetExclusiveIndicator().IsExclusive()) {
				BASS_WASAPI_GetData(buffer, fftFlag);

				// Scale back up to 100% volume
				auto inverseVolume = controls.GetVolume().GetInverseVolume();
				for (auto i = 0; i < bufferLength; ++i)
					floatBuffer[i] *= inverseVolume;

			} else BASS_ChannelGetData(streamHandle, buffer, fftFlag);
		} else {
			if (controls.GetExclusiveIndicator().IsExclusive()) {
				BASS_WASAPI_GetData(buffer, static_cast<DWORD>(bufferLength * sizeof(float) * channelInfo.chans));

				// Scale back up to 100% volume
				auto inverseVolume = controls.GetVolume().GetInverseVolume();
				for (auto i = 0; i < bufferLength * channelInfo.chans; ++i)
					shortBuffer[i] = static_cast<short>(floatBuffer[i] * inverseVolume * std::numeric_limits<short>::max());
			}
			else BASS_ChannelGetData(streamHandle, buffer, static_cast<DWORD>(bufferLength * sizeof(short) * channelInfo.chans));
		}
	} else {
		std::unique_lock lock(audioSink->mutex);
		if (audioSink->dataChanged) {
			//std::cout << audioSink->buffer[0] << std::endl;
			for (int i = 0, j = audioSink->currentBufferPos; i < bufferLength * 2; i++) {
				// Average the channels together
				in[i] = (audioSink->buffer[j] + audioSink->buffer[j + 1]) / 2.0f;
				j += 2;
				if(j >= bufferLength * 4)
					j = 0;
			}

			fftw_execute(plan);

			maxHeardSample = std::numeric_limits<float>::lowest();
			for (int i = 0; i < bufferLength; ++i) {
				// Get the magnitude
				floatBuffer[i] = static_cast<float>(
					std::sqrt(
						std::pow(out[i][0], 2) +
						std::pow(out[i][1], 2)
					)
				);

				if (floatBuffer[i] > maxHeardSample)
					maxHeardSample = floatBuffer[i];
			}

			audioSink->dataChanged = false;
		}
	}

	//rect.x = 0;
	//rect.y = 0;
	//rect.w = buffer[0]/1000000;
	//rect.h = 10;

	if (renderer->IsFloatingPoint())
		lightPack.NextSamples(floatBuffer, bufferLength);

	renderer->OnLoop(
		time,
		fileLoaded,
		hStep,
		maxHeardSample,
		resetGain
	);

	//}

	// If more than half of our bins
	// grew larger (and we've not seen
	// this for at least a half second)
	// change the color
	/*
	if ((timeSinceLastColorChange += time.change.AsSeconds()) >= 0.5 &&
		detectBpm &&
		maxUpdates >= bufferLength / 2 / 2) {
		albumArt.NextBin(true);
		timeSinceLastColorChange = 0.0;
	}
	*/

	if (resetGain) resetGain = false;

	//++frameCount;
	if (rotating) {
		auto changeInSeconds = static_cast<float>(time.change.AsSeconds());
		frameCount += changeInSeconds * rotationSpeed;
		while (frameCount >= 360.0f)
			frameCount -= 360.0f;
	}

	if (strobe) {
		strobeAccum += time.change;
		if (strobeAccum >= strobeFrequency) {
			albumArt.NextBin(true);
			strobeAccum -= strobeFrequency;
		}
	}
			
	renderer->Draw(time, frameCount, GetColor());

	if (!shuttingDown)
		lightPack.OnLoop((albumArt.Loaded() && !overrideColor) ? albumArt.GetColor() : visColor);

	if (blur) {
		glDisable(GL_BLEND);
		glAccum(GL_MULT, blurIntensity);
		glAccum(GL_ACCUM, 1 - blurIntensity);
		glAccum(GL_RETURN, 1.0);
		glEnable(GL_BLEND);
	}

	// Draw album art OVER the accumulation buffer
	// since we don't want it getting blurry
	albumArt.OnLoop(
		windowWidth / 2.0f,
		windowHeight / 2.0f,
		frameCount
	);

	// Render the controls over the accumulation buffer, too
	auto elapsed = controls.OnLoop(time, streamHandle, [this](float alpha) { SetColor(alpha); });

	// If we reached the end of the song, try loading the next
	// song in the playlist
	if (advanceOnNextLoop) {
		LoadFile(controls.GetPlaylist().Current()->path, true);
		advanceOnNextLoop = false;
	} else if (auto &cue = controls.GetPlaylist().GetCue();
		(!controls.GetExclusiveIndicator().IsExclusive() || cue) && elapsed >= controls.GetCurrentSongLength()) {
		if (auto next = controls.GetPlaylist().Next()) {
			CConsole::Console.Print("Reached the end of the current song and loading the next", MSG_DIAG);

			LoadFile(next->path, true);

			if (next->startTime > DBL_EPSILON)
				SeekTo(next->startTime);
		} else {
			if (controls.GetExclusiveIndicator().IsExclusive())
				StopExclusive(FALSE);
			else
				Stop(FALSE);
		}
	}

	if (stopWasapiOnNextLoop) {
		StopExclusive(FALSE);
		stopWasapiOnNextLoop = false;
	}

	if(beatDetect->OnLoop(elapsed - (controls.GetExclusiveIndicator().IsExclusive() ? exclusiveBufferSize : 0)))
		albumArt.NextBin(true);

	SwapBuffers();
}

void CApp::SwapBuffers() {
	controls.GetFpsCounter().OnFrame();
	SDL_GL_SwapWindow(sdlWindow);
}

void CApp::OnDestroy() {
	shuttingDown = true;

	if (keyboardHook) {
		UnhookWindowsHookEx(keyboardHook);
		keyboardHook = nullptr;
	}

	for (auto &detector : beatDetectors)
		detector.Cancel();

	if (listening) {
		audioSink->done = true;
		if (listenThread.joinable())
			listenThread.join();
		listening = false;
	}

	CConsole::Console.OnCleanup();

	lightPack.OnDestroy();
	albumArt.OnDestroy();

	albumArt.RemoveColorChangeListener(this);

	controls.OnDestroy();

	BASS_WASAPI_Free();
	BASS_Free();
	IMG_Quit();
}

HSTREAM CApp::OpenWithFlags(const std::filesystem::path &path, const std::string &extension, DWORD flags) {
	auto ret = BASS_StreamCreateFile(FALSE, path.wstring().c_str(), 0, 0, flags);
	if (!ret) {
		// In case our plugins didn't properly load
		if (extension == ".flac")
			ret = BASS_FLAC_StreamCreateFile(FALSE, path.wstring().c_str(), 0, 0, flags);
		else if (extension == ".ape")
			ret = BASS_APE_StreamCreateFile(FALSE, path.wstring().c_str(), 0, 0, flags);
		else if (extension == ".wv")
			ret = BASS_WV_StreamCreateFile(FALSE, path.wstring().c_str(), 0, 0, flags);
		else
			ret = BASS_StreamCreateFile(FALSE, path.wstring().c_str(), 0, 0, flags);
	}

	return ret;
}

void CApp::Open(const std::filesystem::path &path, const std::string &extension, bool exclusive) {
	if (exclusive) {
		if (wasapiInfo.freq != channelInfo.freq) {
			if (wasapiInfo.freq != 0) StopExclusive(TRUE);

			// Only swap out the handle _after_ we've stopped
			// as StopExclusive(TRUE) frees the handle
			streamHandle = OpenWithFlags(path, extension, BASS_STREAM_PRESCAN | BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT);

			if (BASS_WASAPI_Init(device, channelInfo.freq, channelInfo.chans, BASS_WASAPI_BUFFER | BASS_WASAPI_EXCLUSIVE, exclusiveBufferSize, 0, OutputWasapiProc, reinterpret_cast<void *>(this)) == TRUE) {
				BASS_WASAPI_GetInfo(&wasapiInfo);
				if (wasapiInfo.freq == channelInfo.freq) {
					keyboardHook = SetWindowsHookExA(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
					CConsole::Console.Print("Channel and device frequencies (" + std::to_string(wasapiInfo.freq) + ") match!", MSG_DIAG);

					return;
				} else {
					CConsole::Console.Print("Could not initialize exclusive mode! Error code " + std::to_string(BASS_ErrorGetCode()), MSG_ERROR);
					exclusive = false;
				}
			} else {
				CConsole::Console.Print("Could not initialize exclusive mode! Error code " + std::to_string(BASS_ErrorGetCode()), MSG_ERROR);
				exclusive = false;
			}
		} else {
			streamHandle = OpenWithFlags(path, extension, BASS_STREAM_PRESCAN | BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT);
			return;
		}
	}

	if (!exclusive) {
		BASS_WASAPI_Free();
		controls.GetExclusiveIndicator().SetExclusive(false);
		BASS_StreamFree(streamHandle);
		streamHandle = OpenWithFlags(path, extension, BASS_STREAM_PRESCAN);
	}
}

void CApp::Stop(BOOL reset) {
	BASS_ChannelStop(streamHandle);

	if (reset == TRUE)
		BASS_StreamFree(streamHandle);
}

void CApp::StopExclusive() {
	stopWasapiOnNextLoop = true;
}

void CApp::StopExclusive(BOOL reset) {
	BASS_WASAPI_Stop(reset);

	if (keyboardHook) {
		UnhookWindowsHookEx(keyboardHook);
		keyboardHook = nullptr;
	}

	if (reset == TRUE) {
		BASS_WASAPI_Free();
		BASS_StreamFree(streamHandle);
		wasapiInfo = { 0 };
	}
}

void CApp::Unmute() {
	// Unmute system volume if it's muted
	if (BASS_WASAPI_GetMute(1) == TRUE) {
		if (BASS_WASAPI_SetMute(1, FALSE) == FALSE)
			CConsole::Console.Print("Could not unmute system volume!", MSG_ALERT);
	}
}

void CApp::LoadBeats(
	HSTREAM streamHandle,
	std::filesystem::path path // not a reference because we pass this to the callback lambda
) {
	// When we have a FLAC + .cue, we only
	// want to analyze the current _song_,
	// not the whole file
	const auto &cue = controls.GetPlaylist().GetCue();

	// No matter what, we're done with our _current_ detector
	beatDetect->Reset();

	auto nextDetector = (beatDetect == &beatDetectors[0] ? &beatDetectors[1] : &beatDetectors[0]);

	if (nextDetector->GetState() > BeatDetect::State::Idle) {
		auto temp = nextDetector;

		CConsole::Console.Print("Ping-ponging beat detectors", MSG_DIAG);
		nextDetector = beatDetect;
		beatDetect = temp;
	} else {
		beatDetect->OnLoad(
			streamHandle,
			channelInfo.freq,
			channelInfo.chans,
			[this, path] {
				CConsole::Console.Print("Beat detection finished for the current song in the playlist (" + path.stem().u8string() + ")!", MSG_DIAG);
				beatDetect->SeekTo(controls.GetCurrentPosition());
			},
			cue ? cue->GetCurrentTrack()->startTime : static_cast<std::optional<double>>(std::nullopt),
			cue ? controls.GetCurrentSongLength() : static_cast<std::optional<double>>(std::nullopt)
		);
	}

	if (const auto &next = const_cast<const Playlist &>(controls.GetPlaylist()).Next()) {
		auto nextExtension = next->path.extension().u8string();
		std::transform(nextExtension.begin(), nextExtension.end(), nextExtension.begin(), tolower);
		auto nextHandle = OpenWithFlags(next->path, nextExtension, BASS_STREAM_PRESCAN | BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT);

		BASS_CHANNELINFO nextChannelInfo;
		BASS_ChannelGetInfo(nextHandle, &nextChannelInfo);

		nextDetector->OnLoad(
			nextHandle,
			nextChannelInfo.freq,
			nextChannelInfo.chans,
			[this, next, nextDetector] {
				if (nextDetector->IsDetecting())
					CConsole::Console.Print("Beat detection finished for the next song in the playlist (" + next->path.stem().u8string() + ")!", MSG_DIAG);
			},
			next->startTime > DBL_EPSILON ? next->startTime : static_cast<std::optional<double>>(std::nullopt),
			controls.GetNextSongLength()
		);
	}
}

void CApp::ResetBeatDetection() {
	CConsole::Console.Print("Resetting beat detectors", MSG_DIAG);
	for (auto &detector : beatDetectors) {
		detector.Reset();
	}
}

void CApp::LoadFile(std::filesystem::path path, bool fromPlaylist) {
	auto extension = path.extension().u8string();
	std::transform(extension.begin(), extension.end(), extension.begin(), tolower);

	auto originalPath = extension.empty() ? path : "";

	// If we're still in the same file,
	// just try to get updated tags from
	// the cue sheet and run beat detection
	// on the new song
	if (path == loadedFile) {
		controls.LoadFromCue();

		for (auto &detector : beatDetectors)
			detector.Cancel();

		LoadBeats(
			OpenWithFlags(path, extension, BASS_STREAM_PRESCAN | BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT),
			path
		);

		return;
	}

	if (!fromPlaylist) {
		// If we're not in a playlist, _reset_
		// any current beat detectors.
		for (auto &detector : beatDetectors)
			detector.Reset();

		if (std::filesystem::is_directory(path) || controls.GetPlaylist().IsCue(extension)) {
			path = controls.GetPlaylist().OnLoad(
				path,
				extension,
				[this](const std::filesystem::path &path, const std::string &extension, DWORD flags) {
					return OpenWithFlags(path, extension, flags); 
				}
			)->path;

			extension = path.extension().u8string();
			std::transform(extension.begin(), extension.end(), extension.begin(), tolower);
		} else {
			controls.GetPlaylist().Clear();
		}
	} else {
		// If we're in a playlist, we want the
		// folder that was originally scanned
		// for songs
		originalPath = controls.GetPlaylist().GetPath();

		// If we're in a playlist, cancel any
		// current beat detectors
		for (auto &detector : beatDetectors)
			detector.Cancel();
	}

	albumArt.Reset(visColor);

	overrideColor = false;

	// If we try to load an image directly,
	// use that as the album art
	if (AlbumArt::IsSupported(extension)) {
		albumArt.Load(path, originalPath, true);
		return;
	}

	if (fileLoaded && !controls.GetExclusiveIndicator().IsExclusive()) {
		Stop();
		
		Open(path, extension, controls.GetExclusiveIndicator().IsExclusive());

		// Don't reset gain if we're changing songs
		// in a playlist.
		if (!fromPlaylist) {
			resetGain = true;

			renderer->Reset();
			resetGain = dynamicGain.reset;
		}
	}

	// We want this as a local variable, as it's handed off to BeatDetect
	auto streamHandle = OpenWithFlags(path, extension, BASS_STREAM_PRESCAN | BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT);

	if (streamHandle) {
		BASS_ChannelGetInfo(streamHandle, &channelInfo);

		renderer->SetNumberOfChannels(
			// chans is a DWORD, but I can't imagine
			// many cases where we have > 255 channels
			static_cast<uint8_t>(channelInfo.chans)
		);

		controls.OnLoad(streamHandle);

		LoadBeats(streamHandle, path);

		// If we're loading our first file
		// or we didn't auto-advance, explicitly
		// open the file. Auto-advancing handles
		// opening the file in the WASAPI proc.
		if (!fileLoaded || !advanceOnNextLoop)
			Open(path, extension, controls.GetExclusiveIndicator().IsExclusive());

		metadata.OnLoad(
			path,
			extension,
			this->streamHandle,
			&controls,
			&albumArt
		);

		// Always look for external art,
		// in case it's higher resolution
		// than the embedded
		albumArt.Load(path.wstring(), originalPath);

		// Once we have our final art,
		// scale it down
		albumArt.Scale();

		// If we have any tags from the cue
		// sheet, load them _after_ everything
		// else
		controls.LoadFromCue();

		if (controls.GetExclusiveIndicator().IsExclusive()) {
			// Only unmute if this is the first / only song
			if (!fromPlaylist)
				Unmute();

			// If we're loading our first file
			// or we didn't auto-advance, explicitly
			// start playback. Auto-advancing never
			// _stops_ playback.
			if (!fileLoaded || !advanceOnNextLoop)
				BASS_WASAPI_Start();
		} else {
			BASS_ChannelPlay(this->streamHandle, false);
		}

		fileLoaded = true;
		loadedFile = path;
		loadedFileExtension = extension;
	}
}

void CApp::PlayPreparedFile() {
	LoadFile(savedFile);
}

void CApp::SetColor(int r, int g, int b) {
	visColor.r = std::clamp(r, 0, 255) / 255.0f;
	visColor.g = std::clamp(g, 0, 255) / 255.0f;
	visColor.b = std::clamp(b, 0, 255) / 255.0f;

	// If the user explicitly changes the color
	// while album art is loaded, assume
	// they want to override the colors selected
	// from the album art
	if (albumArt.Loaded())
		overrideColor = true;
}

void CApp::SetDecayTime(Duration<Microseconds> time) {
	if (auto fftRenderer = dynamic_cast<FFTRenderer *>(renderer))
		fftRenderer->SetDecayTime(time);

	SetBufferLength(bufferLength);
}
void CApp::SetFadeTime(Duration<Microseconds> time) {
	if (auto fftRenderer = dynamic_cast<FFTRenderer *>(renderer))
		fftRenderer->SetFadeTime(time);

	SetBufferLength(bufferLength);
}
void CApp::SetStrobeFrequency(Duration<Microseconds> freq) {
	strobeFrequency = freq;
}

void CApp::ToggleBlur() {
	blur = !blur;
}

void CApp::SetBlurIntensity(float intensity) {
	blurIntensity = intensity;
}

void CApp::Seek(double seconds) {
	auto bytes = BASS_ChannelSeconds2Bytes(
		streamHandle,
		seconds
	);

	// BASS_POS_RELATIVE gives BASS_ERROR_NOTAVAIL,
	// so manually calculate the relative position
	auto pos = BASS_ChannelGetPosition(streamHandle, BASS_POS_BYTE);
	auto absolute = BASS_ChannelBytes2Seconds(streamHandle, pos + bytes);

	CConsole::Console.Print(
		"Seeking by " +
			std::to_string(seconds) +
			" seconds (" +
			std::to_string(absolute) +
			")",
		MSG_DIAG
	);

	if (BASS_ChannelSetPosition(streamHandle, pos + bytes, BASS_POS_BYTE) == FALSE) {
		auto code = BASS_ErrorGetCode();
		CConsole::Console.Print("Seek failed! Error code " + std::to_string(code), MSG_ERROR);
	} else {
		// Refresh our times
		controls.SetElapsedSeconds(-1);

		beatDetect->SeekTo(absolute);
	}
}

void CApp::SeekTo(double seconds) {
	if (BASS_ChannelSetPosition(
			streamHandle,
			BASS_ChannelSeconds2Bytes(
				streamHandle,
				seconds
			),
			BASS_POS_BYTE
	) == FALSE) {
		auto code = BASS_ErrorGetCode();
		CConsole::Console.Print("Seek failed! Error code " + std::to_string(code), MSG_ERROR);
	} else {
		// Refresh our times
		controls.SetElapsedSeconds(-1);

		if (auto &cue = controls.GetPlaylist().GetCue())
			beatDetect->SeekTo(seconds - cue->GetCurrentTrack()->startTime);
		else
			beatDetect->SeekTo(seconds);
	}
}

void CApp::FadeControls(bool in) {
	controls.Fade(in);
}

void CApp::TogglePlaying() {
	if (streamHandle) {
		// If we hit the end of the song, we might be in an error state
		if (auto pos = BASS_ChannelGetPosition(streamHandle, BASS_POS_BYTE);
			pos == -1 || BASS_ChannelBytes2Seconds(streamHandle, pos) >= controls.GetCurrentFileLength())
			SeekTo(0.0);

		if (controls.GetExclusiveIndicator().IsExclusive()) {
			if (BASS_WASAPI_IsStarted())
				BASS_WASAPI_Stop(FALSE);
			else
				BASS_WASAPI_Start();
		} else {
			if (BASS_ChannelIsActive(streamHandle) != BASS_ACTIVE_PLAYING)
				BASS_ChannelPlay(streamHandle, FALSE);
			else
				BASS_ChannelPause(streamHandle);
		}
	}
}

void CApp::ToggleFullscreen() {
	if (SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_FULLSCREEN_DESKTOP)
		SDL_SetWindowFullscreen(sdlWindow, 0);
	else
		SDL_SetWindowFullscreen(sdlWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
}

void CApp::NextTrack() {
	if (auto next = controls.GetPlaylist().Next()) {
		LoadFile(next->path, true);

		if (next->startTime > DBL_EPSILON)
			SeekTo(next->startTime);
	}
}

void CApp::PreviousTrack() {
	if (auto prev = controls.GetPlaylist().Previous()) {
		// Since we have the _next_ track preloaded
		// for beat detection, we need to reset
		ResetBeatDetection();

		LoadFile(prev->path, true);

		if (prev->startTime > DBL_EPSILON)
			SeekTo(prev->startTime);
	}
}

void CApp::OnMouseClicked(const Vector2i &mousePos) {
	// Playlist::OnMouseClicked automatically advances
	// our playlist to the clicked position, so we have
	// to figure out what our next track is _before_
	// that happens.
	const auto next = const_cast<const Playlist &>(controls.GetPlaylist()).Next();

	if (mousePos.y >= windowHeight - Controls::SeekbarSize * scale) {
		double time = (static_cast<double>(mousePos.x) / windowWidth) * controls.GetCurrentSongLength();

		if (auto &cue = controls.GetPlaylist().GetCue())
			time += cue->GetCurrentTrack()->startTime;

		SeekTo(time);
	} else if (auto file = controls.GetPlaylist().OnMouseClicked(mousePos)) {
		// If we didn't select the _next_ song in the playlist,
		// we need to reset beat detection.
		if (!next || file->path != next->path)
			ResetBeatDetection();

		LoadFile(file->path, true);

		SeekTo(file->startTime);
	} else if (auto toggled = controls.GetExclusiveIndicator().OnMouseClicked(mousePos)) {
		// Store our elapsed time before freeing
		// the handle
		auto elapsed = BASS_ChannelBytes2Seconds(
			streamHandle,
			BASS_ChannelGetPosition(streamHandle, BASS_POS_BYTE)
		);

		const auto exclusive = controls.GetExclusiveIndicator().IsExclusive();
		
		if (exclusive)
			StopExclusive(TRUE);
		else
			Stop();

		// Wait to toggle until AFTER we've stopped
		// the current stream
		controls.GetExclusiveIndicator().SetExclusive(
			!exclusive
		);

		Open(loadedFile, loadedFileExtension, controls.GetExclusiveIndicator().IsExclusive());

		// Restore our last position
		BASS_ChannelSetPosition(
			streamHandle,
			BASS_ChannelSeconds2Bytes(
				streamHandle,
				elapsed
			),
			BASS_POS_BYTE
		);

		if (controls.GetExclusiveIndicator().IsExclusive()) {
			Unmute();
			BASS_WASAPI_Start();
		} else {
			BASS_Start();
			BASS_ChannelPlay(streamHandle, false);
		}
	}
}

void CApp::LoadPreset(std::size_t index) {
	if (const auto &presets = Preset::GetPresets(); presets.size() > index) {
		const auto &preset = presets.at(index);

		CConsole::Console.Print("Loading preset '" + preset.GetName() + "'", MSG_DIAG);

		SetBufferLength(preset.GetBufferSize());
		SetDecayTime(preset.GetDecayTime());
		SetFadeTime(preset.GetFadeTime());
		renderer->SetPulse(preset.GetPulse());
		renderer->SetPulseTime(preset.GetPulseTime());

		presetIndex = index;
	}
}

void CApp::OnColorChanged(const MathsCPP::Colour<float> &color, bool silent) {
	if (!silent)
		CConsole::Console.Print("Color changed!", MSG_DIAG);

	// Simple, linear function
	//SetGamma(2.8f - color.ToHsv().s * 2.0f);

	/*
	SetGamma(
		std::clamp(
			4.0 - std::pow(4, color.ToHsv().s),
			0.8, // Minimum gamma of 0.8... anything lower is WAY too saturated
			2.8 // Maximum gamma of 2.8... anything > 3 just washes everything out
		)
	);
	*/

	// y = 3.8 - 2.5^x
	// where y {2.8, 1.3} when x {0.0, 1.0}
	//SetGamma(3.8 - std::pow(2.5, color.ToHsv().s));

	// Seems to be the best-fitting curve so far
	//lightPack.SetGamma(4 - std::pow(2.7, color.ToHsv().s), silent);

	// Biasing towards lower gammas
	// 
	// "Let's Just Live"'s primary color looks better at 1.5
	// gamma vs. the above function's selected 2.04
	lightPack.SetGamma(3.5f - std::pow(2.7f, color.ToHsv().s), silent);
}