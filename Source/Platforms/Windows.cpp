#ifdef WIN32

#include "Source/Platforms/Windows.hpp"

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>

#include "Source/CApp.h"

HHOOK keyboardHook = nullptr;

// =====================================================
// ================ Factory Registration ===============
// =====================================================
bool Windows::Register() {
	PlatformFactory::Register("windows", [](CApp *app) {
		return std::make_unique<Windows>(app);
	});

	return true;
}
bool Windows::registered = Register();

// =====================================================
// =================== Implementation ==================
// =====================================================
Windows::Windows(CApp *app) : Desktop(app), dxgi(false) {
#if VULKAN
	interop = new Vulkan();
#else
	interop = &dxgi;
#endif
}

Windows::~Windows() {
	delete audioSink;

	if (interop != &dxgi)
		delete interop;
}

// =====================================================
// =================== Pure Virtuals ===================
// =====================================================

// -----------------------------------------------------
// ------------------- CApp Helpers --------------------
// -----------------------------------------------------
void Windows::OnInit(Interop::InitArgs args, Context &context) {
	dxgi.OnInit(args);

#if VULKAN
	Desktop::OnInit(args, context);
#endif
}

void Windows::OnDestroy() {
	if (keyboardHook) {
		UnhookWindowsHookEx(keyboardHook);
		keyboardHook = nullptr;
	}

#if !VULKAN
	if (HDR::Enabled)
		dxgi.OnDestroy();
#endif
	if (interop != &dxgi)
		interop->OnDestroy();

	if (listening) {
		audioSink->done = true;
		if (listenThread.joinable())
			listenThread.join();
		listening = false;
	}

	BASS_WASAPI_Free();
}

void Windows::OnResize(int windowWidth, int windowHeight) {
	if (auto &context = app->GetContext()) {
#if !VULKAN
#ifdef WIN32
		if (HDR::Enabled) {
			dxgi.OnResize(windowWidth, windowHeight);
			context->SetIdentity(glm::ortho(0.0f, static_cast<float>(windowWidth), 0.0f, static_cast<float>(windowHeight)));
		} else {
#endif
			context->SetIdentity(glm::ortho(0.0f, static_cast<float>(windowWidth), static_cast<float>(windowHeight), 0.0f));
			context->Apply();
#ifdef WIN32
		}
#endif
#else
		Desktop::OnResize(windowWidth, windowHeight);
#endif
	}
}

std::optional<bool> Windows::OnLoop() {
	if (!VULKAN && HDR::Enabled)
		return dxgi.OnLoop();
	else if (VULKAN)
		return interop->OnLoop();

	return true;
}

void Windows::SwapBuffers() {
	if (!VULKAN && HDR::Enabled)
		dxgi.SwapBuffers();
	else
		SDL_GL_SwapWindow(app->GetSdlWindow());
}

// -----------------------------------------------------
// ------------------ Keyboard hooks -------------------
// -----------------------------------------------------
void Windows::HookKeyboard() {
	keyboardHook = SetWindowsHookExA(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
}

// -----------------------------------------------------
// ---------------------- OpenGL -----------------------
// -----------------------------------------------------
bool Windows::CreateOpenGlContext() {
	app->SetOpenGlContext(SDL_GL_CreateContext(
#if VULKAN
		app->GetOpenGlWindow()
#else
		app->GetSdlWindow()
#endif
	));

	return app->GetOpenGlContext() != nullptr;
}

void Windows::OpenOpenGlWindow(SDL_PropertiesID &props) {
#if VULKAN
	Desktop::OpenOpenGlWindow(props);
#endif
}

// -----------------------------------------------------
// ---------------- Device listening -------------------
// -----------------------------------------------------
void Windows::Listen(bool loopback) {
	StopListening();

	SetGain(1.0f);

	in = reinterpret_cast<float *>(fftwf_malloc(sizeof(float) * maxLength * 2));
	out = reinterpret_cast<fftwf_complex *>(fftwf_malloc(sizeof(fftwf_complex) * maxLength * 2));
	plan = fftwf_plan_dft_r2c_1d(static_cast<int>(maxLength * 2), in, out, FFTW_MEASURE);

	audioSink = new MyAudioSink(maxLength * 4 /* we're assuming stereo, for now */);
	audioSink->loopback = loopback;

	if (loopback) {
		BASS_DEVICEINFO info;
		if (BASS_GetDeviceInfo(
			loopback ?
			Platform::GetDeviceIndex<true>(Settings::settings.GetOutputDevice()) :
			Platform::GetDeviceIndex<false>(Settings::settings.GetInputDevice()),
			&info
		)
			) {
			audioSink->deviceName = Utils::ToUTF16(info.driver);
		}
	} else {
		BASS_WASAPI_DEVICEINFO info;
		if (BASS_WASAPI_GetDeviceInfo(Platform::GetDeviceIndex<false>(Settings::settings.GetInputDevice()), &info))
			audioSink->deviceName = Utils::ToUTF16(info.id);
	}

	listenThread = std::thread(RecordAudioStream, audioSink);

	listening = true;
}

void Windows::StopListening() {
	if (listening) {
		audioSink->done = true;
		if (listenThread.joinable())
			listenThread.join();

		fftwf_free(in);
		fftwf_free(out);
		fftwf_destroy_plan(plan);

		delete audioSink;

		listening = false;
	}
}

void Windows::LoadHeardSamples(Renderer *renderer, float *floatBuffer, short *shortBuffer, const std::size_t bufferLength) {
	std::unique_lock lock(audioSink->mutex);
	if (audioSink->dataChanged) {
		if (renderer->IsFloatingPoint()) {
			//std::cout << audioSink->buffer[0] << std::endl;
			int j = audioSink->currentBufferPos - maxLength * 2;
			if (j < 0)
				j = maxLength * 4 + j;

			for (int i = 0; i < maxLength * 2; ++i) {
				// Average the channels together
				in[i] = (audioSink->buffer[j] + audioSink->buffer[j + 1]) / 2.0f;
				j += 2;
				if (j >= maxLength * 4)
					j = 0;
			}

			fftwf_execute(plan);

			maxHeardSample = std::numeric_limits<float>::lowest();

			for (int i = 1; i <= maxLength; ++i) {
				// Get the magnitude
				floatBuffer[i - 1] = static_cast<float>(
					std::sqrt(
						std::pow(out[i][0], 2) +
						std::pow(out[i][1], 2)
					)
					);

				if (floatBuffer[i - 1] > maxHeardSample)
					maxHeardSample = floatBuffer[i - 1];
			}

			// Normalize
			for (int i = 0; i < maxLength; ++i)
				floatBuffer[i] /= maxHeardSample;

		} else {
			int j = audioSink->currentBufferPos - bufferLength * 2;
			if (j < 0)
				j = maxLength * 4 + j;

			for (int i = 0; i < bufferLength; i++) {
				shortBuffer[i * 2] = audioSink->buffer[j] * std::numeric_limits<short>::max();
				shortBuffer[i * 2 + 1] = audioSink->buffer[j + 1] * std::numeric_limits<short>::max();
				j += 2;
				if (j >= maxLength * 4)
					j = 0;
			}
		}

		audioSink->dataChanged = false;
	}
}

// -----------------------------------------------------
// ----------------- Exclusive mode --------------------
// -----------------------------------------------------
bool Windows::OpenExclusive(const std::filesystem::path &path, const std::string &extension, bool exclusive, HSTREAM &target, bool force, const BASS_CHANNELINFO &channelInfo, void *data) {
	if (wasapiInfo.freq != channelInfo.freq || force) {
		if (wasapiInfo.freq != 0) StopExclusive(TRUE);

		auto outputDevice = Platform::GetDeviceIndex<true>(Settings::settings.GetOutputDevice());

		// BASS and BASS_WASAPI use different device indices
		if (outputDevice != -1) {
			BASS_DEVICEINFO info;
			BASS_GetDeviceInfo(outputDevice, &info);

			BASS_WASAPI_DEVICEINFO wasapiInfo;
			for (int i = 0; BASS_WASAPI_GetDeviceInfo(i, &wasapiInfo); ++i) {
				if ((wasapiInfo.flags & BASS_DEVICE_ENABLED) &&
					!(wasapiInfo.flags & BASS_DEVICE_INPUT) &&
					(strncmp(wasapiInfo.id, info.driver, std::min(strlen(wasapiInfo.id), strlen(info.driver))) == 0)) {
					outputDevice = i;
					break;
				}
			}
		}

		if (BASS_WASAPI_Init(outputDevice, channelInfo.freq, channelInfo.chans, BASS_WASAPI_BUFFER | BASS_WASAPI_EXCLUSIVE, exclusiveBufferSize, 0, OutputWasapiProc, data) == TRUE) {
			// Only swap out the handle _after_ we've stopped
			// as StopExclusive(TRUE) frees the handle
			target = OpenWithFlags(path, extension, BASS_STREAM_PRESCAN | BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT);

			BASS_WASAPI_GetInfo(&wasapiInfo);
			if (wasapiInfo.freq == channelInfo.freq) {
				HookKeyboard();
				LogDebug("Channel and device frequencies (", wasapiInfo.freq, ") match!");

				return exclusive;
			} else {
				LogError("Could not initialize exclusive mode! Error code ", BASS_ErrorGetCode());
				exclusive = false;
			}
		} else {
			LogError("Could not initialize exclusive mode! Error code ", BASS_ErrorGetCode());
			exclusive = false;
		}
	} else {
		target = OpenWithFlags(path, extension, BASS_STREAM_PRESCAN | BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT);
		return exclusive;
	}

	if (!exclusive && BASS_WASAPI_GetDevice() != -1)
		BASS_WASAPI_Free();

	return exclusive;
}

void Windows::StopExclusive(bool reset) {
	BASS_WASAPI_Stop(reset ? TRUE : FALSE);

	if (keyboardHook) {
		UnhookWindowsHookEx(keyboardHook);
		keyboardHook = nullptr;
	}

	if (reset && BASS_WASAPI_GetDevice() != -1)
		BASS_WASAPI_Free();

	wasapiInfo = { 0 };
}

// -----------------------------------------------------
// ---------------------- HDR --------------------------
// -----------------------------------------------------
std::optional<std::tuple<bool, float, float>> Windows::GetHdrProperties(int display, bool force) {
	return dxgi.GetHdrProperties(display, force);
}

void Windows::SetHdr(bool enabled, void *hwnd, int width, int height) {
	if (!enabled) {
#if VULKAN
		Desktop::SetHdr(enabled, hwnd, width, height);
#else
		if (auto &blurFbo = app->GetBlurFbo())
			blurFbo->SetDefaultFramebuffer(0);
		if (auto &lastFrame = app->GetLastFrame())
			lastFrame->SetDefaultFramebuffer(0);
		if (auto &uiFbo = app->GetUiFbo())
			uiFbo->SetDefaultFramebuffer(0);

		if (auto font = app->GetControls().GetFont())
			font->SetDefaultFramebuffer(0);
		if (auto outlineFont = app->GetControls().GetOutlineFont())
			outlineFont->SetDefaultFramebuffer(0);

		if (auto &context = app->GetContext()) {
			context->SetIdentity(glm::ortho(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f));
			context->Apply();
		}

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplSDL3_Shutdown();

		// FIXME: It appears that calling swapChain->Present(1, 0)
		//        prevents us from restoring the window's original
		//        OpenGL context.
		//
		//        Destroying the window is only a workaround until
		//        a better solution is found.
		SDL_DestroyWindow(app->GetSdlWindow());

		SDL_PropertiesID props = SDL_CreateProperties();

		SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "popRocks");
		SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, app->GetWindowSize().first);
		SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, app->GetWindowSize().second);
		SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, Settings::settings.GetWindowX());
		SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, Settings::settings.GetWindowY());
		SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
		SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);

		app->SetSdlWindow(SDL_CreateWindowWithProperties(
			props
		));

		SDL_GL_MakeCurrent(app->GetSdlWindow(), app->GetOpenGlContext());

		// Setup Platform/Renderer backends
		ImGui_ImplSDL3_InitForOpenGL(app->GetSdlWindow(), app->GetOpenGlContext());
		ImGui_ImplOpenGL3_Init();
#endif
	} else {
		HWND hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(app->GetSdlWindow()), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
		int width = 0, height = 0;
		SDL_GetWindowSize(app->GetSdlWindow(), &width, &height);
		if (app->GetBlur()) {
			app->SetBlurFbo(std::make_unique<MultisampledFramebufferObject>(app->GetMaxDimension(), app->GetMaxDimension(), enabled ? GL_RGBA16F : GL_RGBA));
			app->SetLastFrame(std::make_unique<MultisampledFramebufferObject>(app->GetMaxDimension(), app->GetMaxDimension(), enabled ? GL_RGBA16F : GL_RGBA));
		}

		app->SetUiFbo(std::make_unique<MultisampledFramebufferObject>(app->GetWindowSize().first, app->GetWindowSize().second, enabled ? GL_RGBA16F : GL_RGBA, IsUiInverted()));

#if VULKAN
		GetInterop()->SetHdr(enabled,
#ifdef WIN32
			hwnd,
			width,
			height
#endif
		);
#else
		dxgi.OnCreate(hwnd, width, height);
		dxgi.OnResize(width, height);
#endif

		Desktop::SetHdr(enabled, hwnd, width, height);
	}
}

void Windows::UpdateHdrProperties(bool force) {
	// SDL does NOT update white level or headroom
	// when the window moves between monitors with 
	// different HDR properties on Windows
	int adapterIndex = 0;
	int outputIndex = 0;

	if (!SDL_GetDXGIOutputInfo(SDL_GetDisplayForWindow(app->GetSdlWindow()),
		&adapterIndex, &outputIndex)) {
		LogError(
			"SDL_DXGIGetOutputInfo() failed: ",
			SDL_GetError()
		);
	}
	Desktop::UpdateHdrProperties(outputIndex, force);
}

// -----------------------------------------------------
// ------------------ File Opening ---------------------
// -----------------------------------------------------
HSTREAM Windows::OpenWithFlags(const std::filesystem::path &path, const std::string &extension, DWORD flags) {
	auto ret = BASS_StreamCreateFile(
		FALSE,
		path.wstring().c_str(),
		0,
		0,
		flags
	);
	if (!ret) {
		// In case our plugins didn't properly load
		if (extension == ".flac") {
			ret = BASS_FLAC_StreamCreateFile(
				FALSE,
				path.wstring().c_str(),
				0,
				0,
				flags
			);
		} else if (extension == ".ape") {
			ret = BASS_APE_StreamCreateFile(
				FALSE,
				path.wstring().c_str(),
				0,
				0,
				flags
			);
		} else if (extension == ".wv") {
			ret = BASS_WV_StreamCreateFile(
				FALSE,
				path.wstring().c_str(),
				0,
				0,
				flags
			);
		}
		else if (extension == ".tta") {
			ret = BASS_TTA_StreamCreateFile(
				FALSE,
				path.wstring().c_str(),
				0,
				0,
				flags
			);
		} else {
			ret = BASS_StreamCreateFile(
				FALSE,
				path.wstring().c_str(),
				0,
				0,
				flags
			);
		}
	}

	return ret;
}

// -----------------------------------------------------
// ---------------------- BASS -------------------------
// -----------------------------------------------------
void Windows::LoadBassPlugins() {
	if (!BASS_PluginLoad("bassflac.dll", 0))
		LogError("Could not load FLAC plugin! Error code ", BASS_ErrorGetCode());
	if (!BASS_PluginLoad("bassape.dll", 0))
		LogError("Could not load APE plugin! Error code ", BASS_ErrorGetCode());
	if (!BASS_PluginLoad("basswv.dll", 0))
		LogError("Could not load WavPack plugin! Error code ", BASS_ErrorGetCode());
	if (!BASS_PluginLoad("bass_tta.dll", 0))
		LogError("Could not load TTA plugin! Error code ", BASS_ErrorGetCode());
}

// -----------------------------------------------------
// -------------------- Fullscreen ---------------------
// -----------------------------------------------------
void Windows::ToggleFullscreen() {
	// It appears Windows captures Alt-Enter when using DXGI
#if !VULKAN
	return;

	BOOL fullscreen = FALSE;
	if (HDR::Enabled)
		dxgi.GetSwapChain()->GetFullscreenState(&fullscreen, NULL);
#endif

	if (SDL_GetWindowFlags(app->GetSdlWindow()) & SDL_WINDOW_FULLSCREEN
#if !VULKAN
		|| fullscreen
#endif
		) {
#if !VULKAN
		if (HDR::Enabled)
			dxgi.GetSwapChain()->SetFullscreenState(FALSE, NULL);
		else
#endif
			SDL_SetWindowFullscreen(app->GetSdlWindow(), 0);
	} else {
#if !VULKAN
		if (HDR::Enabled)
			dxgi.GetSwapChain()->SetFullscreenState(TRUE, NULL);
		else
#endif
			SDL_SetWindowFullscreen(app->GetSdlWindow(), SDL_WINDOW_FULLSCREEN);
	}
}

// =====================================================
// ===================== Virtuals ======================
// =====================================================

// -----------------------------------------------------
// ------------------ CApp Helpers ---------------------
// -----------------------------------------------------
bool Windows::OnMouseClicked(const Vector2i &mousePos) {
	if (auto toggled = app->GetControls().GetExclusiveIndicator().OnMouseClicked(mousePos)) {
		app->ToggleExclusive();
		return true;
	}

	return false;
}

// -----------------------------------------------------
// --------------- Display Properties ------------------
// -----------------------------------------------------
int Windows::GetAdapterIndex() {
	int adapterIndex = 0;
	int outputIndex = 0;

	if (!SDL_GetDXGIOutputInfo(
		SDL_GetDisplayForWindow(
#if VULKAN
			app->GetOpenGlWindow()
#else
			app->GetSdlWindow()
#endif
		),
		&adapterIndex, &outputIndex)) {
		LogError(
			"SDL_DXGIGetOutputInfo() failed: ",
			SDL_GetError()
		);
	}

	return adapterIndex;
}

int Windows::GetDefaultFramebuffer() {
	return (HDR::Enabled || VULKAN) ? GetInterop()->GetFramebuffer() : 0;
}

// -----------------------------------------------------
// ----------------- Exclusive mode --------------------
// -----------------------------------------------------
bool Windows::LoadExclusive(double pos) {
	if (app->GetControls().GetExclusiveIndicator().IsExclusive() && app->Open(app->GetLoadedFile(), app->GetLoadedFileExtension(), true, app->GetStreamHandle(), true)) {
		app->SeekTo(pos);
		BASS_WASAPI_Start();
		app->SetPlaying(true);

		return true;
	}

	return false;
}

bool Windows::ScaleExclusive(Renderer *renderer, uint8_t *buffer, float *floatBuffer, short *shortBuffer) {
	if (app->GetControls().GetExclusiveIndicator().IsExclusive()) {
		if (renderer->IsFloatingPoint()) {

			BASS_WASAPI_GetData(buffer, app->GetFftFlag());

			// Scale back up to 100% volume
			const auto inverseVolume = app->GetControls().GetVolume().GetInverseVolume();
			for (auto i = 0; i < app->GetBufferLength(); ++i)
				floatBuffer[i] *= inverseVolume;

		} else {
			BASS_WASAPI_GetData(buffer, static_cast<DWORD>(app->GetBufferLength() * sizeof(float) * app->GetChannelInfo().chans));

			// Scale back up to 100% volume
			const auto inverseVolume = app->GetControls().GetVolume().GetInverseVolume();
			for (auto i = 0; i < app->GetBufferLength() * app->GetChannelInfo().chans; ++i)
				shortBuffer[i] = static_cast<short>(floatBuffer[i] * inverseVolume * std::numeric_limits<short>::max());
		}

		return true;
	}

	return false;
}

bool Windows::StartPlayingExclusive(bool fromPlaylist, bool fileLoaded, bool advanceOnNextLoop) {
	if (app->GetControls().GetExclusiveIndicator().IsExclusive()) {
		// Only unmute if this is the first / only song
		if (!fromPlaylist)
			Unmute();

		// If we're loading our first file
		// or we didn't auto-advance, explicitly
		// start playback. Auto-advancing never
		// _stops_ playback.
		if (!fileLoaded || !advanceOnNextLoop)
			BASS_WASAPI_Start();

		return true;
	}

	return false;
}

bool Windows::StartExclusive() {
	if (app->GetControls().GetExclusiveIndicator().IsExclusive()) {
		Unmute();
		BASS_WASAPI_Start();

		app->SetPlaying(true);

		return true;
	}

	return false;
}

bool Windows::StopPlayingExclusive() {
	if (app->GetControls().GetExclusiveIndicator().IsExclusive()) {
		if (BASS_WASAPI_IsStarted()) {
			BASS_WASAPI_Stop(FALSE);
			app->SetPlaying(false);
		} else {
			BASS_WASAPI_Start();
			app->SetPlaying(true);
		}

		return true;
	}

	return false;
}

// -----------------------------------------------------
// -------------------- Filepaths ----------------------
// -----------------------------------------------------
std::filesystem::path Windows::GetNativePath(const std::filesystem::path &path) {
	return path.wstring(); 
}

void Windows::GetPicturesPath(std::stringstream &filename) {
	WCHAR picturesPath[MAX_PATH];
	if (SHGetFolderPath(NULL, CSIDL_MYPICTURES, NULL, SHGFP_TYPE_CURRENT, picturesPath) == S_OK) {
		filename << Utils::ToUTF8(std::wstring(picturesPath)) << "/popRocks";
		if (!std::filesystem::exists(filename.str()))
			std::filesystem::create_directory(filename.str());
		filename << "/";
	}
}

// -----------------------------------------------------
// ---------------------- Audio ------------------------
// -----------------------------------------------------
void Windows::Unmute() {
	// Unmute system volume if it's muted
	if (BASS_WASAPI_GetMute(1) == TRUE) {
		if (BASS_WASAPI_SetMute(1, FALSE) == FALSE)
			LogWarning("Could not unmute system volume!");
	}
}

// Polymorphic helper for GetDeviceIndex<Output>
bool Windows::GetDeviceIndex(int &index, const std::string &device) {
	BASS_WASAPI_DEVICEINFO info;

	for (; index != -1 && BASS_WASAPI_GetDeviceInfo(index, &info); ++index) {
		if (strncmp(info.id, device.c_str(), std::min(strlen(info.id), device.size())) == 0)
			return true;
	}

	return false;
}

// =====================================================
// ===================== Callbacks =====================
// =====================================================
const std::map<DWORD, SDL_Keycode> KeyMap = {
	{ VK_VOLUME_DOWN, SDLK_VOLUMEDOWN },
	{ VK_VOLUME_UP, SDLK_VOLUMEUP },
	{ VK_MEDIA_NEXT_TRACK, SDLK_MEDIA_NEXT_TRACK },
	{ VK_MEDIA_PREV_TRACK, SDLK_MEDIA_PREVIOUS_TRACK },
	{ VK_MEDIA_PLAY_PAUSE, SDLK_MEDIA_PLAY }
};

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
			event.type = SDL_EVENT_KEY_DOWN;
			event.key.key = KeyMap.at(hookStruct->vkCode);
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

	std::unique_lock lock(app->GetStreamHandleMutex());

	// Derived from https://forum.team-mediaportal.com/threads/music-gapless-playback.121377/post-1025539
	DWORD c = 0;
	if (BASS_ChannelIsActive(app->GetStreamHandle())) {
		c = BASS_ChannelGetData(app->GetStreamHandle(), buffer, length);
	} else if (auto next = app->GetNextStreamHandle(); next && BASS_ChannelIsActive(next)) {
		// Immediately start adding new samples to the buffer
		// for gapless playback
		c = BASS_ChannelGetData(next, buffer, length);

		if (!BASS_ChannelIsActive(next)) {
			c |= BASS_STREAMPROC_END;

			// Can't kill WASAPI from inside WASAPI,
			// so we also have to do this on the next loop
			app->StopExclusive();
		} else {
			// Update the UI on the next loop
			app->AdvanceToNextTrack();
		}

	} else {
		// Can't kill WASAPI from inside WASAPI,
		// so we also have to do this on the next loop
		app->StopExclusive();

		return BASS_STREAMPROC_END;
	}

	lock.unlock();

	if (app->GetControls().GetVolume().GetVolumeControl()) {
		auto floatBuffer = reinterpret_cast<float *>(buffer);
		const auto volume = app->GetControls().GetVolume().GetScaledVolume();
		for (auto i = 0; i < (c & (~BASS_STREAMPROC_END)) / sizeof(float); ++i)
			floatBuffer[i] *= volume;
	}

	return c;
}

#endif