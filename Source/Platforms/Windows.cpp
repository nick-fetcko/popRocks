#ifdef WIN32

#include "Source/Platforms/Windows.hpp"

#include <winuser.h>
#include <shellapi.h>

#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>

#include "Source/CApp.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "Comctl32.lib")

#if defined _M_IX86
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

// From wingdi.h
#define RGB(r,g,b)          ((COLORREF)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))

HHOOK keyboardHook = nullptr;
HHOOK msgBoxHook = nullptr;

enum class KeyboardHookMode {
	None,
	MediaKeys,
	Volume,
	Both
};

KeyboardHookMode keyboardHookMode = KeyboardHookMode::None;

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
	CreateInterop();

	UpdateKeyboardHookMode();
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
#ifndef _DEBUG
	if (RegOpenKey(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell", &registryKey) != ERROR_SUCCESS) {
		if (RegCreateKey(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell", &registryKey) == ERROR_SUCCESS)
			RegCloseKey(registryKey);
	}

	registryKey = nullptr;

	if (RegOpenKey(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\Visualize with popRocks", &registryKey) != ERROR_SUCCESS) {
		if (RegCreateKey(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\Visualize with popRocks", &registryKey) != ERROR_SUCCESS) {
			registryKey = nullptr;
		}
	}

	if (registryKey) {
		if (RegSetValue(registryKey, L"", REG_SZ, L"Visualize with popRocks", 0) == ERROR_SUCCESS) {
			// FIXME: Make this DRY
			int wargc;
			if (LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &wargc); wargv) {
				std::wstring path(wargv[0]);
				path.insert(path.begin(), L'\"');
				path += L"\"";
				RegSetKeyValue(registryKey, NULL, L"Icon", REG_SZ, path.c_str(), path.length() * sizeof(wchar_t));
				path += L" \"%1\"";
				RegSetValue(registryKey, L"command", REG_SZ, path.c_str(), 0);
			}
		}

		RegCloseKey(registryKey);
		registryKey = nullptr;
	}

	if (RegOpenKey(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\popRocks.exe", &registryKey) == ERROR_SUCCESS) {
		// FIXME: Make this DRY
		int wargc;
		if (LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &wargc); wargv) {
			std::filesystem::path path(wargv[0]);
			std::wstring string;
			string.insert(string.begin(), L'\"');
			string += path.parent_path().wstring();
			string += L"\\Data\\popRocks-document-tall.ico\"";

			RegSetValue(registryKey, L"DefaultIcon", REG_SZ, string.c_str(), 0);
		}

		RegCloseKey(registryKey);
		registryKey = nullptr;
	}
#endif

	if (auto ret = CoInitialize(NULL); ret == S_OK || ret == S_FALSE) {
		CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER,
			IID_ITaskbarList3, reinterpret_cast<void**>(&taskbar));
	}

	dxgi.OnInit(args);

	HookKeyboard();

	if (app->GetVulkan())
		Desktop::OnInit(args, context);
}

void Windows::OnDestroy() {
	if (taskbar) {
		taskbar->Release();
		taskbar = nullptr;
	}

	CoUninitialize();

	if (keyboardHook) {
		UnhookWindowsHookEx(keyboardHook);
		keyboardHook = nullptr;
	}

	DestroyInterop();

	if (listening) {
		audioSink->done = true;
		if (listenThread.joinable())
			listenThread.join();
		listening = false;
	}

	BASS_WASAPI_Free();

#ifndef _DEBUG
	if (mutex) {
		ReleaseMutex(mutex);
		mutex = nullptr;
	}

	SDL_UnregisterApp();
#endif
}

void Windows::OnResize(int windowWidth, int windowHeight) {
	if (auto &context = app->GetContext()) {
		if (!app->GetVulkan()) {
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
		} else {
			Desktop::OnResize(windowWidth, windowHeight);
		}
	}
}

std::optional<bool> Windows::OnLoop() {
	if (!app->GetVulkan() && HDR::Enabled)
		return dxgi.OnLoop();
	else if (app->GetVulkan())
		return interop->OnLoop();

	return true;
}

void Windows::SwapBuffers() {
	if (!app->GetVulkan() && HDR::Enabled)
		dxgi.SwapBuffers();
	else
		SDL_GL_SwapWindow(app->GetSdlWindow());
}

// -----------------------------------------------------
// ------------------ Keyboard hooks -------------------
// -----------------------------------------------------
void Windows::HookKeyboard() {
	if (keyboardHook) {
		UnhookWindowsHookEx(keyboardHook);
		keyboardHook = nullptr;
	}

	UpdateKeyboardHookMode();

	if (keyboardHookMode != KeyboardHookMode::None)
		keyboardHook = SetWindowsHookExA(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
}

// -----------------------------------------------------
// ---------------------- OpenGL -----------------------
// -----------------------------------------------------
bool Windows::CreateOpenGlContext() {
	app->SetOpenGlContext(SDL_GL_CreateContext(
		app->GetVulkan() ? 
			app->GetOpenGlWindow() :
			app->GetSdlWindow()
	));

	return app->GetOpenGlContext() != nullptr;
}

void Windows::OpenOpenGlWindow(SDL_PropertiesID &props) {
	if (app->GetVulkan())
		Desktop::OpenOpenGlWindow(props);
}

// -----------------------------------------------------
// --------------------- Interops ----------------------
// -----------------------------------------------------
void Windows::DestroyInterop() {
	if (!app->GetVulkan()) {
		if (HDR::Enabled)
			dxgi.OnDestroy();
	}

	if (interop != &dxgi) {
		interop->OnDestroy();
		delete interop;
		interop = nullptr;
	}
}

void Windows::CreateInterop() {
	if (app->GetVulkan())
		interop = new Vulkan();
	else
		interop = &dxgi;
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
bool Windows::OpenExclusive(const std::filesystem::path &path, const std::string &extension, bool exclusive, HSTREAM &target, HSTREAM &visualTarget, bool force, const BASS_CHANNELINFO &channelInfo, void *data) {
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
			target = OpenWithFlags(path, extension, BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT);
			visualTarget = OpenWithFlags(path, extension, BASS_STREAM_PRESCAN | BASS_STREAM_DECODE);

			exclusiveBufferBytes = BASS_ChannelSeconds2Bytes(target, exclusiveBufferSize);

			LogDebug("exclusiveBufferBytes = ", exclusiveBufferBytes);

			BASS_WASAPI_GetInfo(&wasapiInfo);
			if (wasapiInfo.freq == channelInfo.freq) {
				HookKeyboard();
				LogDebug("Channel and device frequencies (", wasapiInfo.freq, ") match!");

				exclusiveBufferSize = BASS_ChannelBytes2Seconds(target, wasapiInfo.buflen);

				return exclusive;
			} else {
				const auto error = app->GetBassError(BASS_ErrorGetCode());
				LogError("Could not initialize exclusive mode! Error: ", error);
				ShowDialogBox("Could not initialize exclusive mode!", "Error: " + error);
				exclusive = false;
			}
		} else {
			const auto error = app->GetBassError(BASS_ErrorGetCode());
			LogError("Could not initialize exclusive mode! Error: ", error);
			ShowDialogBox("Could not initialize exclusive mode!", "Error: " + error);
			exclusive = false;
		}
	} else {
		target = OpenWithFlags(path, extension, BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT);
		visualTarget = OpenWithFlags(path, extension, BASS_STREAM_PRESCAN | BASS_STREAM_DECODE);

		exclusiveBufferBytes = BASS_ChannelSeconds2Bytes(target, exclusiveBufferSize);

		LogDebug("exclusiveBufferBytes = ", exclusiveBufferBytes);

		return exclusive;
	}

	if (!exclusive && BASS_WASAPI_GetDevice() != -1)
		BASS_WASAPI_Free();

	return exclusive;
}

void Windows::StopExclusive(bool reset) {
	BASS_WASAPI_Stop(reset ? TRUE : FALSE);

	HookKeyboard();

	if (reset && BASS_WASAPI_GetDevice() != -1)
		BASS_WASAPI_Free();

	wasapiInfo = { 0 };

	exclusiveBufferBytes = 0;

	LogDebug("exclusiveBufferBytes = ", exclusiveBufferBytes);
}

// -----------------------------------------------------
// ---------------------- HDR --------------------------
// -----------------------------------------------------
std::optional<std::tuple<bool, float, float>> Windows::GetHdrProperties(int display, bool force) {
	return dxgi.GetHdrProperties(display, force);
}

void Windows::SetHdr(bool enabled, void *hwnd, int width, int height) {
	hwnd = SDL_GetPointerProperty(SDL_GetWindowProperties(app->GetSdlWindow()), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);

	if (!enabled && HDR::Enabled) {
		if (app->GetVulkan())
			Desktop::SetHdr(enabled, hwnd, width, height);
		else {
			if (auto &blurFbo = app->GetBlurFbo())
				blurFbo->SetDefaultFramebuffer(0);
			if (auto &lastFrame = app->GetLastFrame())
				lastFrame->SetDefaultFramebuffer(0);
			if (auto &uiFbo = app->GetUiFbo())
				uiFbo->SetDefaultFramebuffer(0);

			if (auto font = app->GetControls().GetFont())
				font->SetDefaultFramebuffer(0);
			if (auto boldFont = app->GetControls().GetBoldFont())
				boldFont->SetDefaultFramebuffer(0);
			if (auto outlineFont = app->GetControls().GetOutlineFont())
				outlineFont->SetDefaultFramebuffer(0);
			if (auto boldOutlineFont = app->GetControls().GetBoldOutlineFont())
				boldOutlineFont->SetDefaultFramebuffer(0);

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
		}
	} else if (enabled && !HDR::Enabled) {	
		int width = 0, height = 0;
		SDL_GetWindowSize(app->GetSdlWindow(), &width, &height);
		if (app->GetBlur()) {
			app->SetBlurFbo(std::make_unique<MultisampledFramebufferObject>(app->GetMaxDimension(), app->GetMaxDimension(), enabled ? GL_RGBA16F : GL_RGBA));
			app->SetLastFrame(std::make_unique<MultisampledFramebufferObject>(app->GetMaxDimension(), app->GetMaxDimension(), enabled ? GL_RGBA16F : GL_RGBA));
		}

		app->SetUiFbo(std::make_unique<MultisampledFramebufferObject>(app->GetWindowSize().first, app->GetWindowSize().second, enabled ? GL_RGBA16F : GL_RGBA, IsUiInverted()));

		if (app->GetVulkan()) {
			GetInterop()->SetHdr(enabled,
#ifdef WIN32
				hwnd,
				width,
				height
#endif
			);
		} else {
			dxgi.OnCreate(reinterpret_cast<HWND>(hwnd), width, height);
			dxgi.OnResize(width, height);
		}

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

		ShowDialogBox("SDL_DXGIGetOutputInfo() failed!", SDL_GetError());
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

std::filesystem::path Windows::GetTemporaryFile(const std::string &pattern) {
	return "";
}

// -----------------------------------------------------
// ---------------------- BASS -------------------------
// -----------------------------------------------------
void Windows::LoadBassPlugins() {
	if (!BASS_PluginLoad("bassflac.dll", 0)) {
		LogError("Could not load FLAC plugin! Error code ", BASS_ErrorGetCode());

		ShowDialogBox("Could not load FLAC plugin!", "Error: " + app->GetBassError(BASS_ErrorGetCode()));
	}
	if (!BASS_PluginLoad("bassape.dll", 0)) {
		LogError("Could not load APE plugin! Error code ", BASS_ErrorGetCode());

		ShowDialogBox("Could not load APE plugin!", "Error: " + app->GetBassError(BASS_ErrorGetCode()));
	}
	if (!BASS_PluginLoad("basswv.dll", 0)) {
		LogError("Could not load WavPack plugin! Error code ", BASS_ErrorGetCode());

		ShowDialogBox("Could not load WavPack plugin!", "Error: " + app->GetBassError(BASS_ErrorGetCode()));
	}
	if (!BASS_PluginLoad("bass_tta.dll", 0)) {
		LogError("Could not load TTA plugin! Error code ", BASS_ErrorGetCode());

		ShowDialogBox("Could not load TTA plugin!", "Error: " + app->GetBassError(BASS_ErrorGetCode()));
	}
}

// -----------------------------------------------------
// -------------------- Fullscreen ---------------------
// -----------------------------------------------------
void Windows::ToggleFullscreen() {
	// It appears Windows captures Alt-Enter when using DXGI
	if (!app->GetVulkan() && HDR::Enabled)
		return;

	if (SDL_GetWindowFlags(app->GetSdlWindow()) & SDL_WINDOW_FULLSCREEN) {
		if (!app->GetVulkan() && HDR::Enabled)
			dxgi.GetSwapChain()->SetFullscreenState(FALSE, NULL);
		else
			SDL_SetWindowFullscreen(app->GetSdlWindow(), 0);
	} else {
		if (!app->GetVulkan() && HDR::Enabled)
			dxgi.GetSwapChain()->SetFullscreenState(TRUE, NULL);
		else
			SDL_SetWindowFullscreen(app->GetSdlWindow(), SDL_WINDOW_FULLSCREEN);
	}
}

// -----------------------------------------------------
// -------------------- Miniplayer ---------------------
// -----------------------------------------------------
void Windows::SetChromaKey(bool enabled) {
	if (enabled == colorKeyEnabled) return;

	colorKeyEnabled = enabled;

	const auto hwnd = reinterpret_cast<HWND>(
		SDL_GetPointerProperty(
			SDL_GetWindowProperties(app->GetSdlWindow()),
			SDL_PROP_WINDOW_WIN32_HWND_POINTER,
			NULL
		)
	);

	auto flags = (enabled ? LWA_COLORKEY : 0);
	if (!app->IsFileLoaded())
		flags |= LWA_ALPHA;

	if (enabled) {
		SetWindowLongPtr(hwnd, GWL_EXSTYLE, GetWindowLongPtr(hwnd, GWL_EXSTYLE) & (~WS_EX_LAYERED));
		SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);

		// Value of R = 1, G = 2, B = 3 chosen completely arbitrarily
		SetLayeredWindowAttributes(hwnd, RGB(1, 2, 3), 0xFF, flags);
	} else {
		SetWindowLongPtr(hwnd, GWL_EXSTYLE, GetWindowLongPtr(hwnd, GWL_EXSTYLE) & (~WS_EX_LAYERED));

		// FIXME: I've found no way to invalidate the
		//        window in a way that restores proper
		//        transparency without having one,
		//        final post-resize resize. Quickly hiding
		//        the window and then showing it has the
		//        same effect, but with a visible flicker.
		//
		// InvalidateRect(), UpdateWindow(), SetWindowPos()
		// with SWP_NOSIZE | SWP_NOMOVE | SWP_FRAMECHANGED, etc.
		// have no effect.
		SetWindowPos(
			Settings::settings.GetMiniPlayerX() - 1,
			Settings::settings.GetMiniPlayerY() - 1,
			Settings::settings.GetMiniPlayerWidth() * app->GetScale() + 2,
			Settings::settings.GetMiniPlayerHeight() * app->GetScale() + 2
		);

		SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);

		SetLayeredWindowAttributes(hwnd, 0, 0xFF, flags);
	}

	LogDebug("ColorKey ", enabled ? "Enabled" : "Disabled");
}

void Windows::SetMiniPlayer(bool miniPlayer, uint8_t chromaKey) {
	const auto hwnd = reinterpret_cast<HWND>(
		SDL_GetPointerProperty(
			SDL_GetWindowProperties(app->GetSdlWindow()),
			SDL_PROP_WINDOW_WIN32_HWND_POINTER,
			NULL
		)
	);

	if (miniPlayer) {
		SetLastError(0);
		SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) & (~WS_EX_LAYERED));
		auto style = SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);

		auto ret = SetLayeredWindowAttributes(hwnd, RGB(chromaKey, chromaKey, chromaKey), 0xFF, 0);

		if (auto error = GetLastError())
			LogError("Chroma key could not be set! ret = ", ret, " error = ", error);
		else
			LogDebug("Chroma key set to ", static_cast<int>(chromaKey));
	} else {
		SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) & (~WS_EX_LAYERED));
	}
}

bool Windows::SetTransparent(bool transparent) {
	if (transparent == this->transparent) return false;

	const auto hwnd = reinterpret_cast<HWND>(
		SDL_GetPointerProperty(
			SDL_GetWindowProperties(app->GetSdlWindow()),
			SDL_PROP_WINDOW_WIN32_HWND_POINTER,
			NULL
		)
	);

	if (transparent)
		SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_TRANSPARENT);
	else
		SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) & (~WS_EX_TRANSPARENT));

	this->transparent = transparent;

	return true;
}

bool Windows::AllowsWindowMovement() const {
	return true;
}

std::optional<Vector2i> Windows::SetWindowPos(int x, int y, int width, int height) {
	std::optional<Vector2i> ret = std::nullopt;

	const auto hwnd = reinterpret_cast<HWND>(
		SDL_GetPointerProperty(
			SDL_GetWindowProperties(app->GetSdlWindow()),
			SDL_PROP_WINDOW_WIN32_HWND_POINTER,
			NULL
		)
	);

	if (x == SDL_WINDOWPOS_CENTERED ||
		y == SDL_WINDOWPOS_CENTERED) {
		// Center on the _primary_ monitor
		//
		// From https://devblogs.microsoft.com/oldnewthing/20141106-00/?p=43683
		const POINT ptZero = { 0, 0 };
		const auto monitor = MonitorFromPoint(ptZero, MONITOR_DEFAULTTOPRIMARY);

		MONITORINFO info = { 0 };
		info.cbSize = sizeof(info);

		GetMonitorInfoW(monitor, &info);

		if (x == SDL_WINDOWPOS_CENTERED)
			x = (info.rcMonitor.right - info.rcMonitor.left) / 2 - width / 2;

		if (y == SDL_WINDOWPOS_CENTERED)
			y = (info.rcMonitor.bottom - info.rcMonitor.top) / 2 - height / 2;

		ret = { x, y };
	}

	MoveWindow(hwnd, x, y, width, height, FALSE);
	
	return ret;
}

void Windows::ShowDialogBox(const std::string &title, const std::string &message) {
	const auto hwnd = reinterpret_cast<HWND>(
		SDL_GetPointerProperty(
			SDL_GetWindowProperties(app->GetSdlWindow()),
			SDL_PROP_WINDOW_WIN32_HWND_POINTER,
			NULL
		)
	);

	const auto instance = reinterpret_cast<HINSTANCE>(
		SDL_GetPointerProperty(
			SDL_GetWindowProperties(app->GetSdlWindow()),
			SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER,
			NULL
		)
	);

	const auto wideTitle = Utils::ToUTF16(title);
	const auto wideMessage = Utils::ToUTF16(message);

	msgBoxHook = SetWindowsHookEx(WH_CBT, MessageBoxCbtHookProc, nullptr, GetCurrentThreadId());

	TASKDIALOGCONFIG config = { 0 };
	config.cbSize = sizeof(config);
	config.hInstance = instance;
	config.dwCommonButtons = TDCBF_OK_BUTTON;
	config.pszMainIcon = TD_ERROR_ICON;
	config.pszMainInstruction = wideTitle.c_str();
	config.pszContent = wideMessage.c_str();
	config.pButtons = NULL;
	config.cButtons = 0;
	config.hwndParent = hwnd;
	config.dwFlags = TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT;

	TaskDialogIndirect(&config, NULL, NULL, NULL);
}

bool Windows::HandleExistingWindow() {
#ifndef _DEBUG
	std::stringstream mutexNameStream;
	mutexNameStream << "Local\\" << GUID;

	mutex = CreateMutexA(0, FALSE, mutexNameStream.str().c_str());
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		if (mutex) {
			ReleaseMutex(mutex);
			mutex = nullptr;
		}

		// Find existing window
		if (auto existing = FindWindow(WindowClassName.data(), L"popRocks"); existing) {
			// FIXME: Make this DRY
			int wargc;
			if (LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &wargc); wargv && wargc > 1) {
				COPYDATASTRUCT cds;
				cds.dwData = 1;
				cds.cbData = (lstrlenW(wargv[1])) * sizeof(WCHAR);
				cds.lpData = reinterpret_cast<PVOID>(wargv[1]);

				SendMessage(existing, WM_COPYDATA, static_cast<WPARAM>(NULL), reinterpret_cast<LPARAM>(&cds));
			}
		}

		return true;
	}

	{
		wchar_t path[MAX_PATH] = { 0 };
		if (GetModuleFileNameW(NULL, path, MAX_PATH) > 0) {
			std::filesystem::current_path(std::filesystem::path(path).parent_path());
		}
	}

	SDL_RegisterApp(Utils::ToUTF8(WindowClassName.data()).c_str(), CS_BYTEALIGNCLIENT | CS_OWNDC, nullptr);
#endif

	return false;
}

// =====================================================
// ===================== Virtuals ======================
// =====================================================

// -----------------------------------------------------
// ------------------ CApp Helpers ---------------------
// -----------------------------------------------------
bool Windows::OnMouseClicked(const Vector2i &mousePos) {
	if (auto toggled = app->GetControls().GetExclusiveIndicator().OnMouseClicked(mousePos)) {
		LogInfo("Click captured! Windows exclusive toggle");
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
			app->GetVulkan() ? 
				app->GetOpenGlWindow() :
				app->GetSdlWindow()
		),
		&adapterIndex, &outputIndex)) {
		LogError(
			"SDL_DXGIGetOutputInfo() failed: ",
			SDL_GetError()
		);

		ShowDialogBox("SDL_DXGIGetOutputInfo() failed!", SDL_GetError());
	}

	return adapterIndex;
}

int Windows::GetDefaultFramebuffer() {
	return (HDR::Enabled || app->GetVulkan()) ? GetInterop()->GetFramebuffer() : 0;
}

// -----------------------------------------------------
// ----------------- Exclusive mode --------------------
// -----------------------------------------------------
bool Windows::LoadExclusive(double pos) {
	if (app->GetControls().GetExclusiveIndicator().IsExclusive() && app->Open(app->GetLoadedFile(), app->GetLoadedFileExtension(), true, app->GetStreamHandle(), app->GetVisualStreamHandle(), true)) {
		app->SeekTo(pos);
		BASS_WASAPI_Start();
		app->SetPlaying(true);

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
			BASS_ChannelPause(app->GetStreamHandle());
			BASS_WASAPI_Stop(TRUE);

			const auto bytes = BASS_ChannelSeconds2Bytes(
				app->GetStreamHandle(),
				exclusiveBufferSize
			);

			if (pausePos = BASS_ChannelGetPosition(app->GetStreamHandle(), BASS_POS_BYTE); pausePos > bytes) {
				if (BASS_ChannelSetPosition(app->GetStreamHandle(), pausePos - bytes, BASS_POS_BYTE) == FALSE) {
					LogWarning("Could not step stream handle back after pausing!");
					pausePos = 0;
				}
			} else pausePos = 0;

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

// -----------------------------------------------------
// ---------------- Window Management ------------------
// -----------------------------------------------------
void Windows::HookWindow(bool miniPlayer) {
	const auto hwnd = reinterpret_cast<HWND>(
		SDL_GetPointerProperty(
			SDL_GetWindowProperties(app->GetSdlWindow()),
			SDL_PROP_WINDOW_WIN32_HWND_POINTER,
			NULL
		)
	);

	SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

	sdlWndProc = reinterpret_cast<WNDPROC>(GetWindowLongPtr(hwnd, GWLP_WNDPROC));
	SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc));
}

// -----------------------------------------------------
// ---------------------- Bling ------------------------
// -----------------------------------------------------
void Windows::SetStatus(Status status, int progress) {
	if (taskbar && (status != lastStatus || progress != lastProgress)) {
		const auto hwnd = reinterpret_cast<HWND>(
			SDL_GetPointerProperty(
				SDL_GetWindowProperties(app->GetSdlWindow()),
				SDL_PROP_WINDOW_WIN32_HWND_POINTER,
				NULL
			)
		);

		if (status != lastStatus) {
			TBPFLAG flag;
			switch (status) {
			case Status::Paused:
				flag = TBPF_PAUSED;
				break;
			case Status::Playing:
				flag = TBPF_NORMAL;
				break;
			default:
				flag = TBPF_NOPROGRESS;
				break;
			}
		
			taskbar->SetProgressState(hwnd, flag);
			lastStatus = status;
		}

		if (progress != lastProgress && progress != 0) {
			taskbar->SetProgressValue(hwnd, progress, 100);
			lastProgress = progress;
		}
	}
}

// =====================================================
// ================= Private Functions =================
// =====================================================
void Windows::UpdateKeyboardHookMode() {
	keyboardHookMode =
		Settings::settings.GetCaptureKeyboardMediaKeys() ?
			Settings::settings.GetExclsuive() ?
				KeyboardHookMode::Both :
				KeyboardHookMode::MediaKeys :
			Settings::settings.GetExclsuive() ?
				KeyboardHookMode::Volume :
				KeyboardHookMode::None;

	std::string string;
	switch (keyboardHookMode) {
	case KeyboardHookMode::None:
		string = "none";
		break;
	case KeyboardHookMode::MediaKeys:
		string = "media keys";
		break;
	case KeyboardHookMode::Volume:
		string = "volume";
		break;
	case KeyboardHookMode::Both:
		string = "both";
		break;
	default:
		string = "unknown";
		break;
	}

	LogDebug("KeyboardHookMode = ", string);
}

// =====================================================
// ===================== Callbacks =====================
// =====================================================
const std::map<DWORD, SDL_Keycode> ExclusiveKeyMap = {
	{ VK_VOLUME_DOWN, SDLK_VOLUMEDOWN },
	{ VK_VOLUME_UP, SDLK_VOLUMEUP }
};

const std::map<DWORD, SDL_Keycode> KeyMap = {
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

	const auto exclusiveIter = (keyboardHookMode == KeyboardHookMode::Volume || keyboardHookMode == KeyboardHookMode::Both) ?
		ExclusiveKeyMap.find(hookStruct->vkCode) :
		ExclusiveKeyMap.end();
	const auto iter = (keyboardHookMode == KeyboardHookMode::MediaKeys || keyboardHookMode == KeyboardHookMode::Both) ?
		KeyMap.find(hookStruct->vkCode) :
		KeyMap.end();

	// From https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelkeyboardproc
	// 
	// If the hook procedure processed the message, it may return
	// a nonzero value to prevent the system from passing the message
	// to the rest of the hook chain or the target window procedure.
	if (exclusiveIter != ExclusiveKeyMap.end() ||
		iter != KeyMap.end()) {
		if (wParam == WM_KEYDOWN) {
			SDL_Event event;
			event.type = SDL_EVENT_KEY_DOWN;
			event.key.key = exclusiveIter == ExclusiveKeyMap.end() ? iter->second : exclusiveIter->second;

			SDL_PushEvent(&event);
		}

		return 1;
	}

	return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}

LRESULT CALLBACK MessageBoxCbtHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode == HCBT_ACTIVATE) {
		const auto hwndMsgBox = reinterpret_cast<HWND>(wParam);

		BOOL useDarkMode = TRUE;
		DWORD dwmAttribute = DWMWA_USE_IMMERSIVE_DARK_MODE;

		DwmSetWindowAttribute(hwndMsgBox, dwmAttribute, &useDarkMode, sizeof(useDarkMode));

		UnhookWindowsHookEx(msgBoxHook);
		msgBoxHook = nullptr;
	}
	return CallNextHookEx(nullptr, nCode, wParam, lParam);
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
	if (BASS_ChannelIsActive(app->GetStreamHandle()))
		c = BASS_ChannelGetData(app->GetStreamHandle(), buffer, length);
	
	if (c < length) {
		if (auto next = app->GetNextStreamHandle(); next && BASS_ChannelIsActive(next)) {
			// Immediately start adding new samples to the buffer
			// for gapless playback
			c += BASS_ChannelGetData(next, &reinterpret_cast<uint8_t*>(buffer)[c], length - c - 1);

			if (!BASS_ChannelIsActive(next)) {
				c |= BASS_STREAMPROC_END;

				// Can't kill WASAPI from inside WASAPI,
				// so we also have to do this on the next loop
				app->StopExclusive();
			} else {
				// Update the UI on the next loop
				app->AdvanceToNextTrack();
			}

		} else if (!c) {
			// Can't kill WASAPI from inside WASAPI,
			// so we also have to do this on the next loop
			app->StopExclusive();

			return BASS_STREAMPROC_END;
		}
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

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	Windows *const platform = reinterpret_cast<Windows *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

	if (msg == WM_COPYDATA) {
		COPYDATASTRUCT *pcds = reinterpret_cast<COPYDATASTRUCT *>(lParam);

		platform->GetApp()->LoadFile(
			std::wstring(
				reinterpret_cast<wchar_t *>(pcds->lpData),
				reinterpret_cast<wchar_t *>(pcds->lpData) + pcds->cbData / sizeof(WCHAR)
			)
		);
		return TRUE;
	} else if (msg == WM_ENTERSIZEMOVE)
		platform->SetFilterPaused(false);
	else if (msg == WM_EXITSIZEMOVE)
		platform->SetFilterPaused(true);

	return platform->GetSdlWndProc()(hwnd, msg, wParam, lParam);
}

#endif