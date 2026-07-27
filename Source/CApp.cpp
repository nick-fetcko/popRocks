#include "CApp.h"

#include <filesystem>
#include <map>
#include <math.h>
#include <cmath>
#include <sstream>
#include <vector>
#include <fstream>

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3/SDL_vulkan.h>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>

#include "MathCPP/Colour.hpp"
#include "OpenGL/OpenGLFont.hpp"

#include "Buffer.hpp"
#include "FFTLineRenderer.hpp"
#include "FFTRenderer.hpp"
#include "Hash.hpp"
#include "HDR.hpp"
#include "ID3V2.hpp"
#include "MP4.hpp"
#include "OscilloscopeRenderer.hpp"

using namespace MathsCPP;

// =====================================================
// ======================= CApp ========================
// =====================================================
std::map<int, std::string> CApp::BassErrorCodes = {
	{ 0, "BASS_OK" },
	{ 1, "BASS_ERROR_MEM" },
	{ 2, "BASS_ERROR_FILEOPEN" },
	{ 3, "BASS_ERROR_DRIVER" },
	{ 4, "BASS_ERROR_BUFLOST" },
	{ 5, "BASS_ERROR_HANDLE" },
	{ 6, "BASS_ERROR_FORMAT" },
	{ 7, "BASS_ERROR_POSITION" },
	{ 8, "BASS_ERROR_INIT" },
	{ 9, "BASS_ERROR_START" },
	{ 10, "BASS_ERROR_SSL" },
	{ 11, "BASS_ERROR_REINIT" },
	{ 13, "BASS_ERROR_TRACK" },
	{ 14, "BASS_ERROR_ALREADY" },
	{ 17, "BASS_ERROR_NOTAUDIO" },
	{ 18, "BASS_ERROR_NOCHAN" },
	{ 19, "BASS_ERROR_ILLTYPE" },
	{ 20, "BASS_ERROR_ILLPARAM" },
	{ 21, "BASS_ERROR_NO3D" },
	{ 22, "BASS_ERROR_NOEAX" },
	{ 23, "BASS_ERROR_DEVICE" },
	{ 24, "BASS_ERROR_NOPLAY" },
	{ 25, "BASS_ERROR_FREQ" },
	{ 27, "BASS_ERROR_NOTFILE" },
	{ 29, "BASS_ERROR_NOHW" },
	{ 31, "BASS_ERROR_EMPTY" },
	{ 32, "BASS_ERROR_NONET" },
	{ 33, "BASS_ERROR_CREATE" },
	{ 34, "BASS_ERROR_NOFX" },
	{ 37, "BASS_ERROR_NOTAVAIL" },
	{ 38, "BASS_ERROR_DECODE" },
	{ 39, "BASS_ERROR_DX" },
	{ 40, "BASS_ERROR_TIMEOUT" },
	{ 41, "BASS_ERROR_FILEFORM" },
	{ 42, "BASS_ERROR_SPEAKER" },
	{ 43, "BASS_ERROR_VERSION" },
	{ 44, "BASS_ERROR_CODEC" },
	{ 45, "BASS_ERROR_ENDED" },
	{ 46, "BASS_ERROR_BUSY" },
	{ 47, "BASS_ERROR_UNSTREAMABLE" },
	{ 48, "BASS_ERROR_PROTOCOL" },
	{ 49, "BASS_ERROR_DENIED" },
	{ 50, "BASS_ERROR_FREEING" },
	{ 51, "BASS_ERROR_CANCEL" },
	{ 500, "BASS_ERROR_JAVA_CLASS" },
	{ -1, "BASS_ERROR_UNKNOWN" }
};

CApp::CApp() : 
	albumArt(&controls, context, platform), 
	controls(&albumArt, platform, vulkan),
	close(&albumArt),
	circleLine(12.0f), 
	prng(
		PRNGFactory<unsigned int>::Build(
			Settings::settings.GetRngSource(), &streamHandle
		)
	) {
	platform = PlatformFactory::Build(
#ifdef WIN32
		"windows"
#elif defined(__ANDROID__)
		"android"
#elif defined(USING_FLATPAK)
		"flatpak"
#else
		"linux"
#endif
		, this
	);

	menu = std::make_unique<Menu>(platform);

	renderer = RendererFactory::Build(
		Settings::settings.GetRenderer(),
		&dynamicGain,
		&albumArt
	);

	// If we close the console, make sure to
	// clean up before exit
	Logger::SetOnClose([this] {
		shuttingDown = true;
	});
	Logger::SetIsClosed([this] {
		return destroyed;
	});

	// Add our console commands
	AddCommands();
}

void CApp::UpdateMaxBufferLength() {
	auto length = std::max(fftLength, bufferLength) * channelInfo.chans;

	if (length != platform->GetMaxLength()) {
		delete[] buffer;
		buffer = new uint8_t[length * sizeof(float)];
		memset(buffer, 0, length * sizeof(float));
		floatBuffer = reinterpret_cast<float *>(buffer);
		shortBuffer = reinterpret_cast<short *>(buffer);

		renderer->SetBuffer(buffer, length);
		resetGain = dynamicGain.reset;

		platform->SetMaxLength(length);
	}
}

void CApp::SetBufferLength(std::size_t bufferLength) {
	auto changed = bufferLength != this->bufferLength;
	if (changed) {
		this->bufferLength = bufferLength;
		Settings::settings.SetBufferLength(bufferLength);

		hStep = static_cast<float>(blur ? maxDimension : windowWidth) / bufferLength;
		
		lightPack.SetBufferLength(bufferLength);

		UpdateMaxBufferLength();
	}

	renderer->SetBufferLength(bufferLength, changed);

	if (platform->IsListening() && changed)
		platform->Listen();
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

	Settings::settings.SetFftSize(fftLength * 2);

	UpdateMaxBufferLength();
}

void CApp::SetRotating(bool rotating) { 
	this->rotating = rotating;
	Settings::settings.SetRotating(rotating);
}
void CApp::SetRotationSpeed(float speed) { 
	this->rotationSpeed = speed;
	Settings::settings.SetRotationSpeed(speed);
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
	platform->SetSafeArea(window, *context, *w, *h);
	safeAreaPadding = context->GetSafeArea().y;
	scale = (virtualW == 0 ? 1.0f : static_cast<float>(*w) / virtualW);

#ifndef __linux__
	scale *= SDL_GetWindowDisplayScale(window);
#else
	// mini-player requires unscaled
	// values to update window shape
	if (miniPlayer) {
		*w /= scale;
		*h /= scale;
	}
#endif

	if (scale != originalScale)
		scaleDelta = scale - originalScale;

	originalScale = scale;

	return platform->GetScale(scale);
}

inline void CApp::CacheBlurUniforms(Context::Shader &shader) {
	shader.program.CacheUniformLocation("timeDelta");
	shader.program.CacheUniformLocation("intensity");
	shader.program.CacheUniformLocation("screenSize");
	shader.program.CacheUniformLocation("randomX");
	shader.program.CacheUniformLocation("randomY");
	shader.program.CacheUniformLocation("effectIntensity");
	shader.program.CacheUniformLocation("effectXOffset");
	shader.program.CacheUniformLocation("effectYOffset");
	shader.program.CacheUniformLocation("effectRadiation");
	shader.program.CacheUniformLocation("effectTimeDelta");
	shader.program.CacheUniformLocation("effectHorizontalSpread");
	shader.program.CacheUniformLocation("effectVerticalSpread");
	shader.program.CacheUniformLocation("effectRotation");
	shader.program.CacheUniformLocation("effectEnabled");
	shader.program.CacheUniformLocation("bgr");
	shader.program.CacheUniformLocation("premultipliedAlpha");
}

inline void CApp::SetEffect(const std::string &effect) {
	const Context::Shader *blurShader = nullptr;
	if (effect != Settings::settings.GetEffect()) {
		Settings::settings.SetEffect(effect);

		context->RemoveShader("blur"_hash);

		auto newShader = context->AddShader(
			Utils::GetResource("vertex-blur.glsl"),
			std::vector<std::filesystem::path> {
			Utils::GetResource("fragment-blur.glsl"),
				Utils::GetResource(std::string("Effects/fragment-") + Settings::settings.GetEffect() + ".glsl")
			},
			"blur"_hash
		);

		newShader->program.Use();
		newShader->program.CacheUniformLocation("projection");

		CacheBlurUniforms(*newShader);

		blurShader = const_cast<const Context::Shader *>(newShader);
	} else {
		blurShader = context->GetShader("blur"_hash);
		blurShader->program.Use();
	}

	blurShader->program.Uniform1f("intensity"_hash, blurIntensity);
	blurShader->program.Uniform2f("screenSize"_hash, maxDimension, maxDimension);
	blurShader->program.Uniform1f("effectIntensity"_hash, Settings::settings.GetEffectIntensity());
	blurShader->program.Uniform1f("effectXOffset"_hash, Settings::settings.GetEffectXOffset());
	blurShader->program.Uniform1f("effectYOffset"_hash, Settings::settings.GetEffectYOffset());
	blurShader->program.Uniform1f("effectRadiation"_hash, Settings::settings.GetEffectRadiation());
	blurShader->program.Uniform1f("effectHorizontalSpread"_hash, Settings::settings.GetEffectHorizontalSpread());
	blurShader->program.Uniform1f("effectVerticalSpread"_hash, Settings::settings.GetEffectVerticalSpread());
	blurShader->program.Uniform1f("effectRotation"_hash, Settings::settings.GetEffectRotation());
	blurShader->program.Uniform1f("effectEnabled"_hash, (playing || platform->IsListening()) ? 1.0f : 0.0f);

	context->Use("texture"_hash);
}

void CApp::UpdateBeatCounter() {
	if (!randomizePresetsBeats) return;

	const auto elapsedBeats = beatDetect->GetNumberOfElapsedBeats();
	beatCounter = elapsedBeats % *randomizePresetsBeats;

	LogDebug("Number of elapsed beats: ", elapsedBeats);
	LogDebug("\t% randomizePresetsBeats: ", beatCounter);
}

void CApp::LoadRenderer(const std::string &rendererName) {
	renderer = RendererFactory::Build(
		rendererName,
		&dynamicGain,
		&albumArt,
		renderer,
		windowWidth,
		windowHeight,
		buffer,
		platform->GetMaxLength(),
		bufferLength
	);

	Settings::settings.SetRenderer(rendererName);
}

void CApp::SetVisualizerScale(float scale) {
	Settings::settings.SetScale(scale);

	if (renderer)
		renderer->SetScale(scale);
}

inline void CApp::SetHdr(bool enabled) {
	int width = 0, height = 0;

	if (HDR::Enabled != enabled) {
		SDL_GetWindowSize(sdlWindow, &width, &height);

		if (blur) {
			blurFbo = std::make_unique<MultisampledFramebufferObject>(maxDimension, maxDimension, platform->GetFboInternalFormat(enabled));
			lastFrame = std::make_unique<MultisampledFramebufferObject>(maxDimension, maxDimension, platform->GetFboInternalFormat(enabled));
		}

		uiFbo = std::make_unique<MultisampledFramebufferObject>(windowWidth, windowHeight, platform->GetFboInternalFormat(enabled), platform->IsUiInverted());

		updateUi += 1;
	}

	if (HDR::Enabled && !enabled)
		platform->SetHdr(enabled, nullptr, width, height);
	else if (!HDR::Enabled && enabled)
		platform->SetHdr(enabled, nullptr, width, height);
}

void CApp::LoadShaders() {
	context->AddShader(
		Utils::GetResource("vertex-texture.glsl"),
		Utils::GetResource("fragment-texture.glsl"),
		"texture"_hash
	);
	context->AddShader(
		Utils::GetResource("vertex.glsl"),
		Utils::GetResource("fragment.glsl"),
		"basic"_hash
	);
	context->AddShader(
		Utils::GetResource("vertex-color.glsl"),
		Utils::GetResource("fragment-color.glsl"),
		"color"_hash
	);
	context->AddShader(
		Utils::GetResource("vertex-rotate.glsl"),
		Utils::GetResource("fragment-rotate.glsl"),
		"rotate"_hash
	);
	context->AddShader(
		Utils::GetResource("vertex-font.glsl"),
		Utils::GetResource("fragment-font.glsl"),
		"font"_hash
	);
	context->AddShader(
		Utils::GetResource("vertex-blur.glsl"),
		std::vector<std::filesystem::path>{
			Utils::GetResource("fragment-blur.glsl"),
			Utils::GetResource(std::string("Effects/fragment-") + Settings::settings.GetEffect() + ".glsl")
		},
		"blur"_hash
	);
	context->AddShader(
		Utils::GetResource("vertex-blit.glsl"),
		Utils::GetResource("fragment-blit.glsl"),
		"blit"_hash
	);
	context->AddShader(
		Utils::GetResource("vertex-texture.glsl"),
		Utils::GetResource("fragment-scrolling.glsl"),
		"scrolling"_hash
	);
	context->AddShader(
		Utils::GetResource("vertex.glsl"),
		Utils::GetResource("fragment-ring.glsl"),
		"ring"_hash
	);

	// Cache our uniforms
	for (auto &[hash, shader] : *context) {
		// by calling program.Use() instead of context->Use()
		// we avoid changing the currentProgram
		shader.program.Use();
		shader.program.CacheUniformLocation("projection");

		if (hash != "blur"_hash) {
			shader.program.CacheUniformLocation("color");
			shader.program.Uniform4f("color"_hash, 1.0f, 1.0f, 1.0f, 1.0f);
		} else {
			CacheBlurUniforms(shader);

			shader.program.Uniform1f("effectIntensity"_hash, Settings::settings.GetEffectIntensity());
			shader.program.Uniform1f("effectXOffset"_hash, Settings::settings.GetEffectXOffset());
			shader.program.Uniform1f("effectYOffset"_hash, Settings::settings.GetEffectYOffset());
			shader.program.Uniform1f("effectRadiation"_hash, Settings::settings.GetEffectRadiation());
			shader.program.Uniform1f("effectHorizontalSpread"_hash, Settings::settings.GetEffectHorizontalSpread());
			shader.program.Uniform1f("effectVerticalSpread"_hash, Settings::settings.GetEffectVerticalSpread());
			shader.program.Uniform1f("effectRotation"_hash, Settings::settings.GetEffectRotation());
			shader.program.Uniform1f("effectEnabled"_hash, (playing || platform->IsListening()) ? 1.0f : 0.0f);
			shader.program.Uniform1i("bgr"_hash, 0);
			shader.program.Uniform1i("premultipliedAlpha"_hash, platform->IsAlphaPremultiplied());
		}

		if (hash == "rotate"_hash) {
			shader.program.CacheUniformLocation("screenSize");
			shader.program.CacheUniformLocation("radius");
			shader.program.CacheUniformLocation("multiplier");
			shader.program.Uniform1f("multiplier"_hash, HDR::WhiteLevel * HDR::Headroom);
			shader.program.CacheUniformLocation("normalized");
			shader.program.Uniform1i("normalized"_hash, 0);

			shader.program.CacheUniformLocation("bgr");
			shader.program.Uniform1i("bgr"_hash, 0);
		}

		if (hash == "blit"_hash) {
			shader.program.CacheUniformLocation("screenSize");
			shader.program.CacheUniformLocation("yOffset");
			shader.program.Uniform1f("yOffset"_hash, 0.0f);

			shader.program.CacheUniformLocation("bgr");
			shader.program.Uniform1i("bgr"_hash, 0);

			shader.program.CacheUniformLocation("vignette");
			shader.program.Uniform1i("vignette"_hash, 0);

			shader.program.CacheUniformLocation("windowSize");
		}

		if (hash == "texture"_hash) {
			shader.program.CacheUniformLocation("hdr");
			shader.program.Uniform1i("hdr"_hash, 0);
			shader.program.CacheUniformLocation("expand");
			shader.program.Uniform1f("expand"_hash, 0);
			shader.program.CacheUniformLocation("multiplier");
			shader.program.Uniform1f("multiplier"_hash, HDR::WhiteLevel * HDR::Headroom);
			shader.program.CacheUniformLocation("contrast");
			shader.program.Uniform1f("contrast"_hash, Settings::settings.GetAlbumArtContrast());
			shader.program.CacheUniformLocation("brightness");
			shader.program.Uniform1f("brightness"_hash, Settings::settings.GetAlbumArtBrightness());

			shader.program.CacheUniformLocation("text");
			shader.program.Uniform1i("text"_hash, 0);

			shader.program.CacheUniformLocation("cube");
			shader.program.Uniform1i("cube"_hash, 1);

			shader.program.CacheUniformLocation("gamma");
			shader.program.Uniform1f("gamma"_hash, Settings::settings.GetAlbumArtGamma());

			shader.program.CacheUniformLocation("bgr");
			shader.program.Uniform1i("bgr"_hash, 0);

			shader.program.CacheUniformLocation("origin");
			shader.program.Uniform2f("origin"_hash, 0, 0);
		}

		if (hash == "basic"_hash) {
			shader.program.CacheUniformLocation("bgr");
			shader.program.Uniform1i("bgr"_hash, 0);
		}

		if (hash == "scrolling"_hash) {
			shader.program.CacheUniformLocation("maxWidth");
			shader.program.Uniform1f("maxWidth"_hash, windowWidth);

			shader.program.CacheUniformLocation("screenSize");
			shader.program.Uniform2f("screenSize"_hash, 0, 0);

			shader.program.CacheUniformLocation("origin");
			shader.program.Uniform2f("origin"_hash, 0, 0);

			shader.program.CacheUniformLocation("bleedEdge");
			shader.program.Uniform1i("bleedEdge"_hash, ScrollingText::BleedEdge);

			shader.program.CacheUniformLocation("bgr");
			shader.program.Uniform1i("bgr"_hash, 0);
		}

		if (hash == "ring"_hash) {
			shader.program.CacheUniformLocation("radius");
			shader.program.Uniform1f("radius"_hash, albumArt.GetRadius(miniPlayer));
			shader.program.CacheUniformLocation("screenSize");
			shader.program.Uniform2f("screenSize"_hash, 0, 0);
		}
	}
}

void CApp::UpdateHdrProperties(bool force) {
	platform->UpdateHdrProperties(force);
	albumArt.LoadCube();

	pulseMaxBrightness = HDR::Enabled && Settings::settings.GetPulseMaxBrightness();

	if (context) {
		context->With("blur"_hash, [this](Context::Shader &shader) {
			shader.program.Uniform1i("premultipliedAlpha"_hash, platform->IsAlphaPremultiplied());
		});
		context->With("rotate"_hash, [](Context::Shader &shader) {
			shader.program.Uniform1f("multiplier"_hash, HDR::Enabled ? HDR::WhiteLevel * HDR::Headroom : 1.0f);
		});
		context->With("texture"_hash, [](Context::Shader &shader) {
			shader.program.Uniform1f("multiplier"_hash, HDR::Enabled ? HDR::WhiteLevel * HDR::Headroom : 1.0f);
		});
	}

	if (HDR::Enabled) {
		if (!Settings::settings.GetHdrWhitePoint())
			Settings::settings.SetHdrWhitePoint(HDR::WhiteLevel);
		else
			HDR::SetWhiteLevel(*Settings::settings.GetHdrWhitePoint());
	}

	UpdateVsync();

	// Update our visualizer color
	// to reflect any changes to white point
	// and headroom
	if (!albumArt.Loaded() || overrideColor)
		OnColorChanged(visColor, true);
}

void CApp::UpdateVsync() {
	if (auto interop = platform->GetInterop())
		interop->SetVsync(Settings::settings.GetVsync());

	// Prefer adaptive sync over regular vsync
	if (Settings::settings.GetVsync() && !SDL_GL_SetSwapInterval(-1))
		SDL_GL_SetSwapInterval(1);

	// If HDR is enabled, we're
	// using an interop
	if (!Settings::settings.GetVsync() || HDR::Enabled)
		SDL_GL_SetSwapInterval(0);
}

void CApp::SetVulkan(bool vulkan) {
	if (this->vulkan == vulkan) return;

	LogDebug("Toggling Vulkan interop ", vulkan ? "ON" : "OFF");

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL3_Shutdown();

	platform->DestroyInterop();

	// Keep the old window around to
	// swap out its OpenGL context
	auto oldWindow = sdlWindow;
	sdlWindow = nullptr;

	if (openGlWindow) {
		SDL_DestroyWindow(oldWindow);
		oldWindow = openGlWindow;
		openGlWindow = nullptr;
	}

	this->vulkan = vulkan;
	Settings::settings.SetVulkan(vulkan);

	platform->CreateInterop();

	auto props = CreateSdlWindow();
	if (vulkan)
		platform->OpenOpenGlWindow(props);
	SDL_DestroyProperties(props);

	SDL_GL_MakeCurrent(vulkan ? openGlWindow : sdlWindow, openGlContext);

	// Once the OpenGL context has been
	// swapped, destroy the old window.
	platform->DestroyWindow(oldWindow);

	platform->OnInit(GetInteropArgs(), *context);
	platform->HookWindow(miniPlayer);

	ImGui_ImplSDL3_InitForOpenGL(sdlWindow, openGlContext);
	ImGui_ImplOpenGL3_Init();

	if (miniPlayer)
		platform->SetMiniPlayer(true, static_cast<uint8_t>(albumArt.GetChromaColor() * 0xFF));

	// Force a resize to update projection matrix
	OnResize(windowWidth, windowHeight, scale, true);
}

inline SDL_PropertiesID CApp::CreateSdlWindow() {
	SDL_PropertiesID props = SDL_CreateProperties();

	windowX = Settings::settings.GetMiniPlayerX();
	windowY = Settings::settings.GetMiniPlayerY();

	SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "popRocks");
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, miniPlayer ? Settings::settings.GetMiniPlayerWidth() : Settings::settings.GetWindowWidth());
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, miniPlayer ? Settings::settings.GetMiniPlayerHeight() : Settings::settings.GetWindowHeight());
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, miniPlayer ? windowX : Settings::settings.GetWindowX());
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, miniPlayer ? windowY : Settings::settings.GetWindowY());
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_VULKAN_BOOLEAN, platform->GetVulkanProperty());
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, platform->GetOpenGlProperty());
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN, miniPlayer);

#ifdef __linux__
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WAYLAND_SURFACE_ROLE_CUSTOM_BOOLEAN, miniPlayer);
#endif

	// FIXME: Transparent SDL windows don't play well with Windows' layered windows
	//        The window will always appear at ~50% opacity with this combination,
	//        even for colors well outside of the chosen chroma key.
	//
	//			HDR doesn't play well with this, either.
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_TRANSPARENT_BOOLEAN, miniPlayer);

	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_ALWAYS_ON_TOP_BOOLEAN, miniPlayer);

	sdlWindow = SDL_CreateWindowWithProperties(
		props
	);

	return props;
}

Interop::InitArgs CApp::GetInteropArgs() {
	Interop::InitArgs args;

	args.adapterIndex = platform->GetAdapterIndex();
	args.width = windowWidth;
	args.height = windowHeight;
	args.surfaceCallback = [&](void *instance) {
		VkSurfaceKHR surface;

		if (!SDL_Vulkan_CreateSurface(sdlWindow, reinterpret_cast<VkInstance>(instance), nullptr, &surface)) {
			LogError("Could not create Vulkan surface: ", SDL_GetError());

			platform->ShowDialogBox("Could not create Vulkan surface!", SDL_GetError());
		}

		return reinterpret_cast<void *>(surface);
	};

	return args;
}

void CApp::UpdateMiniPlayer() {
	if (miniPlayer) {
		UpdateBleedEdge(albumArt.GetRadius(miniPlayer));
		if (const auto pos = platform->SetWindowPos(
			Settings::settings.GetMiniPlayerX(),
			Settings::settings.GetMiniPlayerY(),
			Settings::settings.GetMiniPlayerWidth(),
			Settings::settings.GetMiniPlayerHeight()
		)) {
			// If SetWindowPos changed our position,
			// make sure to update it.
			Settings::settings.SetMiniPlayerX(pos->x);
			Settings::settings.SetMiniPlayerY(pos->y);

			windowX = pos->x;
			windowY = pos->y;
		}
		platform->UpdateWindowShape();
	}

	UpdateVsync();
}

void CApp::SetMiniPlayer(bool miniPlayer, bool inLoop) {
	LogDebug("Setting miniPlayer to ", miniPlayer? "ON" : "OFF");
	//if (miniPlayer) SDL_HideWindow(sdlWindow);

	this->miniPlayer = miniPlayer;
	Settings::settings.SetMiniPlayer(miniPlayer);

	// Update our font size first, as everything
	// downstream depends on it
	controls.SetMiniPlayer(*context, miniPlayer);
	controls.UpdateFontSize(std::nullopt, true);

	SetRadius(albumArt.GetRadius(miniPlayer));

	if (sdlWindow) {
#if 1
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplSDL3_Shutdown();

		if (auto interop = platform->GetInterop())
			interop->OnDestroy();

		// Keep the old window around to
		// swap out its OpenGL context
		auto oldWindow = sdlWindow;

		// We don't need the properties here,
		// so destroy them right away
		SDL_DestroyProperties(CreateSdlWindow());

		if (!vulkan)
			SDL_GL_MakeCurrent(sdlWindow, openGlContext);

		// Now that we've swapped the OpenGL
		// context, destroy the old window
		platform->DestroyWindow(oldWindow);

		// Since the platform hasn't yet been given
		// the new miniPlayer value, we need to
		// explicitly pass it here.
		platform->HookWindow(miniPlayer);

		ImGui_ImplSDL3_InitForOpenGL(sdlWindow, openGlContext);
		ImGui_ImplOpenGL3_Init();
#else
		SDL_SetWindowAlwaysOnTop(sdlWindow, miniPlayer);
		platform->SetWindowPos(
			miniPlayer ? Settings::settings.GetMiniPlayerX() : Settings::settings.GetWindowX(),
			miniPlayer ? Settings::settings.GetMiniPlayerY() : Settings::settings.GetWindowY(),
			miniPlayer ? Settings::settings.GetMiniPlayerWidth() : Settings::settings.GetWindowWidth(),
			miniPlayer ? Settings::settings.GetMiniPlayerHeight() : Settings::settings.GetWindowHeight()
		);
		SDL_SetWindowBordered(sdlWindow, !miniPlayer);
#endif

#if 1
		if (auto interop = platform->GetInterop()) {
			interop->OnInit(GetInteropArgs());

			// Menu callbacks take place
			// between OnLoop() and SwapBuffers(),
			// so we have to return to that state
			if (inLoop) interop->OnLoop();
		}
#endif

		backgroundAlpha = miniPlayer ? 0.0f : 1.0f;

		// Either recalculate or reset our
		// chroma value before sending it to
		// the platform
//		if (miniPlayer) albumArt.CalculateChroma();
//		else albumArt.ResetChroma();

		platform->SetMiniPlayer(miniPlayer, static_cast<uint8_t>(albumArt.GetChromaColor() * 0xFF));

		context->With("blur"_hash, [this](Context::Shader &shader) {
			const auto premultipliedAlpha = platform->IsAlphaPremultiplied();
			LogDebug("Turning premultiplied alpha ", premultipliedAlpha ? "ON" : "OFF");
			shader.program.Uniform1i("premultipliedAlpha"_hash, premultipliedAlpha);
		});
	}
	UpdateMiniPlayer();

	// We need to resize the control icons
	controls.OnResize(
		miniPlayer ? Settings::settings.GetMiniPlayerWidth() : Settings::settings.GetWindowWidth(),
		miniPlayer ? Settings::settings.GetMiniPlayerHeight() : Settings::settings.GetWindowHeight(),
		*context,
		scale,
		platform->GetDefaultFramebuffer(),
		miniPlayer
	);

	if (auto index = miniPlayer ? Settings::settings.GetMiniPlayerPresetIndex() : Settings::settings.GetPresetIndex())
		LoadPreset(Preset::GetPresets().at(*index));

	// Update scale to reflect radius change
	if (albumArt.Loaded()) albumArt.Scale(true);

	// Make sure our cursor is visible
	if (miniPlayer)
		SDL_ShowCursor();
}

inline void CApp::UpdateBleedEdge(float radius) {
	const auto ratio = (radius / AlbumArt::BaseRadius);
	ScrollingText::SetBleedEdgeRatio(ratio);
	const auto bleedEdge = ScrollingText::BleedEdge * ratio;
	LogInfo("Setting bleed edge to ", bleedEdge, " pixels");

	// Update our scrolling text's bleed edges relative to the radius
	context->With("scrolling"_hash, [bleedEdge](Context::Shader &shader) {
		shader.program.Uniform1i("bleedEdge"_hash, bleedEdge);
	});
}

inline void CApp::SetRadius(float radius) {
	albumArt.SetRadius(radius, miniPlayer);

	if (!albumArt.IsResizing())
		albumArt.Scale(true);

	controls.OnRadiusChanged(*context, miniPlayer);

	UpdateBleedEdge(radius);
}

void CApp::ResetWindow() {
	if (miniPlayer) {
		// Use temporary scaled values to ensure any
		// DPI changes when moving back to the primary
		// monitor are handled correctly.
		Settings::settings.SetMiniPlayerWidth(1080 * scale, true);
		Settings::settings.SetMiniPlayerHeight(1080 * scale, true);

		Settings::settings.SetMiniPlayerX(SDL_WINDOWPOS_CENTERED, true);
		Settings::settings.SetMiniPlayerY(SDL_WINDOWPOS_CENTERED, true);
		Settings::settings.SetMiniPlayerVisualizerRatio(5.4f, true);
		Settings::settings.SetMiniPlayerRadius(AlbumArt::BaseRadius);

		UpdateMiniPlayer();

		// Prepare for potential DPI change
		Settings::settings.SetMiniPlayerWidth(1080, true);
		Settings::settings.SetMiniPlayerHeight(1080);

		SetRadius(Settings::settings.GetMiniPlayerRadius() * scale);
	}
	else {
		Settings::settings.SetWindowWidth(1920, true);
		Settings::settings.SetWindowHeight(1080, true);
		Settings::settings.SetWindowX(SDL_WINDOWPOS_CENTERED, true);
		Settings::settings.SetWindowY(SDL_WINDOWPOS_CENTERED);

		SDL_SetWindowSize(sdlWindow, Settings::settings.GetWindowWidth(), Settings::settings.GetWindowHeight());
		SDL_SetWindowPosition(sdlWindow, Settings::settings.GetWindowX(), Settings::settings.GetWindowY());
	}
}

void CApp::OnInit() {
#ifdef __linux__
	std::ifstream boardVendor("/sys/devices/virtual/dmi/id/board_vendor");
	std::string vendor;

	std::ifstream boardName("/sys/devices/virtual/dmi/id/board_name");
	std::string name;

	if (boardVendor && boardName) {
		boardVendor >> vendor;
		boardName >> name;

		if (vendor == "Valve" && (name == "Jupiter" /* Steam Deck LCD*/ || name == "Galileo" /* Steam Deck OLED */))
			steamDeck = true;
	}
#endif

	// https://tgui.eu/tutorials/latest-stable/dpi-scaling/
	//SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
	SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

	auto ret = SDL_Init(
		SDL_INIT_VIDEO |
		SDL_INIT_EVENTS
	);

	LogDebug("SDL_Init() returned ", ret ? "true" : "false");
	if (!ret)
		LogDebug("SDL_GetError = ", SDL_GetError());

	/*
	ret = IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_WEBP);

	std::stringstream stream;
	stream << "IMG_Init loaded ";
	if (ret & IMG_INIT_JPG) stream << "JPG ";
	if (ret & IMG_INIT_PNG) stream << "PNG ";
	if (ret & IMG_INIT_WEBP) stream << "WEBP";
	LogDebug(stream.str());
	*/

	platform->SetGlAttributes();

	//SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);

	// For some reason we need to explicitly
	// request an 8-bit alpha channel on certain
	// OpenGL implementations (namely VBoxSVGA's)
	//SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	//SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	//SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	//SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 64); // For RGBA16F
	//SDL_GL_SetAttribute(SDL_GL_FLOATBUFFERS, 1);
	//SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);

	windowWidth = Settings::settings.GetWindowWidth();
	windowHeight = Settings::settings.GetWindowHeight();

	auto props = CreateSdlWindow();
	platform->OpenOpenGlWindow(props);
	SDL_DestroyProperties(props);

	platform->SetMiniPlayer(miniPlayer, static_cast<uint8_t>(albumArt.GetChromaColor() * 0xFF));

	if (platform->CreateOpenGlContext()) {
		LogDebug("gladLoadGL() returned ", platform->LoadGlad());

		context = std::make_unique<Context>();

		LoadShaders();

		platform->OnInit(GetInteropArgs(), *context);

		// Get our initial bounding box
		UpdateDisplayBoundingBox();

		UpdateHdrProperties();

#if GUI
		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO &io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;	// Enable Keyboard Controls

		// Setup Platform/Renderer backends
		ImGui_ImplSDL3_InitForOpenGL(
			sdlWindow,
			openGlContext
		);
		ImGui_ImplOpenGL3_Init();

		menu->SetOnOpen([this](const std::filesystem::path &path) {
			LoadFile(path);
			ImGui::SetWindowFocus(nullptr);
		});
		menu->SetOnBufferSizeChanged([this](int bufferSize) {
			SetBufferLength(bufferSize);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnDecayTimeChanged([this](float decayTime) {
			SetDecayTime(
				std::chrono::duration<double> {
					decayTime
				}
			);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnFadeTimeChanged([this](float fadeTime) {
			SetFadeTime(
				std::chrono::duration<double> {
					fadeTime
				}
			);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnPulseChanged([this](bool pulse) {
			renderer->SetPulse(pulse);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnPulseBackgroundChanged([this](bool pulseBackground) {
			Settings::settings.SetPulseBackground(pulseBackground);

			this->pulseBackground = pulseBackground;
			context->With("blur"_hash, [this](Context::Shader &shader) {
				shader.program.Uniform1i("premultipliedAlpha"_hash, platform->IsAlphaPremultiplied());
			});

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnDarkenPulseOnBrightColorsChanged([this](bool darkenPulseOnBrightColors) {
			Settings::settings.SetDarkenPulseOnBrightColors(darkenPulseOnBrightColors);

			this->darkenPulseOnBrightColors = darkenPulseOnBrightColors;
		});
		menu->SetOnPulseTimeChanged([this](float pulseTime) {
			renderer->SetPulseTime(
				std::chrono::duration<double> {
					pulseTime
				}
			);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnStrobeChanged([this](bool strobe) {
			SetStrobe(strobe);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnStrobeIntensityChanged([this](float strobeIntensity) {
			Settings::settings.SetStrobeIntensity(strobeIntensity);

			this->strobeIntensity = strobeIntensity;

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnBlurChanged([this](bool blur) {
			ToggleBlur();
			ImGui::SetWindowFocus(nullptr);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnSourceFactorChanged([this](GLenum sourceFactor) {
			Settings::settings.SetSourceFactor(sourceFactor);

			this->sourceFactor = sourceFactor;

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnDestFactorChanged([this](GLenum destFactor) {
			Settings::settings.SetDestFactor(destFactor);
			
			this->destFactor = destFactor;

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnSourceAlphaFactorChanged([this](GLenum sourceAlphaFactor) {
			Settings::settings.SetSourceAlphaFactor(sourceAlphaFactor);

			this->sourceAlphaFactor = sourceAlphaFactor;

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnDestAlphaFactorChanged([this](GLenum destAlphaFactor) {
			Settings::settings.SetDestAlphaFactor(destAlphaFactor);

			this->destAlphaFactor = destAlphaFactor;

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnBlurIntensityChanged([this](float blurIntensity) {
			SetBlurIntensity(blurIntensity);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnBlurOpacityChanged([this](float blurOpacity) {
			Settings::settings.SetBlurOpacity(blurOpacity);

			this->blurOpacity = blurOpacity;

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnRotatingChanged([this](bool rotate) {
			SetRotating(rotate);
			ImGui::SetWindowFocus(nullptr);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnRpmChanged([this](float rpm) {
			SetRotationSpeed(rpm * (360.0f / 60.0f) /* 6 */);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnDetectBpmChanged([this](bool detectBpm) {
			for (auto &detector : beatDetectors)
				detector.SetDetecting(detectBpm);

			Settings::settings.SetDetectBpm(beatDetect->IsDetecting());

			if (beatDetect->IsDetecting() && !loadedFile.empty()) {
				for (auto &detector : beatDetectors)
					detector.Cancel();

				auto stream = platform->OpenWithFlags(loadedFile, loadedFileExtension, BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT);

				// Disassociate the stream from a device,
				// so it doesn't get freed on BASS_Free()
				BASS_ChannelSetDevice(stream, BASS_NODEVICE);

				LoadBeats(
					stream,
					loadedFile,
					false // don't ping-pong when we toggle
				);
			}
		});
		menu->SetOnVisualizationTypeChanged([this](const std::string &visualizationType) {
			LoadRenderer(visualizationType);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnLineRendererStyleChanged([this](int lineRendererStyle) {
			const auto style = static_cast<LineRenderer::Style>(lineRendererStyle);

			if (auto lineRenderer = dynamic_cast<LineRenderer *>(renderer)) {
				lineRenderer->SetStyle(style);
			}

			Settings::settings.SetLineRendererStyle(style);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnLightPackVisualizationTypeChanged([this](const std::string &lightPackVisualizationType) {
			lightPack.SetLightType(
				lightPackVisualizationType == "intensity" ?
					LightPack::LightType::Intensity :
					lightPackVisualizationType == "color" ?
						LightPack::LightType::Color :
						LightPack::LightType::ColorIntensity
			);
		});
		menu->SetOnLightPackMappingChanged([this](const std::string &lightPackMapping) {
			lightPack.SetMapping(
				lightPackMapping == "default" ?
					Mappings::DEFAULT :
					lightPackMapping == "mine" ?
						Mappings::MINE :
						lightPackMapping == "ttb" ?
							Mappings::TOP_TO_BOTTOM :
							Mappings::BOTTOM_TO_TOP
			);
		});
		menu->SetOnLightPackFocusAreaChanged([this](const std::string &lightPackFocusArea) {
			lightPack.SetFocusArea(
				lightPackFocusArea == "superbass" ?
					LightPack::FocusArea::SuperBass :
					lightPackFocusArea == "subbass" ?
						LightPack::FocusArea::SubBass :
						lightPackFocusArea == "bass" ?
							LightPack::FocusArea::Bass :
							lightPackFocusArea == "bassandmid" ?
								LightPack::FocusArea::BassAndMid :
								lightPackFocusArea == "bassmidandalittlehighend" ?
									LightPack::FocusArea::BassMidAndHigh :
									lightPackFocusArea == "halfnyquist" ?
										LightPack::FocusArea::HalfNyquist :
										LightPack::FocusArea::Nyquist
			);
		});
		menu->SetOnRadiusChanged([this](int radius) {
			SetRadius(radius);
		});
		menu->SetOnLineWidthChanged([this](float lineWidth) {
			if (auto lineRenderer = dynamic_cast<LineRenderer *>(renderer))
				lineRenderer->SetWidth(lineWidth);
		});
		menu->SetOnSmoothChanged([this](int smooth) {
			lightPack.SetSmooth(smooth);
		});
		menu->SetOnGammaChanged([this](float gamma) {
			// Silent so it doesn't spam the console
			// while the user is dragging
			lightPack.SetGamma(gamma, true);
		});
		menu->SetOnPresetChanged([this](std::optional<std::size_t> preset) {
			LoadPreset(preset);
		});
		menu->SetOnPlaylistOnScreenChanged([this](bool playlistOnScreen) {
			controls.GetPlaylist().SetVisible(playlistOnScreen);
		});
		menu->SetOnPlaylistFadeChanged([this](bool playlistFade) {
			controls.GetPlaylist().SetFade(playlistFade);
		});
		menu->SetOnCurrentSongVisibleChanged([this](bool currentSongVisible) {
			controls.GetPlaylist().SetCurrentSongVisible(currentSongVisible);
		});
		menu->SetOnColorSelectionChanged([this](const Settings::ColorSelection &selection) {
			Settings::settings.SetColorSelection(selection);
			albumArt.ReprocessColors();
		});
		menu->SetOnFftSizeChanged([this](int fftSize) {
			SetFftLength(fftSize);
		});
		menu->SetOnListeningChanged([this](bool listening) {
			if (listening)
				platform->Listen();
			else
				platform->StopListening();

			Settings::settings.SetListening(listening);
		});
		menu->SetOnLoopbackChanged([this](bool loopback) {
			if (loopback)
				platform->Listen(true);
			else
				platform->StopListening();

			Settings::settings.SetLoopback(loopback);
		});
		menu->SetOnOutputDeviceChanged([this](const std::string &outputDevice) {
			Settings::settings.SetOutputDevice(outputDevice);

			if (platform->IsListening() && Settings::settings.GetLoopback())
				platform->Listen(true);
			else {
				auto pos = streamHandle ? BASS_ChannelBytes2Seconds(
					streamHandle,
					BASS_ChannelGetPosition(streamHandle, BASS_POS_BYTE)
				) : 0.0;

				if (!platform->LoadExclusive(pos)) {
					// Free the old device
					BASS_Free();

					// Initialize the new device
					BASS_Init(platform->GetDeviceIndex<true>(outputDevice), freq, 0, 0, nullptr);

					Open(loadedFile, loadedFileExtension, false, streamHandle, visualStreamHandle, true);

					SeekTo(pos);
					TogglePlaying();
				}
			}
		});
		menu->SetOnInputDeviceChanged([this](const std::string &inputDevice) {
			Settings::settings.SetInputDevice(inputDevice);

			if (platform->IsListening() && !Settings::settings.GetLoopback())
				platform->Listen();
		});
		menu->SetOnEffectChanged([this](const std::string &effect) {
			SetEffect(effect);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnEffectIntensityChanged([this](float effectIntensity) {
			Settings::settings.SetEffectIntensity(effectIntensity);

			this->context->With("blur"_hash, [effectIntensity](Context::Shader &shader) {
				shader.program.Uniform1f("effectIntensity"_hash, effectIntensity);
			});

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnEffectXOffsetChanged([this](float effectXOffset) {
			Settings::settings.SetEffectXOffset(effectXOffset);

			this->context->With("blur"_hash, [effectXOffset](Context::Shader &shader) {
				shader.program.Uniform1f("effectXOffset"_hash, effectXOffset);
			});

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnEffectYOffsetChanged([this](float effectYOffset) {
			Settings::settings.SetEffectYOffset(effectYOffset);

			this->context->With("blur"_hash, [effectYOffset](Context::Shader &shader) {
				shader.program.Uniform1f("effectYOffset"_hash, effectYOffset);
			});

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		// TODO: Add setting to change radiation function (circle, centered horizontal line (iTunes-style), etc.)
		menu->SetOnEffectRadiationChanged([this](float effectRadiation) {
			Settings::settings.SetEffectRadiation(effectRadiation);

			this->context->With("blur"_hash, [effectRadiation](Context::Shader &shader) {
				shader.program.Uniform1f("effectRadiation"_hash, effectRadiation);
			});

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnEffectHorizontalSpreadChanged([this](float effectHorizontalSpread) {
			Settings::settings.SetEffectHorizontalSpread(effectHorizontalSpread);

			this->context->With("blur"_hash, [effectHorizontalSpread](Context::Shader &shader) {
				shader.program.Uniform1f("effectHorizontalSpread"_hash, effectHorizontalSpread);
			});

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnEffectVerticalSpreadChanged([this](float effectVerticalSpread) {
			Settings::settings.SetEffectVerticalSpread(effectVerticalSpread);

			this->context->With("blur"_hash, [effectVerticalSpread](Context::Shader &shader) {
				shader.program.Uniform1f("effectVerticalSpread"_hash, effectVerticalSpread);
			});

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnEffectRotationChanged([this](float effectRotation) {
			Settings::settings.SetEffectRotation(effectRotation);

			this->context->With("blur"_hash, [effectRotation](Context::Shader &shader) {
				shader.program.Uniform1f("effectRotation"_hash, effectRotation);
			});

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnLimitFramerateChanged([this](bool limitFramerate) {
			Settings::settings.SetLimitFramerate(limitFramerate);

			if (limitFramerate)
				frameLimit = Settings::settings.GetFrameLimit();
			else
				frameLimit = -1;
		});
		menu->SetOnFrameLimitChanged([this](int frameLimit) {
			Settings::settings.SetFrameLimit(frameLimit);

			this->frameLimit = frameLimit;
		});
		menu->SetOnRandomizeChanged([this](bool randomize) {
			Settings::settings.SetRandomize(randomize);

			if (randomize)
				randomizeTime = Settings::settings.GetRandomizeTime();
			else
				randomizeTime = std::nullopt;
		});
		menu->SetOnRandomizeTimeChanged([this](float randomizeTime) {
			this->randomizeTime = Duration<Microseconds>(
				std::chrono::duration<double>(
					static_cast<double>(randomizeTime)
				)
			);

			Settings::settings.SetRandomizeTime(
				*this->randomizeTime
			);
		});
		menu->SetOnScaleChanged([this](float scale) {
			SetVisualizerScale(scale);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnSelectedPresetsChanged([this](const std::set<std::size_t> &selectedPresets) {
			Settings::settings.SetSelectedPresets(selectedPresets);
		});
		menu->SetOnRandomizePresetsChanged([this](bool randomizePresets) {
			Settings::settings.SetRandomizePresets(randomizePresets);

			if (randomizePresets)
				randomizePresetsTime = Settings::settings.GetRandomizePresetsTime();
			else
				randomizePresetsTime = std::nullopt;
		});
		menu->SetOnRandomizePresetsTimeChanged([this](float randomizePresetsTime) {
			this->randomizePresetsTime = Duration<Microseconds>(
				std::chrono::duration<double>(
					static_cast<double>(randomizePresetsTime)
				)
			);

			Settings::settings.SetRandomizePresetsTime(
				*this->randomizePresetsTime
			);
		});
		menu->SetOnRandomizePresetsByBeatChanged([this](bool randomizePresetsByBeats) {
			Settings::settings.SetRandomizePresetsByBeats(randomizePresetsByBeats);

			if (randomizePresetsByBeats) {
				randomizePresetsBeats = Settings::settings.GetRandomizePresetsBeats();
				UpdateBeatCounter();
			} else randomizePresetsBeats = std::nullopt;
		});
		menu->SetOnRandomizePresetsBeatsChanged([this](int randomizePresetsBeats) {
			Settings::settings.SetRandomizePresetsBeats(randomizePresetsBeats);

			this->randomizePresetsBeats = randomizePresetsBeats;

			UpdateBeatCounter();
		});
		menu->SetOnResetRotation([this] {
			frameCount = 0;
			SetRotating(false);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnClearBlurFbo([this] {
			ClearBlurFbo();
		});
		menu->SetOnResetWindow([this] {
			ResetWindow();
		});
		menu->SetOnQuit([this] {
			SDL_Event event;
			event.type = SDL_EVENT_QUIT;
			SDL_PushEvent(&event);
		});
		menu->SetOnRandom([this] {
			LoadPreset(Preset::Random(dynamicGain));
		});
		menu->SetOnAutoFadeChanged([this](bool autoFade) {
			Settings::settings.SetAutoFade(autoFade);
		});
		menu->SetOnWaitTimeChanged([this](float waitTime) {
			auto duration = Duration<Microseconds>(
				std::chrono::duration<double>(waitTime)
			);

			Settings::settings.SetWaitTime(duration);
			controls.SetWaitTime(duration);
		});
		menu->SetOnAutoFadeSpeedChanged([this](float autoFadeSpeed) {
			Settings::settings.SetAutoFadeSpeed(autoFadeSpeed);
			controls.SetAutoFadeSpeed(autoFadeSpeed);
		});
		menu->SetOnExclusiveChanged([this](bool exclusive) {
			ToggleExclusive();
		});
		menu->SetOnExclusiveVolumeChanged([this](int volume) {
			controls.GetVolume().SetVolume(volume);
		});
		menu->SetOnHalveBpmChanged([this](bool halveBpm) {
			Settings::settings.SetHalveBpm(halveBpm);
		});
		menu->SetOnRendererOffsetChanged([this](int rendererOffset) {
			Settings::settings.SetRendererOffset(rendererOffset);

			renderer->SetOffset(rendererOffset);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu->SetOnLutChanged([this](const std::string &lut) {
			Settings::settings.SetLut(lut);

			albumArt.GetCube()->Load(Utils::GetResource(std::filesystem::path("LUTs") / lut));
		});
		menu->SetOnAlbumArtGammaChanged([this](float gamma) {
			Settings::settings.SetAlbumArtGamma(gamma);

			this->context->With("texture"_hash, [this, gamma](Context::Shader &shader) {
				shader.program.Uniform1f("gamma"_hash, gamma);
			});
		});
		menu->SetOnAlbumArtContrastChanged([this](float contrast) {
			Settings::settings.SetAlbumArtContrast(contrast);

			this->context->With("texture"_hash, [this, contrast](Context::Shader &shader) {
				shader.program.Uniform1f("contrast"_hash, contrast);
			});
		});
		menu->SetOnAlbumArtBrightnessChanged([this](float brightness) {
			Settings::settings.SetAlbumArtBrightness(brightness);

			this->context->With("texture"_hash, [this, brightness](Context::Shader &shader) {
				shader.program.Uniform1f("brightness"_hash, brightness);
			});
		});
		menu->SetOnHdrWhitePointChanged([this](std::optional<float> hdrWhitePoint) {
			Settings::settings.SetHdrWhitePoint(hdrWhitePoint);

			if (hdrWhitePoint)
				HDR::SetWhiteLevel(*hdrWhitePoint);
		});
		menu->SetOnRescanAlbumArt([this] {
			if (loadedFile.empty()) return;

			LogDebug("Re-scanning for album art...");

			auto originalPath =
				controls.GetPlaylist().GetPath().empty() ?
					std::filesystem::is_directory(loadedFile) ?
						loadedFile :
						""
					: controls.GetPlaylist().GetPath();

			metadata.OnLoad(
				loadedFile,
				loadedFileExtension,
				this->streamHandle,
				&controls,
				&albumArt
			);

			albumArt.Load(
				platform->GetNativePath(loadedFile),
				originalPath
			);

			albumArt.Scale();
		});
		menu->SetOnPulseUiChanged([this](bool pulseUi) {
			Settings::settings.SetPulseUi(pulseUi);

			if (!pulseUi) {
				albumArt.RemoveColorChangeListener(menu.get());
				menu->OnColorChanged(visColor);
			} else {
				albumArt.AddColorChangeListener(menu.get());
				menu->OnColorChanged(GetColor());
			}
		});
		menu->SetOnUiGammaChanged([this](float uiGamma) {
			Settings::settings.SetUiGamma(uiGamma);

			this->uiGamma = uiGamma;
		});
		menu->SetOnUiContrastChanged([this](float uiContrast) {
			Settings::settings.SetUiContrast(uiContrast);

			this->uiContrast = uiContrast;
		});
		menu->SetOnUiBrightnessChanged([this](float uiBrightness) {
			Settings::settings.SetUiBrightness(uiBrightness);

			this->uiBrightness = uiBrightness;
		});
		menu->SetOnPulseMaxBrightnessChanged([this](bool pulseMaxBrigtness) {
			Settings::settings.SetPulseMaxBrightness(pulseMaxBrigtness);

			this->pulseMaxBrightness = HDR::Enabled && pulseMaxBrigtness;
		});
		menu->SetOnVsyncChanged([this](bool vsync) {
			Settings::settings.SetVsync(vsync);
			
			UpdateVsync();
		});
		menu->SetOnPlaylistItemChanged([this](std::size_t index) {
			if (auto track = controls.GetPlaylist().TrackAtIndex(index)) {
				const auto next = controls.GetPlaylist().GetNext();

				// If we didn't select the _next_ song in the playlist,
				// we need to reset beat detection.
				if (!next || track->path != next->path || next->title != track->title)
					ResetBeatDetection();

				LoadFile(track->path, true);

				if (track->startTime > DBL_EPSILON)
					SeekTo(track->startTime);
			}
		});
		menu->SetOnRngSourceChanged([this](const std::string &rngSource) {
			Settings::settings.SetRngSource(rngSource);

			prng = PRNGFactory<unsigned int>::Build(rngSource, &streamHandle);
		});
		menu->SetOnMiniPlayerChanged([this](bool miniPlayer) {
			SetMiniPlayer(miniPlayer, true);
		});

		platform->AddMenuCallbacks(menu.get());

		menu->OnColorChanged(visColor);
#endif
	} else {
		LogError("Could not create OpenGL context: ", platform->GetOpenGlContextError());

		platform->ShowDialogBox("Could not create OpenGL context!", platform->GetOpenGlContextError());
	}

	LogDebug("OpenGL Version: ", reinterpret_cast<const char*>(glGetString(GL_VERSION)));

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Initialize our buffers now that we have an OpenGL context
	SetBufferLength(Settings::settings.GetBufferLength());

	// We only sample halfway to the
	// Nyquist at start. This gives
	// us 4096 samples but our buffer
	// length is only 2048. The highest
	// bins are largely empty, anyway.
	SetFftLength(Settings::settings.GetFftSize());

	// This updates the scale variable for us
	GetScale(sdlWindow, &windowWidth, &windowHeight);

	platform->LoadBassPlugins();

	if (BASS_Init(platform->GetDeviceIndex<true>(Settings::settings.GetOutputDevice()), freq, 0, 0, nullptr) != TRUE) {
		LogError("Could not initialize audio device!");

		platform->ShowDialogBox("Could not initialize audio device!", "Error: " + GetBassError(BASS_ErrorGetCode()));
	}

	lightPack.OnInit();

	renderer->OnInit(windowWidth, windowHeight);

	// controls.OnInit() calls albumArt.OnInit()
	controls.OnInit(windowWidth, windowHeight, *context, scale, platform->GetDefaultFramebuffer(), miniPlayer);
	controls.SetFadeCallback([this](bool in) {
		if (!miniPlayer) {
			if (in) SDL_ShowCursor();
			else SDL_HideCursor();
		}
	});

	albumArt.SetOnLoaded([this] (bool embedded) {
		for (auto integration : integrations) {
			auto file = albumArt.GetCurrentFile();
			if (embedded && albumArt.HasEmbedded()) {
				if (auto tempFile = platform->GetTemporaryFile("/tmp/popRocks_embedded_art.XXXXXX"); !tempFile.empty()) {
					const auto [bytes, size] = albumArt.GetEmbedded();

					std::ofstream outFile(tempFile, std::ios::out | std::ios::binary);
					outFile.write(reinterpret_cast<const char *>(bytes), size);

					file = tempFile;
				}
			}

			integration->OnSongChanged(
				beatDetect->GetHash(),
				controls.GetTitle(),
				controls.GetArtist(),
				controls.GetAlbum(),
				file,
				std::chrono::duration_cast<std::chrono::microseconds>(
					std::chrono::duration<double>(
						controls.GetCurrentSongLength()
					)
				).count()
			);
		}
	});

	close.OnInit(controls.GetIconSize());

#if GUI
	albumArt.AddColorChangeListener(menu.get());
#endif
	albumArt.AddColorChangeListener(this);
	albumArt.AddColorChangeListener(&close);
	albumArt.AddBlackChangedListener(this);

	spindle.OnInit(albumArt.GetRadius(miniPlayer) / SpindleSize);

	UpdateMiniPlayer();

	OnResize(windowWidth, windowHeight, scale, true);

	if (Settings::settings.GetListening())
		platform->Listen();
	else if (Settings::settings.GetLoopback())
		platform->Listen(true);
}

 CApp::~CApp() {
	 delete renderer;
	 delete[] buffer;
}

HSTREAM CApp::GetStreamHandle() const {
	return streamHandle;
}

HSTREAM CApp::GetNextStreamHandle() const {
	return nextStreamHandle;
}

void CApp::OnResize(int width, int height, float scale, bool force) {
	if (width == windowWidth && height == windowHeight && !scaleDelta && !force) return;

	windowWidth = width;
	windowHeight = height;

	//LogDebug("Resizing to ", width, " x ", height);

	if (!miniPlayer) {
		Settings::settings.SetWindowWidth(
			width
#ifdef __linux__
			/ scale
#endif
		);
		Settings::settings.SetWindowHeight(
			height
#ifdef __linux__
			/ scale
#endif
		);
	} else if (scaleDelta) {
		platform->HandleScaleDelta(
			scale,
			scaleDelta,
			windowWidth,
			windowHeight,
			lastMousePos,
			windowX,
			windowY
		);
	}

	platform->OnResize(windowWidth, windowHeight);

	glViewport(0, 0, windowWidth, windowHeight);

	albumArt.OnResize(windowWidth, windowHeight, scale);

	controls.OnResize(windowWidth, windowHeight, *context, scale,
		platform->GetDefaultFramebuffer(),
		miniPlayer
	);

	close.OnResize(controls.GetIconSize());

	loadingIndicator.OnInit(
		3.0f * albumArt.GetRadius(miniPlayer) / AlbumArt::BaseRadius,
		miniPlayer ? controls.GetIconSize() / 1.5f : albumArt.GetRadius(miniPlayer) * Controls::MiniPlayerIconRatio,
		controls.GetFont(),
		controls.GetOutlineFont(),
		*context,
		"Loading"
	);

	beatLoadingIndicator.OnInit(
		3.0f * albumArt.GetRadius(miniPlayer) / AlbumArt::BaseRadius,
		miniPlayer ? controls.GetIconSize() / 1.5f : albumArt.GetRadius(miniPlayer) * Controls::MiniPlayerIconRatio,
		controls.GetFont(),
		controls.GetOutlineFont(),
		*context,
		"Loading Beats"
	);

	if (blur) {
		maxDimension = std::sqrt(std::pow(windowWidth, 2) + std::pow(windowHeight, 2));
		//maxDimension = windowHeight;
		hStep = static_cast<float>(maxDimension) / bufferLength;

		blurOffset = {
			windowWidth - maxDimension,
			windowHeight - maxDimension
		};

		blurFbo = std::make_unique<MultisampledFramebufferObject>(maxDimension, maxDimension, platform->GetFboInternalFormat(HDR::Enabled), platform->IsUiInverted());
		lastFrame = std::make_unique<MultisampledFramebufferObject>(maxDimension, maxDimension, platform->GetFboInternalFormat(HDR::Enabled), platform->IsUiInverted());

		blurFbo->SetDefaultFramebuffer(platform->GetDefaultFramebuffer());
		lastFrame->SetDefaultFramebuffer(platform->GetDefaultFramebuffer());

		context->With("blur"_hash, [this](Context::Shader &shader) {
			shader.program.Uniform1f("intensity"_hash, blurIntensity);
			shader.program.Uniform2f("screenSize"_hash, maxDimension, maxDimension);
		});
		context->With("rotate"_hash, [this](Context::Shader &shader) {
			shader.program.Uniform2f("screenSize"_hash, maxDimension, maxDimension);
		});
		context->With("blit"_hash, [this](Context::Shader &shader) {
			shader.program.Uniform2f("screenSize"_hash, maxDimension, maxDimension);
			shader.program.Uniform2f("windowSize"_hash, windowWidth, windowHeight);
		});
	} else {
		maxDimension = windowWidth;
		hStep = static_cast<float>(windowWidth) / bufferLength;
		context->With("rotate"_hash, [this](Context::Shader &shader) {
			shader.program.Uniform2f("screenSize"_hash, windowWidth, windowHeight);
		});
		context->With("blit"_hash, [this](Context::Shader &shader) {
			shader.program.Uniform2f("screenSize"_hash, windowWidth, windowHeight);
			shader.program.Uniform2f("windowSize"_hash, windowWidth, windowHeight);
		});
	}

	renderer->OnResize(windowWidth, windowHeight, maxDimension);
	if (!playing && !platform->IsListening())
		updateRenderer = true;

#if GUI
	uiFbo = std::make_unique<MultisampledFramebufferObject>(windowWidth, windowHeight, platform->GetFboInternalFormat(HDR::Enabled), platform->IsUiInverted());

	uiFbo->SetDefaultFramebuffer(platform->GetDefaultFramebuffer());

	// We want the menu to be a bit easier to touch
	// on the Steam Deck, so we enlarge it
	menu->OnResize(
		width,
		height,
		steamDeck ? 1.33f * originalScale : originalScale
		, safeAreaPadding,
		steamDeck || platform->IsTouchScreen()
	);

	// Needs 3 frames:
	// 1 to layout the menu
	// 1 to measure the menu
	// 1 extra to ensure we lose focus
	updateUi += 3;
#endif
}

const Colour<float> &CApp::GetColor() const {
	if (albumArt.Loaded() && !overrideColor)
		return albumArt.GetColor();
	else
		return visColor;
}

void CApp::SetColor(float alpha) const {
	const auto &color = GetColor();

	context->Color(color.r, color.g, color.b, alpha);
}

void CApp::AdvanceToNextTrack() {
	advanceOnNextLoop = true;
}

void CApp::LoadRandomPreset() {
	/*
	const auto &selectedPresets = Settings::settings.GetSelectedPresets();
	auto begin = selectedPresets.begin();

	do {
		begin = selectedPresets.begin();
		std::advance(begin, (prng() % selectedPresets.size()));
	} while (presetIndex && *begin == *presetIndex);

	LoadPreset(*begin);
	*/

	// Shuffle presets like they're tetrominoes
	if (shuffledPresets.empty()) {
		// Copy because we're modifying it
		auto selectedPresets = Settings::settings.GetSelectedPresets();
		while (!selectedPresets.empty()) {
			std::set<std::size_t>::iterator preset;
			do {
				preset = selectedPresets.begin();
				std::advance(preset, (prng->Next() % selectedPresets.size()));
			} while (shuffledPresets.empty() && presetIndex && *preset == *presetIndex);

			shuffledPresets.emplace_back(*preset);
			selectedPresets.erase(preset);
		}
	}
	
	LoadPreset(*shuffledPresets.begin());
	shuffledPresets.erase(shuffledPresets.begin());
}

inline void CApp::DrawCloseButton(const Delta &time) {
	if (miniPlayer) {
		const auto closeSize = controls.GetIconSize() / Close::GetLowestRatio();
		const float one = 1.0f;

		close.OnLoop(
			windowWidth / 2 + albumArt.GetRadius(miniPlayer) - closeSize,
			windowHeight / 2 - albumArt.GetRadius(miniPlayer) + closeSize,
			time,
			*context,
			(!fileLoaded && !platform->IsListening()) ? &one : &controls.GetAlpha()
		);
	}
}

inline bool CApp::IsOnCloseButton(const Vector2i &mousePos) {
	const auto radius = albumArt.GetRadius(miniPlayer);
	const auto closeSize = controls.GetIconSize() / Close::GetLowestRatio();

	return
		mousePos.x >= windowWidth / 2 + radius - closeSize * 2 && mousePos.x <= windowWidth / 2 + radius &&
		mousePos.y >= windowHeight / 2 - radius && mousePos.y <= windowHeight / 2 - radius + closeSize * 2;
}

void CApp::OnLoop(const Delta &time) {
	if (shuttingDown) {
		if (!destroyed) OnDestroy(false);

		return;
	}

	if (!playlistLoading && playlistLoaded) {
		playlistLoaded = false;

		PlaylistLoaded(loadFilePath, loadFileExtension, loadFileOriginalPath, loadFileFromPlaylist);

		LogDebug("Playlist loaded!");

		// Clear the blur FBO when we load a new file / playlist
		if (blur)
			ClearBlurFbo();
	}

	Logger::ProcessCommands();

	if (auto looped = platform->OnLoop(); looped && !(*looped)) {
		if (blurFbo)
			blurFbo->SetDefaultFramebuffer(platform->GetInterop()->GetFramebuffer());
		if (lastFrame)
			lastFrame->SetDefaultFramebuffer(platform->GetInterop()->GetFramebuffer());
		if (uiFbo)
			uiFbo->SetDefaultFramebuffer(platform->GetInterop()->GetFramebuffer());

		if (auto font = controls.GetFont())
			font->SetDefaultFramebuffer(platform->GetInterop()->GetFramebuffer());
		if (auto boldFont = controls.GetBoldFont())
			boldFont->SetDefaultFramebuffer(platform->GetInterop()->GetFramebuffer());
		if (auto outlineFont = controls.GetOutlineFont())
			outlineFont->SetDefaultFramebuffer(platform->GetInterop()->GetFramebuffer());
		if (auto boldOutlineFont = controls.GetBoldOutlineFont())
			boldOutlineFont->SetDefaultFramebuffer(platform->GetInterop()->GetFramebuffer());
	} else if (!looped) return;

	const auto &chroma = albumArt.GetChromaColor();

	if (miniPlayer) {
		glClearColor(
			chroma,
			chroma,
			chroma,
			backgroundAlpha
		);
	} else glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	glClear(GL_COLOR_BUFFER_BIT);

	context->Use("texture"_hash);

	if (miniPlayer &&
		(mouseCaptureAccum += time.change.AsSeconds()) >= 0.06667 /* Capture mouse at 15FPS maximum */ &&
		!controls.IsScrolling() &&
		platform->IsPointerInWindow() &&
		!platform->IsMoving() &&
		!platform->IsResizing()) {
		float x = 0.0f, y = 0.0f;

#ifdef WIN32
		SDL_GetGlobalMouseState(&x, &y);

		x -= windowX;
		y -= windowY;
#else
		SDL_GetMouseState(&x, &y);
		
		x *= scale;
		y *= scale;
#endif

		// Only show the controls if we're
		// hovered over the album art circle
		// or close button
		if (auto onClose = IsOnCloseButton({ x, y }); 
			Vector2f(x, y).Distance({ windowWidth / 2, windowHeight / 2 }) / scale <= albumArt.GetRadius(miniPlayer) + albumArt.GetOutline().GetWidth() ||
			onClose) {
			platform->SetTransparent(false);

			if (controls.Stick())
				controls.Fade(true);

			close.SetHovered(onClose);
		} else if (!controls.GetHelp().IsHovered()) {
			close.SetHovered(false);

			if (albumArt.OnMouseMoved({ x, y })) {
				platform->SetTransparent(false);
				
				// Need to trigger a SINGLE, fresh event
				if (controls.Unstick())
					controls.Fade(true);
			}
			else if (platform->SetTransparent(true)) {
				if (controls.Unstick()) {

					// We want to fade _in_ so that the
					// user-controlled wait time passes
					// before the eventual fade _out_
					if (controls.GetAlpha() > 0.0f) {
						controls.Fade(true);

						OnMouseLeave();
					}
				}
			}
		}

		mouseCaptureAccum = 0.0;
	} else if (!platform->IsPointerInWindow() && !platform->IsResizing() && platform->SetTransparent(true)) {
		if (controls.Unstick()) {
			// We want to fade _in_ so that the
			// user-controlled wait time passes
			// before the eventual fade _out_
			if (controls.GetAlpha() > 0.0f) {
				controls.Fade(true);
			}
		}

		OnMouseLeave();
	}

	if (!fileLoaded && !platform->IsListening()) {
		// Draw blank album art
		albumArt.OnLoop(
			time,
			windowWidth / 2.0f,
			windowHeight / 2.0f,
			frameCount,
			visColor,
			1.0f,
			*context,
			fileLoaded || platform->IsListening(),
			preLoaded
		);

		DrawCloseButton(time);

		if (playlistLoading && !controls.IsMessageVisible())
			loadingIndicator.OnLoop(time, windowWidth / 2, windowHeight / 2 + (preLoaded ? 0.0f : albumArt.GetRadius(miniPlayer) / 2.0f), *context, 1.0f);

		SwapBuffers(time);

		return;
	}

	if (randomizeTime && std::chrono::system_clock::now() > lastRandomize + std::chrono::duration<double>(randomizeTime->AsSeconds())) {
		LoadPreset(Preset::Random(dynamicGain));
		lastRandomize = std::chrono::system_clock::now();
	} else if (randomizePresetsTime && std::chrono::system_clock::now() > lastPresetRandomize + std::chrono::duration<double>(randomizePresetsTime->AsSeconds())) {
		LoadRandomPreset();
		lastPresetRandomize = std::chrono::system_clock::now();
	}

	if(fileLoaded) {
		int64_t available = 0;

		if (controls.GetExclusiveIndicator().IsExclusive())
			available = platform->GetAvailable();

		auto pos =
			static_cast<int64_t>(BASS_ChannelGetPosition(streamHandle, BASS_POS_BYTE));

		double inSeconds = 0.0;
			
		if (audioOffset || controls.GetExclusiveIndicator().IsExclusive()) {
			pos = pos -
				(audioOffset ? static_cast<int64_t>(BASS_ChannelSeconds2Bytes(streamHandle, std::abs(*audioOffset))) * (*audioOffset < 0 ? -1 : 1) : 0)
				- platform->GetExclusiveBufferSizeInBytes()
				+ (platform->GetExclusiveBufferSizeInBytes() - available);

			if (pos < 0) pos = 0;

			inSeconds = BASS_ChannelBytes2Seconds(streamHandle, pos);

			BASS_ChannelSetPosition(
				visualStreamHandle,
				BASS_ChannelSeconds2Bytes(visualStreamHandle, inSeconds),
				BASS_POS_BYTE
			);

			if (renderer->IsFloatingPoint())
				BASS_ChannelGetData(visualStreamHandle, buffer, fftFlag);
			else
				BASS_ChannelGetData(visualStreamHandle, buffer, static_cast<DWORD>(bufferLength * sizeof(short) * channelInfo.chans));
		} else {
			inSeconds = BASS_ChannelBytes2Seconds(streamHandle, pos);

			if (renderer->IsFloatingPoint())
				BASS_ChannelGetData(streamHandle, buffer, fftFlag);
			else
				BASS_ChannelGetData(streamHandle, buffer, static_cast<DWORD>(bufferLength * sizeof(short) * channelInfo.chans));
		}

#ifdef _DEBUG
		std::stringstream posStream;
		posStream << std::fixed << std::setprecision(3) << std::setfill('0') << inSeconds;

		const auto newPos = posStream.str();

		if (lastPos != newPos && !lastPos.empty() && std::stod(newPos) > std::stod(lastPos))
			++pps;

		if (auto now = std::chrono::system_clock::now(); now - posTimer >= 1s) {
			controls.SetStats("  " /* padding */ + std::to_string(pps) + "PPS"); // POSITIONS per second
			pps = 0;
			posTimer = now;
		}

		lastPos = newPos;
#endif
	} else {
		platform->LoadHeardSamples(renderer, floatBuffer, shortBuffer, bufferLength);
	}

	if (renderer->IsFloatingPoint())
		lightPack.NextSamples(floatBuffer, bufferLength);

	auto color = GetColor();

	currentFadeTime += playing ? time.change.AsSeconds() : 0.0f;
	auto lerp = playing ? std::min(1.0f, currentFadeTime / fadeTime) : 0.0f;

	if (playing && ((renderer->GetPulse() && !renderer->GetPulses()) || strobe)) {
		auto hsv = color.ToHsv();

		if (!darkenPulseOnBrightColors || hsv.v < 0.66 * (HDR::Enabled ? HDR::WhiteLevel * HDR::Headroom : 1.0f)) {
			auto brightHsv = brightColor.ToHsv();
			brightHsv.v = brightHsv.v + ((strobe ? hsv.v - (strobeIntensity * (pulseMaxBrightness ? 1.0f : HDR::WhiteLevel)) : hsv.v) - brightHsv.v) * lerp;
			color = Colour<float>::FromHsv(brightHsv);
		} else {
			auto darkHsv = darkColor.ToHsv();
			darkHsv.v = hsv.v + ((strobe ? darkHsv.v - strobeIntensity : darkHsv.v) - hsv.v) * lerp;
			color = Colour<float>::FromHsv(darkHsv);
		}
	}

	if (playing || platform->IsListening() || updateRenderer || beatDetected) {
		static const Delta zero;

		auto hsv = color.ToHsv();
		auto brightHsv = this->brightColor.ToHsv();
		brightHsv.v = std::max(0.0f, brightHsv.v - strobeIntensity * lerp);

		renderer->OnLoop(
			updateRenderer ? zero : time,
			fileLoaded,
			hStep,
			*context,
			!darkenPulseOnBrightColors || hsv.v < 0.66 * (HDR::Enabled ? HDR::WhiteLevel * HDR::Headroom : 1.0f) ? color : darkColor,
			((strobe && playing) ? Colour<float>::FromHsv(brightHsv) : ((!darkenPulseOnBrightColors || hsv.v < 0.66 * (HDR::Enabled ? HDR::WhiteLevel * HDR::Headroom : 1.0f)) ? this->brightColor : color)),
			frameCount,
			platform->GetMaxHeardSample(),
			resetGain,
			miniPlayer
		);

		if (updateRenderer) updateRenderer = false;
	}

	//}

	if (resetGain) resetGain = false;

	//++frameCount;
	if (rotating && (playing || platform->IsListening())) {
		auto changeInSeconds = static_cast<float>(time.change.AsSeconds());
		frameCount += changeInSeconds * rotationSpeed;
		while (frameCount >= 360.0f)
			frameCount -= 360.0f;
	}

	if (blur) {
		GLint oldViewport[4];
		auto identity = context->GetIdentity();

		blurFbo->Bind();

		glGetIntegerv(GL_VIEWPORT, oldViewport);
		auto projection = glm::ortho(0.0f, static_cast<float>(maxDimension), static_cast<float>(maxDimension), 0.0f);
		context->SetIdentity(std::move(projection));
		glViewport(0, 0, maxDimension, maxDimension);

		if (pulseBackground)
			glClearColor(platform->IsBgr() ? color.b : color.r, color.g, platform->IsBgr() ? color.r : color.b, 1.0f);
		else if (miniPlayer)
			glClearColor(chroma, chroma, chroma, backgroundAlpha);
		else
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

		glClear(GL_COLOR_BUFFER_BIT);

		glBlendFuncSeparate(
			sourceFactor,
			destFactor,
			sourceAlphaFactor,
			destAlphaFactor
		);
		
		context->Use("blur"_hash);
		context->GetShaderProgram().Uniform1f(
			"timeDelta"_hash,
			(playing || platform->IsListening()) ?
				(
					// We currently treat anything >= 3
					// as "never fades out"
					blurIntensity >= 3.0f ?
					0.0f :
					static_cast<float>(time.change.AsSeconds())
				) 
				: 0.0f
		);

		context->GetShaderProgram().Uniform1f(
			"effectTimeDelta"_hash,
			(playing || platform->IsListening()) ?
				// Effects were written with a framerate
				// of 240 in mind, so scale accordingly
				static_cast<float>(time.change.AsSeconds() / (1.0 / 240.0)) :
				0.0f
		);

		const auto nextTwo = prng->NextTwo();

		context->GetShaderProgram().Uniform1f("randomX"_hash, nextTwo.first / static_cast<float>(prng->Max()));
		context->GetShaderProgram().Uniform1f("randomY"_hash, nextTwo.second / static_cast<float>(prng->Max()));
		context->GetShaderProgram().Uniform1f("effectEnabled"_hash, (playing || platform->IsListening()) ? 1.0f : 0.0f);

		lastFrame->DrawMultisampled(0, 0, *context);

		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		context->With("rotate"_hash, [this](Context::Shader &shader) {
			shader.program.Uniform2f("screenSize"_hash, maxDimension, maxDimension);

			if ((Settings::IsColorBlend(sourceFactor) || Settings::IsColorBlend(destFactor)) && HDR::Enabled)
				shader.program.Uniform1i("normalized"_hash, 1);
		});

		renderer->Draw(time, frameCount, color, blurOffset, *context);

		if ((Settings::IsColorBlend(sourceFactor) || Settings::IsColorBlend(destFactor)) && HDR::Enabled)
			context->GetShaderProgram().Uniform1i("expand"_hash, 1);

		glBlendFuncSeparate(
			sourceFactor,
			destFactor,
			sourceAlphaFactor,
			destAlphaFactor
		);

		context->LoadIdentity();
		blurFbo->Unbind();
		
		context->Color(1.0f, 1.0f, 1.0f, 1.0f);

		glClear(GL_COLOR_BUFFER_BIT);

		if (playing)
			blurFbo->Draw(0, 0, *context, lastFrame.get());

		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		context->With("blit"_hash, [this](Context::Shader &shader) {
			shader.program.Uniform1i("vignette"_hash, miniPlayer);
		});

		platform->BlitBlurFbo();

		context->With("blit"_hash, [this](Context::Shader &shader) {
			shader.program.Uniform1i("vignette"_hash, 0);
		});

		context->Color(1.0f, 1.0f, 1.0f, blurOpacity - (strobe ? lerp * (strobeIntensity) : 0.0));
		context->SetIdentity(std::move(identity));
		glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
		context->LoadIdentity();

		blurFbo->DrawWithoutBlitting(blurOffset.x / 2.0f, blurOffset.y / 2.0f, *context);

		context->GetShaderProgram().Uniform1i("expand"_hash, 0);

		context->LoadIdentity();

		context->With("rotate"_hash, [this](Context::Shader &shader) {
			shader.program.Uniform2f("screenSize"_hash, windowWidth, windowHeight);
			shader.program.Uniform1i("normalized"_hash, 0);
		});
	}

	renderer->Draw(time, frameCount, color, {0, 0}, *context);

	if (!shuttingDown)
		lightPack.OnLoop((albumArt.Loaded() && !overrideColor) ? albumArt.GetColor() : visColor);

	// Draw album art OVER the accumulation buffer
	// since we don't want it getting blurry
	albumArt.OnLoop(
		time,
		windowWidth / 2.0f,
		windowHeight / 2.0f,
		frameCount,
		visColor,
		controls.GetAlpha(),
		*context,
		fileLoaded || platform->IsListening(),
		preLoaded,
		!controls.GetHelp().IsHovered()
	);

	// 45 RPM = 270
	// 33 RPM = 198
	// 33.34 RPM = 200.04
	if (rotationSpeed == 270.0f || (rotationSpeed >= 198.0f && rotationSpeed <= 200.05f)) {
		if (spindle.GetRadius() != albumArt.GetRadius(miniPlayer) / SpindleSize)
			spindle.SetRadius(albumArt.GetRadius(miniPlayer) / SpindleSize);

		context->Use("basic"_hash);
		context->Color(0.0f, 0.0f, 0.0f, 1.0f);
		spindle.OnLoop(windowWidth / 2.0f, windowHeight / 2.0f, *context);
		context->Use("texture"_hash);
	}

	// Render the controls over the accumulation buffer, too
	auto elapsed = controls.OnLoop(
		time,
		streamHandle,
		*context,
		miniPlayer ? brightColor : GetColor(),
		playing
	);

	if (!controls.IsMessageVisible()) {
		if (playlistLoading)
			loadingIndicator.OnLoop(time, windowWidth / 2, windowHeight / 2 - albumArt.GetRadius(miniPlayer) / 2.0f - loadingIndicator.GetRadius() / 2.0f, *context, std::max(controls.GetAlpha(), 0.5f));
		else if (beatDetect && beatDetect->GetState() == BeatDetect::State::Loading)
			beatLoadingIndicator.OnLoop(time, windowWidth / 2, windowHeight / 2 - albumArt.GetRadius(miniPlayer) + beatLoadingIndicator.GetRadius() * 2.0f, *context, std::max(controls.GetAlpha(), 0.5f));
	}

	platform->SetStatus(
		playing ? Platform::Status::Playing : Platform::Status::Paused,
		// Windows treats a progress of 0 as if the state was changed
		// to TBPF_NOPROGRESS, so we clamp the bottom end at 1%
		std::max(static_cast<int>((elapsed / controls.GetCurrentSongLength()) * 100), 1)
	);

	for (auto integration : integrations) {
		integration->SetPosition(
			std::chrono::duration_cast<std::chrono::microseconds>(
				std::chrono::duration<double>(
					elapsed
				)
			).count()
		);
	}

	DrawCloseButton(time);
	
	// Line up the next file at >= 90% completion of current file
	if (controls.GetExclusiveIndicator().IsExclusive() && elapsed >= controls.GetCurrentSongLength() * 0.9 && !nextStreamHandle && !controls.GetPlaylist().GetCue()) {
		if (auto next = GetControls().GetPlaylist().GetNext()) {
			std::unique_lock lock(streamHandleMutex);
			auto extension = next->path.extension().u8string();
			std::transform(extension.begin(), extension.end(), extension.begin(), tolower);

			Open(next->path, extension, true, nextStreamHandle, nextVisualStreamHandle);

			LogDebug("Loaded next track: ", next->path);
		}
	}

	// If we reached the end of the song, try loading the next
	// song in the playlist
	if (advanceOnNextLoop) {
		LoadFile(controls.GetPlaylist().Next()->path, true);
		advanceOnNextLoop = false;
	} else if (auto &cue = controls.GetPlaylist().GetCue();
		(!controls.GetExclusiveIndicator().IsExclusive() || cue) && elapsed >= controls.GetCurrentSongLength()) {
		if (auto next = controls.GetPlaylist().Next()) {
			LogDebug("Reached the end of the current song and loading the next");

			LoadFile(next->path, true);
			SeekTo(next->startTime);

			// This is a very specific edge case
			//
			// It would probably be safe to call BASS_ChannelPlay()
			// indiscriminately, but the intent is clearer this way:
			//
			// If we're not in exclusive mode and rolling around
			// back to the beginning of a single file with a cue
			// sheet, we need to restart playback.
			if (next->startTime < DBL_EPSILON &&
				!controls.GetExclusiveIndicator().IsExclusive() &&
				controls.GetPlaylist().GetCue()) {
				BASS_ChannelPlay(streamHandle, TRUE);
				SetPlaying(true);
			}
		} else {
			if (controls.GetExclusiveIndicator().IsExclusive())
				StopExclusive(FALSE);
			else
				Stop(FALSE);

			SetPlaying(false);
		}
	}

	if (stopWasapiOnNextLoop) {
		StopExclusive(FALSE);
		stopWasapiOnNextLoop = false;
	}

	beatDetectTime = elapsed - (controls.GetExclusiveIndicator().IsExclusive() ? platform->GetExclusiveBufferSize() : 0);

	if (audioOffset)
		beatDetectTime += *audioOffset;

	if (beatDetectTime < 0.0)
		beatDetectTime = 0.0;

	if (beatDetect->OnLoop(beatDetectTime)) {
		albumArt.NextBin(true);

		currentFadeTime = 0.0f;
		fadeTime = beatDetect->NextBeatTime() - beatDetectTime;

		if (randomizePresetsBeats) {
			if (++beatCounter == *randomizePresetsBeats || resyncBeats) {
				LoadRandomPreset();
				beatCounter = 0;
				resyncBeats = false;
			}
		}

		beatDetected = true;
	} else if (beatDetected) beatDetected = false;

	SwapBuffers(time);
}

inline void CApp::SwapBuffers(const Delta &time) {
	std::chrono::duration<double, std::nano> over;

	if (frameLimit != -1) {
		if (std::chrono::duration_cast<std::chrono::microseconds>(frameStart.time_since_epoch()).count() == 0)
			frameStart = std::chrono::steady_clock::now();

		while (std::chrono::steady_clock::now() < frameStart + (1s / frameLimit))
			std::this_thread::sleep_for(1ms);

		// How far over the target time are we?
		over = std::chrono::steady_clock::now() - (frameStart + (1s / frameLimit));
	}

	controls.GetFpsCounter().OnFrame();

#if GUI
	if (!miniPlayer) {
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();

		// Keep the controls on screen if a menu is open
		context->Color(HDR::WhiteLevel, HDR::WhiteLevel, HDR::WhiteLevel, 1.0f);
		if (!miniPlayer && menu->OnLoop(lightPack, controls, albumArt, *context, dynamicGain))
			controls.Fade(true);

		if (menu->HasColorChanged() && updateUi == 0)
			updateUi = 1;

		// Keep the UI in an FBO and only update it as needed
		//
		// Why?
		// 
		// The high overhead involved in caching the OpenGL context (+5% CPU usage on a 9950X)
		// as part of ImGui_ImplOpenGL3_RenderDrawData() conflicts with my goal of ~1% CPU usage
		if (updateUi && (uiAccum += time.change.AsSeconds()) >= 0.01667 /* Render UI at 60FPS maximum */) {
			uiFbo->Bind();
			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glClear(GL_COLOR_BUFFER_BIT);
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			uiFbo->Unbind();

			// https://github.com/ocornut/imgui/issues/314#issuecomment-1750082073
			// 
			// Need to call this on the first *two* frames
			if (updateUi > 1)
				ImGui::SetWindowFocus(nullptr);

			--updateUi;
			uiAccum = 0.0;
		} else ImGui::EndFrame();

		if (HDR::Enabled) {
			context->Color(1.0f, 1.0f, 1.0f, (menu->IsPresetPopupVisible() ? 1.0f : 0.98f) * controls.GetAlpha());

			context->GetShaderProgram().Uniform1i("hdr"_hash, true);

			context->GetShaderProgram().Uniform1f("gamma"_hash, uiGamma);
			context->GetShaderProgram().Uniform1f("contrast"_hash, uiContrast);
			context->GetShaderProgram().Uniform1f("brightness"_hash, uiBrightness);

			glActiveTexture(GL_TEXTURE0 + 1);
			albumArt.GetCube()->Bind();
			glActiveTexture(GL_TEXTURE0 + 0);

			// FIXME: this is the only FBO that's rendered upside down
			//
			// The yOffset uniform is a stopgap until a better
			// solution can be found.
			context->With("blit"_hash, [this](Context::Shader &shader) {
				shader.program.Uniform1f("yOffset"_hash, -windowHeight);
			});
		} else context->Color(HDR::WhiteLevel, HDR::WhiteLevel, HDR::WhiteLevel, (menu->IsPresetPopupVisible() ? 1.0f : 0.90f) * controls.GetAlpha());

		if (vulkan) {
			context->With("blit"_hash, [this](Context::Shader &shader) {
				shader.program.Uniform1f("yOffset"_hash, -windowHeight);

				if (platform->IsBgr()) shader.program.Uniform1i("bgr"_hash, 0);
				});
		}

		uiFbo->Draw(0, 0, *context);

		if (HDR::Enabled) {
			albumArt.GetCube()->Unbind();
			context->GetShaderProgram().Uniform1i("hdr"_hash, 0);
			context->GetShaderProgram().Uniform1f("gamma"_hash, Settings::settings.GetAlbumArtGamma());
			context->GetShaderProgram().Uniform1f("contrast"_hash, Settings::settings.GetAlbumArtContrast());
			context->GetShaderProgram().Uniform1f("brightness"_hash, Settings::settings.GetAlbumArtBrightness());

			context->With("blit"_hash, [this](Context::Shader &shader) {
				shader.program.Uniform1f("yOffset"_hash, 0.0f);
			});
		}
#endif
	}

	if (vulkan) {
		context->With("blit"_hash, [this](Context::Shader &shader) {
			shader.program.Uniform1f("yOffset"_hash, 0.0f);

			if (platform->IsBgr()) shader.program.Uniform1i("bgr"_hash, 1);
			});

		// Wait for the new FBO to be generated before
		// swapping to it
		if (!platform->GetInterop()->SwapBuffers()) {
			if (blurFbo)
				blurFbo->SetDefaultFramebuffer(platform->GetInterop()->GetFramebuffer());
			if (lastFrame)
				lastFrame->SetDefaultFramebuffer(platform->GetInterop()->GetFramebuffer());
			if (uiFbo)
				uiFbo->SetDefaultFramebuffer(platform->GetInterop()->GetFramebuffer());

			if (auto font = controls.GetFont())
				font->SetDefaultFramebuffer(platform->GetInterop()->GetFramebuffer());
			if (auto boldFont = controls.GetBoldFont())
				boldFont->SetDefaultFramebuffer(platform->GetInterop()->GetFramebuffer());
			if (auto outlineFont = controls.GetOutlineFont())
				outlineFont->SetDefaultFramebuffer(platform->GetInterop()->GetFramebuffer());
			if (auto boldOutlineFont = controls.GetBoldOutlineFont())
				boldOutlineFont->SetDefaultFramebuffer(platform->GetInterop()->GetFramebuffer());
		}
	} else {
		platform->SwapBuffers();
	}

	frameStart = std::chrono::steady_clock::now() - std::chrono::duration_cast<std::chrono::microseconds>(over);
}

void CApp::SyncToNearestBeat() {
	beatCounter = beatDetect->IsNextBeatCloser(beatDetectTime) ? -1 : 0;

	if (beatCounter == -1)
		resyncBeats = true;

	LogDebug("Setting beatCounter to ", beatCounter);
}

void CApp::OnDestroy(bool includingLog) {
	LogDebug("Shutting down...");

	playlistLoaded = false;
	playlistLoading = false;

	if (playlistThread.joinable())
		playlistThread.join();

	shuttingDown = true;

	platform->OnDestroy();

	for (auto &detector : beatDetectors)
		detector.Cancel();

	lightPack.OnDestroy();
	albumArt.OnDestroy();

	albumArt.RemoveColorChangeListener(this);

	loadingIndicator.OnDestroy();
	beatLoadingIndicator.OnDestroy();
	controls.OnDestroy();

	renderer->OnDestroy();

	spindle.OnDestroy();

	blurFbo.reset();
	lastFrame.reset();

	// Make sure to free our shader resources
	context.reset();

	menu->OnDestroy();

#if GUI
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
#endif

	BASS_Free();
	SDL_GL_DestroyContext(openGlContext);
	SDL_DestroyWindow(sdlWindow);

	if (openGlWindow)
		SDL_DestroyWindow(openGlWindow);

	if (includingLog)
		Logger::OnDestroy();

	destroyed = true;
}

bool CApp::Open(const std::filesystem::path &path, const std::string &extension, bool exclusive, HSTREAM &target, HSTREAM &visualTarget, bool force) {
	if (exclusive)
		exclusive = platform->OpenExclusive(path, extension, exclusive, target, visualTarget, force, channelInfo, reinterpret_cast<void*>(this));

	if (!exclusive) {
		controls.GetExclusiveIndicator().SetExclusive(false);

		BASS_StreamFree(streamHandle);
		BASS_StreamFree(visualStreamHandle);

		target = platform->OpenWithFlags(path, extension, BASS_STREAM_PRESCAN);
		visualTarget = platform->OpenWithFlags(path, extension, BASS_STREAM_PRESCAN | BASS_STREAM_DECODE);

		// Update keyboard hook if we got
		// forced out of exclusive mode
		platform->HookKeyboard();
	}

	// Reset beat counter on each song
	beatCounter = 0;

	return exclusive;
}

void CApp::Stop(BOOL reset) {
	BASS_ChannelStop(streamHandle);

	if (reset == TRUE) {
		BASS_StreamFree(streamHandle);
		streamHandle = 0;
	}

	SetPlaying(false);
}

void CApp::StopExclusive() {
	stopWasapiOnNextLoop = true;
}

void CApp::StopExclusive(BOOL reset) {
	platform->StopExclusive(reset == TRUE);

	if (reset == TRUE) {
		BASS_StreamFree(streamHandle);
		streamHandle = 0;
	}

	SetPlaying(false);
}

void CApp::LoadBeats(
	HSTREAM streamHandle,
	std::filesystem::path path, // not a reference because we pass this to the callback lambda
	bool pingPong
) {
	// When we have a FLAC + .cue, we only
	// want to analyze the current _song_,
	// not the whole file
	const auto &cue = controls.GetPlaylist().GetCue();

	// No matter what, we're done with our _current_ detector
	beatDetect->Reset();

	auto nextDetector = (beatDetect == &beatDetectors[0] ? &beatDetectors[1] : &beatDetectors[0]);

	if (pingPong && nextDetector->GetState() > BeatDetect::State::Idle) {
		auto temp = nextDetector;

		LogDebug("Ping-ponging beat detectors");
		nextDetector = beatDetect;
		beatDetect = temp;
	} else {
		auto fileName = cue ?
			cue->GetCurrentTrack()->title :
			path.stem().u8string();

		beatDetect->OnLoad(
			path,
			Settings::settings.GetCacheDetectionResults(),
			streamHandle,
			channelInfo.freq,
			channelInfo.chans,
			[this, fileName] {
				LogDebug(
					"Beat detection finished for the current song in the playlist (",
					fileName,
					")!"
				);

				// Set beat counter to how many beats we _skipped_
				beatCounter = beatDetect->SeekTo(controls.GetCurrentPosition());

				LogDebug("\tSkipped first ", beatCounter, " beats");

				if (randomizePresetsBeats)
					beatCounter %= *randomizePresetsBeats;

				LogDebug("\tStarting beat counter at ", beatCounter);
			},
			cue ? cue->GetCurrentTrack()->startTime : static_cast<std::optional<double>>(std::nullopt),
			cue ? controls.GetCurrentSongLength() : static_cast<std::optional<double>>(std::nullopt),
			cue ? cue->GetCurrentTrack()->index : static_cast<std::optional<uint8_t>>(std::nullopt)
		);
	}

	if (const auto &next = controls.GetPlaylist().GetNext()) {
		auto nextExtension = next->path.extension().u8string();
		std::transform(nextExtension.begin(), nextExtension.end(), nextExtension.begin(), tolower);
		auto nextHandle = platform->OpenWithFlags(next->path, nextExtension, BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT);

		BASS_CHANNELINFO nextChannelInfo;
		BASS_ChannelGetInfo(nextHandle, &nextChannelInfo);

		// Disassociate the stream from a device,
		// so it doesn't get freed on BASS_Free()
		BASS_ChannelSetDevice(nextHandle, BASS_NODEVICE);

		nextDetector->OnLoad(
			next->path,
			Settings::settings.GetCacheDetectionResults(),
			nextHandle,
			nextChannelInfo.freq,
			nextChannelInfo.chans,
			[this, next, nextDetector] {
				if (nextDetector->IsDetecting()) {
					LogDebug(
						"Beat detection finished for the next song in the playlist (",
						next->title.empty() ? next->path.stem().u8string() : next->title,
						")!"
					);
				}
			},
			next->startTime > DBL_EPSILON ? next->startTime : static_cast<std::optional<double>>(std::nullopt),
			controls.GetNextSongLength(),
			cue ? const_cast<const Cue*>(cue.get())->Next().index : static_cast<std::optional<uint8_t>>(std::nullopt)
		);
	}
}

void CApp::ResetBeatDetection() {
	LogDebug("Resetting beat detectors");
	for (auto &detector : beatDetectors) {
		detector.Reset();
	}
}

inline void CApp::ClearBlurFbo() {
	blurFbo->Bind();

	if (miniPlayer) {
		const auto &chroma = albumArt.GetChromaColor();
		glClearColor(chroma, chroma, chroma, backgroundAlpha);
	} else glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	glClear(GL_COLOR_BUFFER_BIT);

	blurFbo->Unbind();
	lastFrame->Bind();

	glClear(GL_COLOR_BUFFER_BIT);

	lastFrame->Unbind();
}

void CApp::PlaylistLoaded(std::filesystem::path path, std::string extension, std::filesystem::path originalPath, bool fromPlaylist) {
	controls.GetPlaylist().LoadTitles();

	const auto wasPlaying = playing;

	if (fileLoaded && !controls.GetExclusiveIndicator().IsExclusive()) {
		Stop();

		Open(path, extension, controls.GetExclusiveIndicator().IsExclusive(), streamHandle, visualStreamHandle);

		// Don't reset gain if we're changing songs
		// in a playlist.
		if (!fromPlaylist) {
			resetGain = true;

			renderer->Reset();
			resetGain = dynamicGain.reset;
		}
	}

	// We want this as a local variable, as it's handed off to BeatDetect
	auto streamHandle = platform->OpenWithFlags(path, extension, BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT | BASS_STREAM_PRESCAN);

	// Disassociate the stream from a device,
	// so it doesn't get freed on BASS_Free()
	BASS_ChannelSetDevice(streamHandle, BASS_NODEVICE);

	if (streamHandle) {
		BASS_ChannelGetInfo(streamHandle, &channelInfo);

		renderer->SetNumberOfChannels(
			// chans is a DWORD, but I can't imagine
			// many cases where we have > 255 channels
			static_cast<uint8_t>(channelInfo.chans)
		);

		// If our number of channels changed,
		// we need to update the buffer
		UpdateMaxBufferLength();

		controls.OnLoad(streamHandle);

		LoadBeats(streamHandle, path);

		// If we're loading our first file
		// or we didn't auto-advance, explicitly
		// open the file. Auto-advancing handles
		// opening the file in the WASAPI proc.
		if (!fileLoaded || !advanceOnNextLoop) {
			// Clear our next stream handle when
			// tracks are changed by the user
			if (nextStreamHandle) {
				BASS_StreamFree(nextStreamHandle);
				nextStreamHandle = 0;
			}
			if (nextVisualStreamHandle) {
				BASS_StreamFree(nextVisualStreamHandle);
				nextVisualStreamHandle = 0;
			}

			Open(path, extension, controls.GetExclusiveIndicator().IsExclusive(), this->streamHandle, this->visualStreamHandle, !fromPlaylist);
		} else if (advanceOnNextLoop) {
			std::unique_lock lock(streamHandleMutex);

			BASS_StreamFree(this->streamHandle);
			BASS_StreamFree(this->visualStreamHandle);
			this->streamHandle = nextStreamHandle;
			nextStreamHandle = 0;
			this->visualStreamHandle = nextVisualStreamHandle;
			nextVisualStreamHandle = 0;
		}

		// Reset our tags before
		// trying to load new ones
		if (!fromPlaylist)
			controls.ClearTags();

		// Metadata's OnLoad updates the
		// _embedded_ album art, so we need
		// to make sure the hash reset happens
		// _before_ metadata.OnLoad()
		albumArt.UpdateParentPath(originalPath);

		metadata.OnLoad(
			path,
			extension,
			this->streamHandle,
			&controls,
			&albumArt
		);

		if (controls.GetPlaylist().Empty()) {
			controls.GetPlaylist().AddFile(path);
			controls.GetPlaylist().AddItem(controls.GetTitle(), 0, controls.GetTitle());
		}

		// Update the metadata we can before album
		// art load
		for (auto integration : integrations) {
			integration->OnSongChanged(
				beatDetect->GetHash(),
				controls.GetTitle(),
				controls.GetArtist(),
				controls.GetAlbum(),
				"",
				std::chrono::duration_cast<std::chrono::microseconds>(
					std::chrono::duration<double>(
						controls.GetCurrentSongLength()
					)
				).count(),
				false
			);
		}

		// Always look for external art,
		// in case it's higher resolution
		// than the embedded
		albumArt.Load(
			platform->GetNativePath(path),
			originalPath
		);

		// If we have any tags from the cue
		// sheet, load them _after_ everything
		// else
		controls.LoadFromCue();

		if (!fileLoaded || wasPlaying) {
			if (!platform->StartPlayingExclusive(fromPlaylist, fileLoaded, advanceOnNextLoop)) {
				if (platform->PlayAfterLoad())
					BASS_ChannelPlay(this->streamHandle, false);
			}

			if (platform->PlayAfterLoad())
				SetPlaying(true);
			else SetPlaying(false);
		}

		bool lastFileLoaded = fileLoaded;

		fileLoaded = true;

		// Use updated chroma color
		if (albumArt.HasChromaChanged() || lastFileLoaded != fileLoaded) {
			platform->SetMiniPlayer(miniPlayer, static_cast<uint8_t>(albumArt.GetChromaColor() * 0xFF));

			if (blur)
				ClearBlurFbo();
		}

		// If this is our first load and Help
		// has never been dismissed, show it
		if (!lastFileLoaded && !Settings::settings.GetHelpDismissed())
			controls.GetHelp().SetHovered(true, true, true /* bypass loading lag */);

		loadedFile = path;
		loadedFileExtension = extension;
	} else {
		fileLoaded = false;
		albumArt.Reset(visColor);

		std::stringstream stream;
		
		stream << "\"" << path.u8string() << "\" failed to load. Error: " << GetBassError(BASS_ErrorGetCode());
		LogError("Could not open file! ", stream.str());
		platform->ShowDialogBox("Could not open file!", stream.str());
	}

	preLoaded = false;
}

void CApp::LoadFile(std::filesystem::path path, bool fromPlaylist) {
	loadFilePath = path;

	loadFileExtension = path.extension().u8string();
	std::transform(loadFileExtension.begin(), loadFileExtension.end(), loadFileExtension.begin(), tolower);

	loadFileOriginalPath = std::filesystem::is_directory(path) ? path : "";

	loadFileFromPlaylist = fromPlaylist;

	// If we're still in the same file,
	// just try to get updated tags from
	// the cue sheet and run beat detection
	// on the new song
	if (path == loadedFile && controls.GetPlaylist().GetCue()) {
		controls.LoadFromCue();

		for (auto &detector : beatDetectors)
			detector.Cancel();

		auto stream = platform->OpenWithFlags(path, loadFileExtension, BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT);

		// Disassociate the stream from a device,
		// so it doesn't get freed on BASS_Free()
		BASS_ChannelSetDevice(stream, BASS_NODEVICE);

		LoadBeats(
			stream,
			path
		);

		return;
	}

	albumArt.ResetState();

	overrideColor = false;

	// If we try to load an image directly,
	// use that as the album art
	if (AlbumArt::IsSupported(loadFileExtension)) {
		albumArt.Load(path, loadFileOriginalPath, true);
		return;
	}

	if (!loadFileFromPlaylist) {
		{
			std::unique_lock lock(playlistMutex);
			playlistLoaded = false;

			if (playlistLoading) LogWarning("Interrupting playlist load!");

			playlistLoading = false;
		}

		if (playlistThread.joinable())
			playlistThread.join();

		controls.GetPlaylist().Clear();

		playlistThread = std::thread([this] {
			{
				std::unique_lock lock(playlistMutex);
				playlistLoading = true;
			}
			// If we're not in a playlist, _reset_
			// any current beat detectors.
			for (auto &detector : beatDetectors)
				detector.Reset();

			if (auto ret = controls.GetPlaylist().OnLoad(
				loadFilePath,
				loadFileExtension,
				playlistLoading,
				[this](const std::filesystem::path &path, const std::string &extension, DWORD flags) {
					return platform->OpenWithFlags(path, extension, flags);
				}
			)
				) {
				loadFilePath = ret->path;
			} else if (playlistLoading) {
				LogError("Could not load playlist ", loadFilePath);

				platform->ShowDialogBox("Could not load files! ", "\"" + loadFilePath.u8string() + "\" could not be loaded.");

				return;
			}

			loadFileExtension = loadFilePath.extension().u8string();
			std::transform(loadFileExtension.begin(), loadFileExtension.end(), loadFileExtension.begin(), tolower);

			{
				std::unique_lock lock(playlistMutex);
				if (playlistLoading) {
					playlistLoaded = true;
					playlistLoading = false;
				}
			}
		});
	} else {
		// If we're in a playlist, we want the
		// folder that was originally scanned
		// for songs
		loadFileOriginalPath = controls.GetPlaylist().GetPath();

		// If we're in a playlist, cancel any
		// current beat detectors
		for (auto &detector : beatDetectors)
			detector.Cancel();

		PlaylistLoaded(loadFilePath, loadFileExtension, loadFileOriginalPath, loadFileFromPlaylist);
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

	OnColorChanged(visColor);
	menu->OnColorChanged(visColor);
}

void CApp::SetDecayTime(Duration<Microseconds> time) {
	if (auto fftRenderer = dynamic_cast<FFTRenderer *>(renderer))
		fftRenderer->SetDecayTime(time);

	Settings::settings.SetDecayTime(time);

	SetBufferLength(bufferLength);
}
void CApp::SetFadeTime(Duration<Microseconds> time) {
	if (auto fftRenderer = dynamic_cast<FFTRenderer *>(renderer))
		fftRenderer->SetFadeTime(time);

	Settings::settings.SetFadeTime(time);

	SetBufferLength(bufferLength);
}

void CApp::SetStrobe(bool strobe) {
	this->strobe = strobe;
	Settings::settings.SetStrobe(strobe);
}

void CApp::SetBlur(bool blur) {
	// If blur isn't actually changing,
	// ignore it.
	if (this->blur == blur) return;

	this->blur = blur;

	LogDebug("Turning blur ", blur ? "on" : "off");
	Settings::settings.SetBlur(blur);
	if (blur) {
		maxDimension = std::sqrt(std::pow(windowWidth, 2) + std::pow(windowHeight, 2));
		hStep = static_cast<float>(maxDimension) / bufferLength;

		blurOffset = {
			windowWidth - maxDimension,
			windowHeight - maxDimension
		};

		blurFbo = std::make_unique<MultisampledFramebufferObject>(maxDimension, maxDimension, HDR::Enabled ? GL_RGBA16F : GL_RGBA);
		lastFrame = std::make_unique<MultisampledFramebufferObject>(maxDimension, maxDimension, HDR::Enabled ? GL_RGBA16F : GL_RGBA);

		blurFbo->SetDefaultFramebuffer(platform->GetDefaultFramebuffer());
		lastFrame->SetDefaultFramebuffer(platform->GetDefaultFramebuffer());

		context->With("blur"_hash, [this](Context::Shader &shader) {
			shader.program.Uniform1f("intensity"_hash, blurIntensity);
		});
		context->With("rotate"_hash, [this](Context::Shader &shader) {
			shader.program.Uniform2f("screenSize"_hash, maxDimension, maxDimension);
		});
	} else {
		blurFbo.reset();
		lastFrame.reset();

		blurOffset = { 0, 0 };

		hStep = static_cast<float>(windowWidth) / bufferLength;
		maxDimension = windowWidth;

		context->With("rotate"_hash, [this](Context::Shader &shader) {
			shader.program.Uniform2f("screenSize"_hash, windowWidth, windowHeight);
		});
	}

	renderer->OnResize(windowWidth, windowHeight, maxDimension);
}

void CApp::ToggleBlur() {
	SetBlur(!blur);
}

void CApp::SetBlurIntensity(float intensity) {
	blurIntensity = intensity;
	Settings::settings.SetBlurIntensity(intensity);

	context->With("blur"_hash, [this](Context::Shader &shader) {
		shader.program.Uniform1f("intensity"_hash, blurIntensity);
	});
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

	LogDebug(
		"Seeking by ",
		seconds,
		" seconds (",
		absolute,
		")"
	);

	if (BASS_ChannelSetPosition(streamHandle, pos + bytes, BASS_POS_BYTE) == FALSE) {
		const auto error = GetBassError(BASS_ErrorGetCode());
		LogError("Seek failed! Error: ", error);

		platform->ShowDialogBox("Seek failed!", "Error: " + error);
	} else {
		// Refresh our times
		controls.SetElapsedSeconds(-1);

		beatDetect->SeekTo(absolute);

		UpdateBeatCounter();
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
		const auto error = GetBassError(BASS_ErrorGetCode());
		LogError("Seek failed! Error: ", error);

		platform->ShowDialogBox("Seek failed!", "Error: " + error);
	} else {
		// Refresh our times
		controls.SetElapsedSeconds(-1);

		if (auto &cue = controls.GetPlaylist().GetCue())
			beatDetect->SeekTo(seconds - cue->GetCurrentTrack()->startTime);
		else
			beatDetect->SeekTo(seconds);

		UpdateBeatCounter();
	}
}

void CApp::FadeControls(bool in) {
	controls.Fade(in);
}

void CApp::SetPlaying(bool playing) { 
	this->playing = playing;

	for (auto integration : integrations) {
		if (playing) {
			integration->OnPlay();
			platform->SetStatus(Platform::Status::Playing, controls.GetCurrentPosition() / controls.GetCurrentSongLength());
		} else {
			integration->OnPause();
			platform->SetStatus(Platform::Status::Paused, controls.GetCurrentPosition() / controls.GetCurrentSongLength());
		}
	}
}

void CApp::TogglePlaying() {
	if (streamHandle) {
		// If we hit the end of the song, we might be in an error state
		if (auto pos = BASS_ChannelGetPosition(streamHandle, BASS_POS_BYTE);
			pos == -1 || BASS_ChannelBytes2Seconds(streamHandle, pos) >= controls.GetCurrentFileLength())
			SeekTo(0.0);

		if (!platform->StopPlayingExclusive()) {
			if (BASS_ChannelIsActive(streamHandle) != BASS_ACTIVE_PLAYING) {
				BASS_ChannelPlay(streamHandle, FALSE);
				SetPlaying(true);
			} else {
				BASS_ChannelPause(streamHandle);
				SetPlaying(false);
			}
		}
	}
}

void CApp::ToggleFullscreen() {
	platform->ToggleFullscreen();
}

void CApp::NextTrack() {
	if (auto next = controls.GetPlaylist().Next()) {
		LoadFile(next->path, true);

		if (next->startTime > DBL_EPSILON)
			SeekTo(next->startTime);
	}
}

void CApp::PreviousTrack() {
	// Go to the beginning of the current track
	// if we're further than 5 seconds in
	if (auto current = controls.GetPlaylist().Current();
		current && controls.GetCurrentPosition() >= 5.0) {
		SeekTo(current->startTime);	
	} else if (auto prev = controls.GetPlaylist().Previous()) {
		// Since we have the _next_ track preloaded
		// for beat detection, we need to reset
		ResetBeatDetection();

		LoadFile(prev->path, true);

		SeekTo(prev->startTime);
	} else SeekTo(0.0);
}

inline bool CApp::SeekToMousePos(const Vector2i &mousePos, bool ignoreY) {
	if (ignoreY ||
			(mousePos.y >= (context->GetSafeArea().h + context->GetSafeArea().y) - Controls::SeekbarSize * scale &&
			 mousePos.y <= (context->GetSafeArea().h + context->GetSafeArea().y))) {
		double time = (static_cast<double>(mousePos.x) / windowWidth) * controls.GetCurrentSongLength();

		if (auto &cue = controls.GetPlaylist().GetCue())
			time += cue->GetCurrentTrack()->startTime;

		SeekTo(time);

		return true;
	}

	return false;
}

void CApp::ToggleExclusive() {
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

	if (!exclusive)
		controls.GetVolume().SetRadius(albumArt.GetRadius(miniPlayer) / scale);

	Open(loadedFile, loadedFileExtension, controls.GetExclusiveIndicator().IsExclusive(), streamHandle, visualStreamHandle);

	// Restore our last position
	BASS_ChannelSetPosition(
		streamHandle,
		BASS_ChannelSeconds2Bytes(
			streamHandle,
			elapsed
		),
		BASS_POS_BYTE
	);

	if (!platform->StartExclusive()) {
		BASS_Start();
		BASS_ChannelPlay(streamHandle, false);

		SetPlaying(true);
	}
}

bool CApp::OnMouseClicked(const Vector2i &mousePos) {
	lastMousePos = std::nullopt;

	if (auto preset = controls.GetPresetList().OnMouseClicked(mousePos)) {
		LoadPreset(preset);
		return false;
	}

	// Playlist::OnMouseClicked automatically advances
	// our playlist to the clicked position, so we have
	// to figure out what our next track is _before_
	// that happens.
	const auto next = controls.GetPlaylist().GetNext();

	auto file = 
		controls.GetPlaylist().OnMouseClicked(mousePos);

	if (file) {
		// If we didn't select the _next_ song in the playlist,
		// we need to reset beat detection.
		if (!next || file->path != next->path || next->title != file->title)
			ResetBeatDetection();

		LoadFile(file->path, true);

		SeekTo(file->startTime);
	} else if (auto button = 
		controls.OnMouseClicked(
			mousePos,
			[this](float pos) { 
				if (fileLoaded && !miniPlayer) {
					auto time = pos * controls.GetCurrentSongLength();

					if (auto &cue = controls.GetPlaylist().GetCue())
						time += cue->GetCurrentTrack()->startTime;

					SeekTo(time);
				}
			},
			playing
		);
		button != Controls::ControlButton::None
	) {
		if (button == Controls::ControlButton::Previous)
			PreviousTrack();
		else if (button == Controls::ControlButton::Next)
			NextTrack();
		else if (button == Controls::ControlButton::CaptureCheckbox)
			platform->HookKeyboard();
		else if (button == Controls::ControlButton::RotateCheckbox) {
			rotating = !rotating;
			if (!rotating) frameCount = 0;
		} else
			TogglePlaying();

	} else if (!miniPlayer && !platform->OnMouseClicked(mousePos) && albumArt.OnMouseClicked(mousePos)) {
		TogglePlaying();
		if (!playing) controls.GetPause().Fade(true);
		else controls.GetPlay().Fade(true);
	} else if (!miniPlayer) {
		if (mousePos.x > windowWidth / 2 && doubleClick.OnClick({ windowWidth / 2, 0 })) {
			NextTrack();
			controls.GetNext().Fade(true);
		} else if (mousePos.x < windowWidth / 2 && doubleClick.OnClick({ 0, 0 })) {
			PreviousTrack();
			controls.GetPrevious().Fade(true);
		}
	} else if (controls.GetHelp().IsHovered() /* short circuit */ || (miniPlayer && !platform->OnMouseClicked(mousePos))) {
		return IsOnCloseButton(mousePos);
	}

	return false;
}

bool CApp::OnMouseDown(const Vector2i &mousePos, MouseDownState *state) {
	if (controls.OnMouseDown(mousePos))
		return true;

	auto ret = fileLoaded && !miniPlayer ? SeekToMousePos(mousePos) : false;

	if (!ret && miniPlayer) {
		if (!albumArt.OnMouseDown(mousePos) && (state || !platform->OnMouseDown(mousePos))) {
			lastMousePos = mousePos;

			if (fileLoaded) {
				// Did we click the mini player's seekbar?
				controls.OnMouseClicked(mousePos, [this](float pos) {
					auto time = pos * controls.GetCurrentSongLength();

					if (auto &cue = controls.GetPlaylist().GetCue())
						time += cue->GetCurrentTrack()->startTime;

					SeekTo(time);

					lastMousePos = std::nullopt;
				}, playing, false);
			}

			SDL_CaptureMouse(lastMousePos.has_value());

			if (state && lastMousePos) {
				lastMousePos = std::nullopt;
				*state = MouseDownState::Dragging;
			}
		} else {
			// INSANE HACK:
			//   In order to prevent redrawing while resizing
			//   (creating flicker), we enable chroma keying
			//   on the window. This appears to bypass redrawing
			//   entirely. However, when using Vulkan, this also
			//   causes ALL graphical elements to become semi-
			//   transparent, no matter their distance from
			//   the chroma key (multiplicative blending, possibly?).
			//   The only way I've found to prevent this is to use
			//   BGR instead of RGB. I've absolutely NO IDEA why
			//   this works, but it's what I'm going with
			//   for now.
			//
			//   A more proper solution will probably be to hijack
			//   window creation from SDL so we have full access
			//   to WindowProc. SDL's event filters only go so
			//   far and can't seem to intercept WM_NCCALCSIZE
			//
			//   Further reading:
			//      https://stackoverflow.com/questions/53000291/how-to-smooth-ugly-jitter-flicker-jumping-when-resizing-windows-especially-drag
			if (vulkan) {
				if (!HDR::Enabled) {
					dynamic_cast<Vulkan *>(platform->GetInterop())->SetFormat(
						VK_FORMAT_B8G8R8A8_UNORM,
						VK_COLORSPACE_SRGB_NONLINEAR_KHR,
						GL_RGBA8
					);

					platform->SetBgr(true, *context);
				}
			}

			// Make sure to hide any MiniPlayerLists
			controls.GetPlaylist().SetFadeSpeed(4.0f);
			controls.GetPlaylist().SetHovered(false, false, false, [this] {
				controls.GetPlaylist().SetFadeSpeed(2.0f);
			});
			controls.GetPresetList().SetFadeSpeed(4.0f);
			controls.GetPresetList().SetHovered(false, false, false, [this] {
				controls.GetPresetList().SetFadeSpeed(2.0f);
			});

			platform->SetChromaKey(true);

			resizeTimer = std::chrono::system_clock::now();
			scaleTimer = std::chrono::system_clock::now();

			if (state)
				*state = MouseDownState::Resizing;
		}

		ret = (state == nullptr);
	}

	return ret;
}

void CApp::OnMouseUp(const Vector2i &mousePos) {
	if (miniPlayer && !platform->IsResizing()) {
		LogInfo("Mouse up...");

		resizeTimer = std::nullopt;
		scaleTimer = std::nullopt;

		controls.OnMouseUp(mousePos);

		miniPlayerVisualizerRatio = Settings::settings.GetMiniPlayerVisualizerRatio();

		lastMousePos = std::nullopt;

		SDL_CaptureMouse(false);

		if (vulkan) {
			if (!HDR::Enabled) {
				dynamic_cast<Vulkan *>(platform->GetInterop())->SetFormat(
					VK_FORMAT_R8G8B8A8_UNORM,
					VK_COLORSPACE_SRGB_NONLINEAR_KHR,
					GL_RGBA8
				);

				platform->SetBgr(false, *context);
			}
		}

		Settings::settings.SetMiniPlayerX(windowX, true);
		Settings::settings.SetMiniPlayerY(windowY, true);
		Settings::settings.SetMiniPlayerWidth(albumArt.GetRadius(miniPlayer) * miniPlayerVisualizerRatio / scale, true);
		Settings::settings.SetMiniPlayerHeight(albumArt.GetRadius(miniPlayer) * miniPlayerVisualizerRatio / scale, true);

		platform->SetChromaKey(false);
		platform->UpdateWindowShape();

		Settings::settings.Save();
	}
}

void CApp::OnMouseMoved(const Vector2i &mousePos) {
	if (controls.IsScrolling()) {
		albumArt.OnMouseLeave();
		return;
	}

	controls.OnMouseMoved(mousePos);

	if (controls.IsScrollBarHovered()) {
		albumArt.OnMouseLeave();
		return;
	}

	if (miniPlayer && !controls.GetHelp().IsHovered())
		albumArt.OnMouseMoved(mousePos);
}

bool CApp::OnMouseDragged(const Vector2i &mousePos) {
	if (controls.OnMouseDragged(mousePos))
		return false;

	bool seek = false;

	if (fileLoaded && !lastMousePos && !albumArt.IsResizing()) {
		// Did we drag the mini player's seekbar?
		controls.OnMouseClicked(mousePos, [this, &seek](float pos) {
			seek = true;
			SeekTo(pos * controls.GetCurrentSongLength());
		}, playing);
	}

	if (lastMousePos) {
		if (!seek) {
			if (windowX == SDL_WINDOWPOS_CENTERED)
				SDL_GetWindowPosition(sdlWindow, &windowX, &windowY);

			Vector2i delta = {
				(mousePos.x - lastMousePos->x),
				(mousePos.y - lastMousePos->y)
			};

			// Don't allow window to move further than halfway outside of the total display area
			windowX = std::clamp(windowX + delta.x, displayBoundingBox.x - windowWidth / 2, displayBoundingBox.w - windowWidth / 2);
			windowY = std::clamp(windowY + delta.y, displayBoundingBox.y - windowHeight / 2, displayBoundingBox.h - windowHeight / 2);

			SDL_SetWindowPosition(sdlWindow, windowX, windowY);

			// The window moving is going to change
			// our mouse pos by delta
			lastMousePos = mousePos - delta;
		}
	} else if(const auto now = std::chrono::system_clock::now();
		// Only allow resizes to occur at 30FPS
		resizeTimer && now - *resizeTimer >= 0.0334s && 
		miniPlayer && !controls.GetHelp().IsHovered() && albumArt.OnMouseDragged(mousePos)) {
		const auto &ratio = Settings::settings.GetMiniPlayerVisualizerRatio();
		const auto oldRadius = dynamic_cast<Circle<Circles::Textured>*>(&albumArt)->GetRadius();

		const auto newRadius = albumArt.GetRadius(miniPlayer);

		if (!platform->AllowsWindowMovement()) {
			miniPlayerVisualizerRatio = std::min(windowWidth, windowHeight) / newRadius;
			Settings::settings.SetMiniPlayerVisualizerRatio(miniPlayerVisualizerRatio);
		}

		SetRadius(newRadius);

		if (platform->AllowsWindowMovement()) {
			Vector2i delta = {
				std::lround((newRadius * ratio - windowWidth) / 2),
				std::lround((newRadius * ratio - windowHeight) / 2)
			};

			// If deltas don't match, we likely hit the edge of the
			// screen. Let us _shrink_ in this case, but not _grow_
			if (delta.x == delta.y || (delta.x < 0 && delta.y < 0)) {
				windowX -= delta.x;
				windowY -= delta.y;

				platform->SetWindowPos(
					windowX,
					windowY,
					std::lround(newRadius * ratio),
					std::lround(newRadius * ratio)
				);

				if (!vulkan)
					OnResize(std::lround(newRadius * ratio), std::lround(newRadius * ratio), scale, true);

				miniPlayerVisualizerRatio = ratio;
			} else { // If we hit the edge of the screen, revert the change
				Settings::settings.SetMiniPlayerVisualizerRatio(miniPlayerVisualizerRatio, true);

				// FIXME: Make this a proactive approach instead of a reactive one
				SetRadius(oldRadius);

				Settings::settings.SetMiniPlayerFontSize(controls.UpdateFontSize() / scale, true);
			}
		} else {
			controls.OnResize(
				windowWidth,
				windowHeight,
				*context,
				scale,
				platform->GetDefaultFramebuffer(),
				miniPlayer
			);

			close.OnResize(controls.GetIconSize());
		}

		// Rescale the album art every second while scaling
		if (const auto now = std::chrono::system_clock::now(); scaleTimer && now - *scaleTimer >= 1s) {
			albumArt.Scale(true);
			scaleTimer = now;
		}

		resizeTimer = now;

		return true;
	} else if (!miniPlayer) SeekToMousePos(mousePos, true);

	return false;
}

void CApp::OnMouseLeave() {
	albumArt.OnMouseLeave();
	close.SetHovered(false);
}

void CApp::LoadPreset(std::optional<std::size_t> index) {
	const auto &presets = Preset::GetPresets();

	if (index && presets.size() > *index) {
		const auto &preset = presets.at(*index);

		LoadPreset(preset);
	} else index = std::nullopt;

	if (miniPlayer) {
		Settings::settings.SetMiniPlayerPresetIndex(index);
		controls.OnPresetChanged(presets);
	} else {
		Settings::settings.SetPresetIndex(index);
		presetIndex = index;
	}
}

void CApp::LoadPreset(const Preset &preset) {
	LogDebug("Loading preset '", preset.GetName(), "'");

	if (preset.GetName() == "Random") {
		presetIndex = std::nullopt;
		Settings::settings.SetPresetIndex(presetIndex, true);
	}

	if (auto renderer = preset.GetRenderer()) {
		LoadRenderer(*renderer);

		if (auto lineRendererStyle = preset.GetLineRendererStyle()) {
			if (auto lineRenderer = dynamic_cast<LineRenderer *>(this->renderer)) {
				lineRenderer->SetStyle(*lineRendererStyle);
				Settings::settings.SetLineRendererStyle(*lineRendererStyle);
			}
		}
	}

	if (auto scale = preset.GetScale(); scale && !miniPlayer)
		SetVisualizerScale(*scale);
	else // scale doesn't apply to mini-player
		SetVisualizerScale(1.0f);

	SetBufferLength(preset.GetBufferSize());
	dynamicGain = preset.GetDynamicGain();
	// FFTRenderer needs to be reset
	// with the new DynamicGain
	renderer->Reset();
	SetFftLength(preset.GetFftSize());
	SetDecayTime(preset.GetDecayTime());
	SetFadeTime(preset.GetFadeTime());
	renderer->SetPulse(preset.GetPulse());
	pulseBackground = preset.GetPulseBackground();
	context->With("blur"_hash, [this](Context::Shader &shader) {
		shader.program.Uniform1i("premultipliedAlpha"_hash, platform->IsAlphaPremultiplied());
	});
	Settings::settings.SetPulseBackground(pulseBackground, true);
	renderer->SetPulseTime(preset.GetPulseTime());

	if (const auto &rendererOffset = preset.GetRendererOffset()) {
		renderer->SetOffset(*rendererOffset);
		Settings::settings.SetRendererOffset(*rendererOffset, true);
	} else {
		renderer->SetOffset(0);
		Settings::settings.SetRendererOffset(0, true);
	}

	SetStrobe(preset.GetStrobe());
	strobeIntensity = preset.GetStrobeIntensity();
	Settings::settings.SetStrobeIntensity(strobeIntensity, true);

	auto rotating = preset.GetRotating();

	if (miniPlayer) rotating = Settings::settings.GetMiniPlayerRotating();

	if (rotating)
		SetRotating(*rotating);

	if (rotating && *rotating) {
		SetRotationSpeed(preset.GetRotationSpeed());
	} else {
		frameCount = 0; // Reset rotation
		//SetRotationSpeed(6.0f /* default */);
	}

	auto blur = preset.GetBlur();
	SetBlur(blur && *blur);

	// Make sure we don't blow out any existing colors
	if ((blur && *blur) && (Settings::IsColorBlend(sourceFactor) != Settings::IsColorBlend(preset.GetSourceFactor()) ||
		Settings::IsColorBlend(destFactor) != Settings::IsColorBlend(preset.GetDestFactor()) ||
		miniPlayer))
		ClearBlurFbo();

	sourceFactor = preset.GetSourceFactor();
	Settings::settings.SetSourceFactor(sourceFactor, true);
	destFactor = preset.GetDestFactor();
	Settings::settings.SetDestFactor(destFactor, true);
	sourceAlphaFactor = preset.GetSourceAlphaFactor();
	Settings::settings.SetSourceAlphaFactor(sourceAlphaFactor, true);
	destAlphaFactor = preset.GetDestAlphaFactor();
	Settings::settings.SetDestAlphaFactor(destAlphaFactor, true);
	if (blur && *blur) {
		SetBlurIntensity(preset.GetBlurIntensity());

		Settings::settings.SetBlurOpacity(preset.GetBlurOpacity(), true);
		blurOpacity = preset.GetBlurOpacity();
	}

	Settings::settings.SetEffectIntensity(preset.GetEffectIntensity(), true);
	Settings::settings.SetEffectXOffset(preset.GetEffectXOffset(), true);
	Settings::settings.SetEffectYOffset(preset.GetEffectYOffset(), true);
	Settings::settings.SetEffectRadiation(preset.GetEffectRadiation(), true);
	Settings::settings.SetEffectHorizontalSpread(preset.GetEffectHorizontalSpread(), true);
	Settings::settings.SetEffectVerticalSpread(preset.GetEffectVerticalSpread(), true);
	Settings::settings.SetEffectRotation(preset.GetEffectRotation(), true);
	Settings::settings.SetDynamicGain(preset.GetDynamicGain(), true);

	// Force a save at the end, since
	// all settings changes were delayed
	Settings::settings.Save();

	SetEffect(preset.GetEffect());

	updateUi += 1;
}

void CApp::SaveBlurFBO() {
	auto time = ::time(nullptr);
	auto tm = *std::localtime(&time);
	std::stringstream filename;

	platform->GetPicturesPath(filename);

	filename << "popRocks-" << std::put_time(&tm, "%Y%m%d%H%M%S") << ".png";

	blurFbo->SaveAsPNG(filename.str());
}

void CApp::UpdateDisplayBoundingBox() {
	int numDisplays = 0;
	const auto displays = SDL_GetDisplays(&numDisplays);

	displayBoundingBox = { 0, 0, 0, 0 };
	for (int i = 0; i < numDisplays; ++i) {
		SDL_Rect bounds;
		SDL_GetDisplayBounds(displays[i], &bounds);

		if (bounds.x < displayBoundingBox.x)
			displayBoundingBox.x = bounds.x;
		if (bounds.y < displayBoundingBox.y)
			displayBoundingBox.y = bounds.y;

		if (bounds.x + bounds.w > displayBoundingBox.w)
			displayBoundingBox.w = bounds.x + bounds.w;
		if (bounds.y + bounds.h > displayBoundingBox.h)
			displayBoundingBox.h = bounds.y + bounds.h;
	}
}

bool CApp::AddToScrollOffset(int offset) {
	if (!controls.AddToScrollOffset(offset)) {
		const auto modState = SDL_GetModState();

		if (modState & SDL_KMOD_CTRL) {
			if (auto lineRenderer = dynamic_cast<LineRenderer *>(renderer); lineRenderer && miniPlayer && fileLoaded) {
				const auto lineWidth = std::clamp(Settings::settings.GetWidth() + offset, 1.0f, 10.0f);
				lineRenderer->SetWidth(lineWidth);
				Settings::settings.SetWidth(lineWidth);

				LogDebug("Line width set to ", lineWidth);

				controls.ShowMessage("Line width set to " + std::to_string(static_cast<int>(lineWidth)));

				return true;
			}
		} else if (modState & SDL_KMOD_SHIFT) {
			// If we don't have an offset, start it at 0
			if (!audioOffset) audioOffset = 0.0f;

			*audioOffset += offset / 100.0f;

			// If we're back to 0, remove offset
			if (*audioOffset < 0.01f && *audioOffset > -0.01f)
				audioOffset = std::nullopt;

			std::stringstream stream;
			stream << "Audio offset set to " << std::setprecision(2) << std::fixed << std::setfill('0') << (audioOffset ? *audioOffset : 0) << "s";

			controls.ShowMessage(stream.str());

			Settings::settings.SetAudioOffset(audioOffset);
		}

		return false;
	}

	return true;
}

void CApp::OnColorChanged(const MathsCPP::Colour<float> &color, bool silent) {
	if (!silent)
		LogDebug("Color changed!");

	auto hsv = color.ToHsv();
	hsv.v = 1.0f;
	brightColor = Colour<float>::FromHsv(hsv);
	if (HDR::Enabled) {
		brightColor.Tone(
			Settings::settings.GetAlbumArtGamma(),
			Settings::settings.GetAlbumArtContrast(),
			Settings::settings.GetAlbumArtBrightness(),
			HDR::WhiteLevel * (pulseMaxBrightness ? HDR::Headroom : 1.0f)
		);
	}

	hsv.v = 0.55f;
	darkColor = Colour<float>::FromHsv(hsv);
	if (HDR::Enabled) {
		darkColor.Tone(
			Settings::settings.GetAlbumArtGamma(),
			Settings::settings.GetAlbumArtContrast(),
			Settings::settings.GetAlbumArtBrightness(),
			(pulseMaxBrightness ? HDR::WhiteLevel * HDR::Headroom : 1.0f)
		);
	}

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

void CApp::OnBlackChanged(const float &black) {
	context->With("blur"_hash, [this](Context::Shader &shader) {
		const auto premultipliedAlpha = platform->IsAlphaPremultiplied();
		LogDebug("Turning premultiplied alpha ", premultipliedAlpha ? "ON" : "OFF");
		shader.program.Uniform1i("premultipliedAlpha"_hash, premultipliedAlpha);
	});
}