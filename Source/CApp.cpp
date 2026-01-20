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
CApp::CApp() : albumArt(context), controls(&albumArt), circleLine(12.0f), prng(time(nullptr)), platform(PlatformFactory::Build(
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
)) {
	renderer = RendererFactory::Build(
		Settings::settings.GetRenderer(),
		&dynamicGain,
		&albumArt
	);

	// If we close the console, make sure to
	// clean up before exit
	Logger::SetOnClose([this] {
		OnDestroy();
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

	scale *= SDL_GetWindowDisplayScale(window);

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
		}

		if (hash == "basic"_hash) {
			shader.program.CacheUniformLocation("bgr");
			shader.program.Uniform1i("bgr"_hash, 0);
		}
	}
}

void CApp::UpdateHdrProperties(bool force) {
	platform->UpdateHdrProperties(force);

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

	SDL_PropertiesID props = SDL_CreateProperties();

	SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "popRocks");
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, windowWidth);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, windowHeight);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, Settings::settings.GetWindowX());
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, Settings::settings.GetWindowY());
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_VULKAN_BOOLEAN, platform->GetVulkanProperty());
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, platform->GetOpenGlProperty());
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);

	sdlWindow = SDL_CreateWindowWithProperties(
		props
	);

	platform->OpenOpenGlWindow(props);

	if (platform->CreateOpenGlContext()) {
		LogDebug("gladLoadGL() returned ", platform->LoadGlad());

		context = std::make_unique<Context>();

		LoadShaders();

		Interop::InitArgs args;

		args.adapterIndex = platform->GetAdapterIndex();
		args.width = windowWidth;
		args.height = windowHeight;
		args.surfaceCallback = [&](void *instance) {
			VkSurfaceKHR surface;

			if (!SDL_Vulkan_CreateSurface(sdlWindow, reinterpret_cast<VkInstance>(instance), nullptr, &surface))
				LogError("Could not create Vulkan surface: ", SDL_GetError());

			return reinterpret_cast<void*>(surface);
		};

		platform->OnInit(args, *context);

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

		menu.SetOnOpen([this](const std::filesystem::path &path) {
			LoadFile(path);
			ImGui::SetWindowFocus(nullptr);
		});
		menu.SetOnBufferSizeChanged([this](int bufferSize) {
			SetBufferLength(bufferSize);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu.SetOnDecayTimeChanged([this](float decayTime) {
			SetDecayTime(
				std::chrono::duration<double> {
					decayTime
				}
			);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu.SetOnFadeTimeChanged([this](float fadeTime) {
			SetFadeTime(
				std::chrono::duration<double> {
					fadeTime
				}
			);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu.SetOnPulseChanged([this](bool pulse) {
			renderer->SetPulse(pulse);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu.SetOnPulseBackgroundChanged([this](bool pulseBackground) {
			Settings::settings.SetPulseBackground(pulseBackground);

			this->pulseBackground = pulseBackground;
			context->With("blur"_hash, [this](Context::Shader &shader) {
				shader.program.Uniform1i("premultipliedAlpha"_hash, platform->IsAlphaPremultiplied());
			});

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu.SetOnDarkenPulseOnBrightColorsChanged([this](bool darkenPulseOnBrightColors) {
			Settings::settings.SetDarkenPulseOnBrightColors(darkenPulseOnBrightColors);

			this->darkenPulseOnBrightColors = darkenPulseOnBrightColors;
		});
		menu.SetOnPulseTimeChanged([this](float pulseTime) {
			renderer->SetPulseTime(
				std::chrono::duration<double> {
					pulseTime
				}
			);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu.SetOnStrobeChanged([this](bool strobe) {
			SetStrobe(strobe);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu.SetOnStrobeIntensityChanged([this](float strobeIntensity) {
			Settings::settings.SetStrobeIntensity(strobeIntensity);

			this->strobeIntensity = strobeIntensity;

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu.SetOnBlurChanged([this](bool blur) {
			ToggleBlur();
			ImGui::SetWindowFocus(nullptr);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu.SetOnSourceFactorChanged([this](GLenum sourceFactor) {
			Settings::settings.SetSourceFactor(sourceFactor);

			this->sourceFactor = sourceFactor;

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu.SetOnDestFactorChanged([this](GLenum destFactor) {
			Settings::settings.SetDestFactor(destFactor);
			
			this->destFactor = destFactor;

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu.SetOnBlurIntensityChanged([this](float blurIntensity) {
			SetBlurIntensity(blurIntensity);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu.SetOnBlurOpacityChanged([this](float blurOpacity) {
			Settings::settings.SetBlurOpacity(blurOpacity);

			this->blurOpacity = blurOpacity;

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu.SetOnRotatingChanged([this](bool rotate) {
			SetRotating(rotate);
			ImGui::SetWindowFocus(nullptr);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu.SetOnRpmChanged([this](float rpm) {
			SetRotationSpeed(rpm * (360.0f / 60.0f) /* 6 */);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu.SetOnDetectBpmChanged([this](bool detectBpm) {
			for (auto &detector : beatDetectors)
				detector.SetDetecting(detectBpm);

			Settings::settings.SetDetectBpm(beatDetect->IsDetecting());

			if (beatDetect->IsDetecting() && !loadedFile.empty()) {
				for (auto &detector : beatDetectors)
					detector.Cancel();

				auto stream = platform->OpenWithFlags(loadedFile, loadedFileExtension, BASS_STREAM_PRESCAN | BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT);

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
		menu.SetOnVisualizationTypeChanged([this](const std::string &visualizationType) {
			LoadRenderer(visualizationType);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu.SetOnLightPackVisualizationTypeChanged([this](const std::string &lightPackVisualizationType) {
			lightPack.SetLightType(
				lightPackVisualizationType == "intensity" ?
					LightPack::LightType::Intensity :
					lightPackVisualizationType == "color" ?
						LightPack::LightType::Color :
						LightPack::LightType::ColorIntensity
			);
		});
		menu.SetOnLightPackMappingChanged([this](const std::string &lightPackMapping) {
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
		menu.SetOnLightPackFocusAreaChanged([this](const std::string &lightPackFocusArea) {
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
		menu.SetOnRadiusChanged([this](int radius) {
			albumArt.SetRadius(radius);
			albumArt.Scale(true);
			controls.GetVolume().SetRadius(radius);
			controls.GetPause().OnResize(radius);
			controls.GetPlay().OnResize(radius);
		});
		menu.SetOnLineWidthChanged([this](float lineWidth) {
			if (auto lineRenderer = dynamic_cast<LineRenderer *>(renderer))
				lineRenderer->SetWidth(lineWidth);
		});
		menu.SetOnSmoothChanged([this](int smooth) {
			lightPack.SetSmooth(smooth);
		});
		menu.SetOnGammaChanged([this](float gamma) {
			// Silent so it doesn't spam the console
			// while the user is dragging
			lightPack.SetGamma(gamma, true);
		});
		menu.SetOnPresetChanged([this](std::optional<std::size_t> preset) {
			LoadPreset(preset);
		});
		menu.SetOnCurrentSongVisibleChanged([this](bool currentSongVisible) {
			controls.GetPlaylist().SetCurrentSongVisible(currentSongVisible);
		});
		menu.SetOnColorSelectionChanged([this](const Settings::ColorSelection &selection) {
			Settings::settings.SetColorSelection(selection);
			albumArt.ReprocessColors();
		});
		menu.SetOnFftSizeChanged([this](int fftSize) {
			SetFftLength(fftSize);
		});
		menu.SetOnListeningChanged([this](bool listening) {
			if (listening)
				platform->Listen();
			else
				platform->StopListening();

			Settings::settings.SetListening(listening);
		});
		menu.SetOnLoopbackChanged([this](bool loopback) {
			if (loopback)
				platform->Listen(true);
			else
				platform->StopListening();

			Settings::settings.SetLoopback(loopback);
		});
		menu.SetOnOutputDeviceChanged([this](const std::string &outputDevice) {
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

					Open(loadedFile, loadedFileExtension, false, streamHandle, true);

					SeekTo(pos);
					TogglePlaying();
				}
			}
		});
		menu.SetOnInputDeviceChanged([this](const std::string &inputDevice) {
			Settings::settings.SetInputDevice(inputDevice);

			if (platform->IsListening() && !Settings::settings.GetLoopback())
				platform->Listen();
		});
		menu.SetOnEffectChanged([this](const std::string &effect) {
			SetEffect(effect);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu.SetOnEffectIntensityChanged([this](float effectIntensity) {
			Settings::settings.SetEffectIntensity(effectIntensity);

			this->context->With("blur"_hash, [effectIntensity](Context::Shader &shader) {
				shader.program.Uniform1f("effectIntensity"_hash, effectIntensity);
			});

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu.SetOnEffectXOffsetChanged([this](float effectXOffset) {
			Settings::settings.SetEffectXOffset(effectXOffset);

			this->context->With("blur"_hash, [effectXOffset](Context::Shader &shader) {
				shader.program.Uniform1f("effectXOffset"_hash, effectXOffset);
			});

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu.SetOnEffectYOffsetChanged([this](float effectYOffset) {
			Settings::settings.SetEffectYOffset(effectYOffset);

			this->context->With("blur"_hash, [effectYOffset](Context::Shader &shader) {
				shader.program.Uniform1f("effectYOffset"_hash, effectYOffset);
			});

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		// TODO: Add setting to change radiation function (circle, centered horizontal line (iTunes-style), etc.)
		menu.SetOnEffectRadiationChanged([this](float effectRadiation) {
			Settings::settings.SetEffectRadiation(effectRadiation);

			this->context->With("blur"_hash, [effectRadiation](Context::Shader &shader) {
				shader.program.Uniform1f("effectRadiation"_hash, effectRadiation);
			});

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu.SetOnEffectHorizontalSpreadChanged([this](float effectHorizontalSpread) {
			Settings::settings.SetEffectHorizontalSpread(effectHorizontalSpread);

			this->context->With("blur"_hash, [effectHorizontalSpread](Context::Shader &shader) {
				shader.program.Uniform1f("effectHorizontalSpread"_hash, effectHorizontalSpread);
			});

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu.SetOnEffectVerticalSpreadChanged([this](float effectVerticalSpread) {
			Settings::settings.SetEffectVerticalSpread(effectVerticalSpread);

			this->context->With("blur"_hash, [effectVerticalSpread](Context::Shader &shader) {
				shader.program.Uniform1f("effectVerticalSpread"_hash, effectVerticalSpread);
			});

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu.SetOnEffectRotationChanged([this](float effectRotation) {
			Settings::settings.SetEffectRotation(effectRotation);

			this->context->With("blur"_hash, [effectRotation](Context::Shader &shader) {
				shader.program.Uniform1f("effectRotation"_hash, effectRotation);
			});

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu.SetOnLimitFramerateChanged([this](bool limitFramerate) {
			Settings::settings.SetLimitFramerate(limitFramerate);

			if (limitFramerate)
				frameLimit = Settings::settings.GetFrameLimit();
			else
				frameLimit = -1;
		});
		menu.SetOnFrameLimitChanged([this](int frameLimit) {
			Settings::settings.SetFrameLimit(frameLimit);

			this->frameLimit = frameLimit;
		});
		menu.SetOnRandomizeChanged([this](bool randomize) {
			Settings::settings.SetRandomize(randomize);

			if (randomize)
				randomizeTime = Settings::settings.GetRandomizeTime();
			else
				randomizeTime = std::nullopt;
		});
		menu.SetOnRandomizeTimeChanged([this](float randomizeTime) {
			this->randomizeTime = Duration<Microseconds>(
				std::chrono::duration<double>(
					static_cast<double>(randomizeTime)
				)
			);

			Settings::settings.SetRandomizeTime(
				*this->randomizeTime
			);
		});
		menu.SetOnScaleChanged([this](float scale) {
			SetVisualizerScale(scale);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu.SetOnSelectedPresetsChanged([this](const std::set<std::size_t> &selectedPresets) {
			Settings::settings.SetSelectedPresets(selectedPresets);
		});
		menu.SetOnRandomizePresetsChanged([this](bool randomizePresets) {
			Settings::settings.SetRandomizePresets(randomizePresets);

			if (randomizePresets)
				randomizePresetsTime = Settings::settings.GetRandomizePresetsTime();
			else
				randomizePresetsTime = std::nullopt;
		});
		menu.SetOnRandomizePresetsTimeChanged([this](float randomizePresetsTime) {
			this->randomizePresetsTime = Duration<Microseconds>(
				std::chrono::duration<double>(
					static_cast<double>(randomizePresetsTime)
				)
			);

			Settings::settings.SetRandomizePresetsTime(
				*this->randomizePresetsTime
			);
		});
		menu.SetOnRandomizePresetsByBeatChanged([this](bool randomizePresetsByBeats) {
			Settings::settings.SetRandomizePresetsByBeats(randomizePresetsByBeats);

			if (randomizePresetsByBeats) {
				randomizePresetsBeats = Settings::settings.GetRandomizePresetsBeats();
				UpdateBeatCounter();
			} else randomizePresetsBeats = std::nullopt;
		});
		menu.SetOnRandomizePresetsBeatsChanged([this](int randomizePresetsBeats) {
			Settings::settings.SetRandomizePresetsBeats(randomizePresetsBeats);

			this->randomizePresetsBeats = randomizePresetsBeats;

			UpdateBeatCounter();
		});
		menu.SetOnResetRotation([this] {
			frameCount = 0;
			SetRotating(false);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu.SetOnClearBlurFbo([this] {
			ClearBlurFbo();
		});
		menu.SetOnResetWindow([this] {
			Settings::settings.SetWindowWidth(1920);
			Settings::settings.SetWindowHeight(1080);
			Settings::settings.SetWindowX(SDL_WINDOWPOS_CENTERED);
			Settings::settings.SetWindowY(SDL_WINDOWPOS_CENTERED);

			SDL_SetWindowSize(sdlWindow, Settings::settings.GetWindowWidth(), Settings::settings.GetWindowHeight());
			SDL_SetWindowPosition(sdlWindow, Settings::settings.GetWindowX(), Settings::settings.GetWindowY());
		});
		menu.SetOnQuit([this] {
			SDL_Event event;
			event.type = SDL_EVENT_QUIT;
			SDL_PushEvent(&event);
		});
		menu.SetOnRandom([this] {
			LoadPreset(Preset::Random());
		});
		menu.SetOnAutoFadeChanged([this](bool autoFade) {
			Settings::settings.SetAutoFade(autoFade);
		});
		menu.SetOnWaitTimeChanged([this](float waitTime) {
			auto duration = Duration<Microseconds>(
				std::chrono::duration<double>(waitTime)
			);

			Settings::settings.SetWaitTime(duration);
			controls.SetWaitTime(duration);
		});
		menu.SetOnAutoFadeSpeedChanged([this](float autoFadeSpeed) {
			Settings::settings.SetAutoFadeSpeed(autoFadeSpeed);
			controls.SetAutoFadeSpeed(autoFadeSpeed);
		});
		menu.SetOnExclusiveChanged([this](bool exclusive) {
			ToggleExclusive();
		});
		menu.SetOnExclusiveVolumeChanged([this](int volume) {
			controls.GetVolume().SetVolume(volume);
		});
		menu.SetOnHalveBpmChanged([this](bool halveBpm) {
			Settings::settings.SetHalveBpm(halveBpm);
		});
		menu.SetOnRendererOffsetChanged([this](int rendererOffset) {
			Settings::settings.SetRendererOffset(rendererOffset);

			renderer->SetOffset(rendererOffset);

			// We deviated from a preset
			LoadPreset(std::nullopt);
		});
		menu.SetOnLutChanged([this](const std::string &lut) {
			Settings::settings.SetLut(lut);

			albumArt.GetCube()->Load(Utils::GetResource(std::filesystem::path("LUTs") / lut));
		});
		menu.SetOnAlbumArtGammaChanged([this](float gamma) {
			Settings::settings.SetAlbumArtGamma(gamma);

			this->context->With("texture"_hash, [this, gamma](Context::Shader &shader) {
				shader.program.Uniform1f("gamma"_hash, gamma);
			});
		});
		menu.SetOnAlbumArtContrastChanged([this](float contrast) {
			Settings::settings.SetAlbumArtContrast(contrast);

			this->context->With("texture"_hash, [this, contrast](Context::Shader &shader) {
				shader.program.Uniform1f("contrast"_hash, contrast);
			});
		});
		menu.SetOnAlbumArtBrightnessChanged([this](float brightness) {
			Settings::settings.SetAlbumArtBrightness(brightness);

			this->context->With("texture"_hash, [this, brightness](Context::Shader &shader) {
				shader.program.Uniform1f("brightness"_hash, brightness);
			});
		});
		menu.SetOnHdrWhitePointChanged([this](std::optional<float> hdrWhitePoint) {
			Settings::settings.SetHdrWhitePoint(hdrWhitePoint);

			if (hdrWhitePoint)
				HDR::SetWhiteLevel(*hdrWhitePoint);
		});
		menu.SetOnRescanAlbumArt([this] {
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
		menu.SetOnPulseUiChanged([this](bool pulseUi) {
			Settings::settings.SetPulseUi(pulseUi);

			if (!pulseUi) {
				albumArt.RemoveColorChangeListener(&menu);
				menu.OnColorChanged(visColor);
			} else {
				albumArt.AddColorChangeListener(&menu);
				menu.OnColorChanged(GetColor());
			}
		});
		menu.SetOnUiGammaChanged([this](float uiGamma) {
			Settings::settings.SetUiGamma(uiGamma);

			this->uiGamma = uiGamma;
		});
		menu.SetOnUiContrastChanged([this](float uiContrast) {
			Settings::settings.SetUiContrast(uiContrast);

			this->uiContrast = uiContrast;
		});
		menu.SetOnUiBrightnessChanged([this](float uiBrightness) {
			Settings::settings.SetUiBrightness(uiBrightness);

			this->uiBrightness = uiBrightness;
		});
		menu.SetOnPulseMaxBrightnessChanged([this](bool pulseMaxBrigtness) {
			Settings::settings.SetPulseMaxBrightness(pulseMaxBrigtness);

			this->pulseMaxBrightness = HDR::Enabled && pulseMaxBrigtness;
		});
		menu.SetOnVsyncChanged([this](bool vsync) {
			Settings::settings.SetVsync(vsync);
			
			UpdateVsync();
		});

		platform->AddMenuCallbacks(&menu);

		menu.OnColorChanged(visColor);
#endif
	} else LogError("Could not create OpenGL context: ", platform->GetOpenGlContextError());

	LogDebug("OpenGL Version: ", reinterpret_cast<const char*>(glGetString(GL_VERSION)));

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
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

	if (BASS_Init(platform->GetDeviceIndex<true>(Settings::settings.GetOutputDevice()), freq, 0, 0, nullptr) != TRUE)
		LogError("Could not initialize audio device!");

	lightPack.OnInit();
	albumArt.OnInit(windowWidth, windowHeight, scale);
#if GUI
	albumArt.AddColorChangeListener(&menu);
#endif
	renderer->OnInit(windowWidth, windowHeight);
	albumArt.AddColorChangeListener(this);
	controls.OnInit(windowWidth, windowHeight, *context, scale, platform->GetDefaultFramebuffer());

	controls.SetFadeCallback([this](bool in) {
		if (in) SDL_ShowCursor();
		else SDL_HideCursor();
	});

	spindle.OnInit(albumArt.GetRadius() / SpindleSize);

	OnResize(windowWidth, windowHeight, scale);

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

void CApp::OnResize(int width, int height, float scale) {
	windowWidth = width;
	windowHeight = height;

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

	platform->OnResize(windowWidth, windowHeight);

	glViewport(0, 0, windowWidth, windowHeight);

	albumArt.OnResize(windowWidth, windowHeight, scale);
	controls.OnResize(windowWidth, windowHeight, *context, scale,
		platform->GetDefaultFramebuffer()
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
		});
	} else {
		maxDimension = windowWidth;
		hStep = static_cast<float>(windowWidth) / bufferLength;
		context->With("rotate"_hash, [width, height](Context::Shader &shader) {
			shader.program.Uniform2f("screenSize"_hash, width, height);
		});
		context->With("blit"_hash, [width, height](Context::Shader &shader) {
			shader.program.Uniform2f("screenSize"_hash, width, height);
		});
	}

	renderer->OnResize(windowWidth, windowHeight, maxDimension);

#if GUI
	uiFbo = std::make_unique<MultisampledFramebufferObject>(windowWidth, windowHeight, platform->GetFboInternalFormat(HDR::Enabled), platform->IsUiInverted());

	uiFbo->SetDefaultFramebuffer(platform->GetDefaultFramebuffer());

	// We want the menu to be a bit easier to touch
	// on the Steam Deck, so we enlarge it
	menu.OnResize(
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
				std::advance(preset, (prng() % selectedPresets.size()));
			} while (shuffledPresets.empty() && presetIndex && *preset == *presetIndex);

			shuffledPresets.emplace_back(*preset);
			selectedPresets.erase(preset);
		}
	}
	
	LoadPreset(*shuffledPresets.begin());
	shuffledPresets.erase(shuffledPresets.begin());
}

void CApp::OnLoop(const Delta &time) {
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
		if (auto outlineFont = controls.GetOutlineFont())
			outlineFont->SetDefaultFramebuffer(platform->GetInterop()->GetFramebuffer());
	} else if (!looped) return;

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	
	context->Use("texture"_hash);

	if (!fileLoaded && !platform->IsListening()) {
		SwapBuffers(time);
		return;
	}

	if (randomizeTime && std::chrono::system_clock::now() > lastRandomize + std::chrono::duration<double>(randomizeTime->AsSeconds())) {
		LoadPreset(Preset::Random());
		lastRandomize = std::chrono::system_clock::now();
	} else if (randomizePresetsTime && std::chrono::system_clock::now() > lastPresetRandomize + std::chrono::duration<double>(randomizePresetsTime->AsSeconds())) {
		LoadRandomPreset();
		lastPresetRandomize = std::chrono::system_clock::now();
	}

	if(fileLoaded) {
		if (renderer->IsFloatingPoint()) {
			if (!platform->ScaleExclusive(renderer, buffer, floatBuffer, shortBuffer))
				BASS_ChannelGetData(streamHandle, buffer, fftFlag);
		} else {
			if (!platform->ScaleExclusive(renderer, buffer, floatBuffer, shortBuffer))
				BASS_ChannelGetData(streamHandle, buffer, static_cast<DWORD>(bufferLength * sizeof(short) * channelInfo.chans));
		}
	} else {
		platform->LoadHeardSamples(renderer, floatBuffer, shortBuffer, bufferLength);
	}

	//rect.x = 0;
	//rect.y = 0;
	//rect.w = buffer[0]/1000000;
	//rect.h = 10;

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

	if (playing || platform->IsListening()) {
		auto hsv = color.ToHsv();
		auto brightHsv = this->brightColor.ToHsv();
		brightHsv.v = std::max(0.0f, brightHsv.v - strobeIntensity * lerp);

		renderer->OnLoop(
			time,
			fileLoaded,
			hStep,
			*context,
			!darkenPulseOnBrightColors || hsv.v < 0.66 * (HDR::Enabled ? HDR::WhiteLevel * HDR::Headroom : 1.0f) ? color : darkColor,
			((strobe && playing) ? Colour<float>::FromHsv(brightHsv) : ((!darkenPulseOnBrightColors || hsv.v < 0.66 * (HDR::Enabled ? HDR::WhiteLevel * HDR::Headroom : 1.0f)) ? this->brightColor : color)),
			frameCount,
			platform->GetMaxHeardSample(),
			resetGain
		);
	}

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
		else
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

		glClear(GL_COLOR_BUFFER_BIT);

		glBlendFunc(sourceFactor, destFactor);
		
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

		context->GetShaderProgram().Uniform1f("randomX"_hash, prng() / static_cast<float>(prng.max()));
		context->GetShaderProgram().Uniform1f("randomY"_hash, prng() / static_cast<float>(prng.max()));
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

		glBlendFunc(sourceFactor, destFactor);

		context->LoadIdentity();
		blurFbo->Unbind();
		
		context->Color(1.0f, 1.0f, 1.0f, 1.0f);

		glClear(GL_COLOR_BUFFER_BIT);

		if (playing)
			blurFbo->Draw(0, 0, *context, lastFrame.get());

		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		platform->BlitBlurFbo();

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
		windowWidth / 2.0f,
		windowHeight / 2.0f,
		frameCount,
		*context
	);

	// 45 RPM = 270
	// 33 RPM = 198
	// 33.34 RPM = 200.04
	if (rotationSpeed == 270.0f || (rotationSpeed >= 198.0f && rotationSpeed <= 200.05f)) {
		if (spindle.GetRadius() != albumArt.GetRadius() / SpindleSize)
			spindle.SetRadius(albumArt.GetRadius() / SpindleSize);

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
		GetColor()
	);
	
	// Line up the next file at >= 90% completion of current file
	if (controls.GetExclusiveIndicator().IsExclusive() && elapsed >= controls.GetCurrentSongLength() * 0.9 && !nextStreamHandle && !controls.GetPlaylist().GetCue()) {
		if (auto next = GetControls().GetPlaylist().GetNext()) {
			std::unique_lock lock(streamHandleMutex);
			auto extension = next->path.extension().u8string();
			std::transform(extension.begin(), extension.end(), extension.begin(), tolower);

			Open(next->path, extension, true, nextStreamHandle);

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
				playing = true;
			}
		} else {
			if (controls.GetExclusiveIndicator().IsExclusive())
				StopExclusive(FALSE);
			else
				Stop(FALSE);

			playing = false;
		}
	}

	if (stopWasapiOnNextLoop) {
		StopExclusive(FALSE);
		stopWasapiOnNextLoop = false;
	}

	beatDetectTime = elapsed - (controls.GetExclusiveIndicator().IsExclusive() ? platform->GetExclusiveBufferSize() : 0);
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
	}

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
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	// Keep the controls on screen if a menu is open
	context->Color(HDR::WhiteLevel, HDR::WhiteLevel, HDR::WhiteLevel, 1.0f);
	if (menu.OnLoop(lightPack, albumArt, *context))
		controls.Fade(true);

	if (menu.HasColorChanged() && updateUi == 0)
		updateUi = 1;

	// Keep the UI in an FBO and only update it as needed
	//
	// Why?
	// 
	// The high overhead involved in caching the OpenGL context (+5% CPU usage on a 9950X)
	// as part of ImGui_ImplOpenGL3_RenderDrawData() conflicts with my goal of ~1% CPU usage
	//LogDebug("updateUi = ", static_cast<int>(updateUi));
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
		context->Color(1.0f, 1.0f, 1.0f, (menu.IsPresetPopupVisible() ? 1.0f : 0.98f) * controls.GetAlpha());

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
	} else context->Color(HDR::WhiteLevel, HDR::WhiteLevel, HDR::WhiteLevel, (menu.IsPresetPopupVisible() ? 1.0f : 0.90f) * controls.GetAlpha());

#if VULKAN
	context->With("blit"_hash, [this](Context::Shader &shader) {
		shader.program.Uniform1f("yOffset"_hash, -windowHeight);

		if (platform->IsBgr()) shader.program.Uniform1i("bgr"_hash, 0);
	});
#endif

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

#if VULKAN
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
		if (auto outlineFont = controls.GetOutlineFont())
			outlineFont->SetDefaultFramebuffer(platform->GetInterop()->GetFramebuffer());
	}
#else
	platform->SwapBuffers();
#endif

	frameStart = std::chrono::steady_clock::now() - std::chrono::duration_cast<std::chrono::microseconds>(over);
}

void CApp::SyncToNearestBeat() {
	beatCounter = beatDetect->IsNextBeatCloser(beatDetectTime) ? -1 : 0;

	if (beatCounter == -1)
		resyncBeats = true;

	LogDebug("Setting beatCounter to ", beatCounter);
}

void CApp::OnDestroy() {
	shuttingDown = true;

	platform->OnDestroy();

	for (auto &detector : beatDetectors)
		detector.Cancel();

	lightPack.OnDestroy();
	albumArt.OnDestroy();

	albumArt.RemoveColorChangeListener(this);

	controls.OnDestroy();

	renderer->OnDestroy();

	spindle.OnDestroy();

	blurFbo.reset();
	lastFrame.reset();

	// Make sure to free our shader resources
	context.reset();

	menu.OnDestroy();

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

	Logger::OnDestroy();
}

bool CApp::Open(const std::filesystem::path &path, const std::string &extension, bool exclusive, HSTREAM &target, bool force) {
	if (exclusive)
		exclusive = platform->OpenExclusive(path, extension, exclusive, target, force, channelInfo, reinterpret_cast<void*>(this));

	if (!exclusive) {
		controls.GetExclusiveIndicator().SetExclusive(false);
		BASS_StreamFree(streamHandle);
		target = platform->OpenWithFlags(path, extension, BASS_STREAM_PRESCAN);
	}

	// Reset beat counter on each song
	beatCounter = 0;

	return exclusive;
}

void CApp::Stop(BOOL reset) {
	BASS_ChannelStop(streamHandle);

	if (reset == TRUE)
		BASS_StreamFree(streamHandle);

	playing = false;
}

void CApp::StopExclusive() {
	stopWasapiOnNextLoop = true;
}

void CApp::StopExclusive(BOOL reset) {
	platform->StopExclusive(reset == TRUE);

	if (reset == TRUE)
		BASS_StreamFree(streamHandle);

	playing = false;
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
		auto nextHandle = platform->OpenWithFlags(next->path, nextExtension, BASS_STREAM_PRESCAN | BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT);

		BASS_CHANNELINFO nextChannelInfo;
		BASS_ChannelGetInfo(nextHandle, &nextChannelInfo);

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

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	blurFbo->Unbind();
	lastFrame->Bind();

	glClear(GL_COLOR_BUFFER_BIT);

	lastFrame->Unbind();
}

void CApp::LoadFile(std::filesystem::path path, bool fromPlaylist) {
	std::ifstream inFile(path);

	auto extension = path.extension().u8string();
	std::transform(extension.begin(), extension.end(), extension.begin(), tolower);

	auto originalPath = std::filesystem::is_directory(path) ? path : "";

	// If we're still in the same file,
	// just try to get updated tags from
	// the cue sheet and run beat detection
	// on the new song
	if (path == loadedFile && controls.GetPlaylist().GetCue()) {
		controls.LoadFromCue();

		for (auto &detector : beatDetectors)
			detector.Cancel();

		auto stream = platform->OpenWithFlags(path, extension, BASS_STREAM_PRESCAN | BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT);

		// Disassociate the stream from a device,
		// so it doesn't get freed on BASS_Free()
		BASS_ChannelSetDevice(stream, BASS_NODEVICE);

		LoadBeats(
			stream,
			path
		);

		return;
	}

	albumArt.Reset(visColor, fromPlaylist);

	overrideColor = false;

	// If we try to load an image directly,
	// use that as the album art
	if (AlbumArt::IsSupported(extension)) {
		albumArt.Load(path, originalPath, true);
		return;
	}

	if (!fromPlaylist) {
		// If we're not in a playlist, _reset_
		// any current beat detectors.
		for (auto &detector : beatDetectors)
			detector.Reset();

		if (std::filesystem::is_directory(path) || controls.GetPlaylist().IsCue(extension)) {
			if (auto ret = controls.GetPlaylist().OnLoad(
					path,
					extension,
					[this](const std::filesystem::path &path, const std::string &extension, DWORD flags) {
						return platform->OpenWithFlags(path, extension, flags);
					}
				)
			) {
				path = ret->path;
			} else {
				LogError("Could not load playlist ", path);
				return;
			}

			extension = path.extension().u8string();
			std::transform(extension.begin(), extension.end(), extension.begin(), tolower);
		} else {
			controls.GetPlaylist().Clear();
		}

		// Clear the blur FBO when we load a new file / playlist
		if (blur)
			ClearBlurFbo();
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

	if (fileLoaded && !controls.GetExclusiveIndicator().IsExclusive()) {
		Stop();
		
		Open(path, extension, controls.GetExclusiveIndicator().IsExclusive(), streamHandle);

		// Don't reset gain if we're changing songs
		// in a playlist.
		if (!fromPlaylist) {
			resetGain = true;

			renderer->Reset();
			resetGain = dynamicGain.reset;
		}
	}

	// We want this as a local variable, as it's handed off to BeatDetect
	auto streamHandle = platform->OpenWithFlags(path, extension, BASS_STREAM_PRESCAN | BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT);

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

			Open(path, extension, controls.GetExclusiveIndicator().IsExclusive(), this->streamHandle, !fromPlaylist);
		} else if (advanceOnNextLoop) {
			BASS_StreamFree(this->streamHandle);
			this->streamHandle = nextStreamHandle;
			nextStreamHandle = 0;
		}

		// Reset our tags before
		// trying to load new ones
		if (!fromPlaylist)
			controls.ClearTags();

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
		albumArt.Load(
			platform->GetNativePath(path),
			originalPath
		);

		// Once we have our final art,
		// scale it down
		albumArt.Scale();

		// If we have any tags from the cue
		// sheet, load them _after_ everything
		// else
		controls.LoadFromCue();

		if (!platform->StartPlayingExclusive(fromPlaylist, fileLoaded, advanceOnNextLoop)) {
			if (platform->PlayAfterLoad())
				BASS_ChannelPlay(this->streamHandle, false);
		}

		if (platform->PlayAfterLoad())
			playing = true;
		else playing = false;

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

	OnColorChanged(visColor);
	menu.OnColorChanged(visColor);
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
		auto code = BASS_ErrorGetCode();
		LogError("Seek failed! Error code ", code);
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
		auto code = BASS_ErrorGetCode();
		LogError("Seek failed! Error code ", code);
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

void CApp::TogglePlaying() {
	if (streamHandle) {
		// If we hit the end of the song, we might be in an error state
		if (auto pos = BASS_ChannelGetPosition(streamHandle, BASS_POS_BYTE);
			pos == -1 || BASS_ChannelBytes2Seconds(streamHandle, pos) >= controls.GetCurrentFileLength())
			SeekTo(0.0);

		if (!platform->StopPlayingExclusive()) {
			if (BASS_ChannelIsActive(streamHandle) != BASS_ACTIVE_PLAYING) {
				BASS_ChannelPlay(streamHandle, FALSE);
				playing = true;
			} else {
				BASS_ChannelPause(streamHandle);
				playing = false;
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

	Open(loadedFile, loadedFileExtension, controls.GetExclusiveIndicator().IsExclusive(), streamHandle);

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

		playing = true;
	}
}

void CApp::OnMouseClicked(const Vector2i &mousePos) {
	// Playlist::OnMouseClicked automatically advances
	// our playlist to the clicked position, so we have
	// to figure out what our next track is _before_
	// that happens.
	const auto next = controls.GetPlaylist().GetNext();

	if (auto file = controls.GetPlaylist().OnMouseClicked(mousePos)) {
		// If we didn't select the _next_ song in the playlist,
		// we need to reset beat detection.
		if (!next || file->path != next->path || next->title != file->title)
			ResetBeatDetection();

		LoadFile(file->path, true);

		SeekTo(file->startTime);
	} else if (!platform->OnMouseClicked(mousePos) && albumArt.OnMouseClicked(mousePos)) {
		TogglePlaying();
		if (!playing) controls.GetPause().Fade(true);
		else controls.GetPlay().Fade(true);
	} else {
		if (mousePos.x > windowWidth / 2 && doubleClick.OnClick({ windowWidth / 2, 0 })) {
			NextTrack();
			controls.GetNext().Fade(true);
		} else if (mousePos.x < windowWidth / 2 && doubleClick.OnClick({ 0, 0 })) {
			PreviousTrack();
			controls.GetPrevious().Fade(true);
		}
	}
}

bool CApp::OnMouseDown(const Vector2i &mousePos) {
	return SeekToMousePos(mousePos);
}

void CApp::OnMouseDragged(const Vector2i &mousePos) {
	SeekToMousePos(mousePos, true);
}

void CApp::LoadPreset(std::optional<std::size_t> index) {
	if (const auto &presets = Preset::GetPresets(); index && presets.size() > *index) {
		const auto &preset = presets.at(*index);

		LoadPreset(preset);
	} else index = std::nullopt;

	presetIndex = index;

	Settings::settings.SetPresetIndex(index);
}

void CApp::LoadPreset(const Preset &preset) {
	LogDebug("Loading preset '", preset.GetName(), "'");

	if (preset.GetName() == "Random") {
		presetIndex = std::nullopt;
		Settings::settings.SetPresetIndex(presetIndex);
	}

	if (auto renderer = preset.GetRenderer())
		LoadRenderer(*renderer);

	if (auto scale = preset.GetScale())
		SetVisualizerScale(*scale);

	SetBufferLength(preset.GetBufferSize());
	SetDecayTime(preset.GetDecayTime());
	SetFadeTime(preset.GetFadeTime());
	renderer->SetPulse(preset.GetPulse());
	pulseBackground = preset.GetPulseBackground();
	context->With("blur"_hash, [this](Context::Shader &shader) {
		shader.program.Uniform1i("premultipliedAlpha"_hash, platform->IsAlphaPremultiplied());
	});
	Settings::settings.SetPulseBackground(pulseBackground);
	renderer->SetPulseTime(preset.GetPulseTime());

	if (const auto &rendererOffset = preset.GetRendererOffset()) {
		renderer->SetOffset(*rendererOffset);
		Settings::settings.SetRendererOffset(*rendererOffset);
	} else {
		renderer->SetOffset(0);
		Settings::settings.SetRendererOffset(0);
	}

	SetStrobe(preset.GetStrobe());
	strobeIntensity = preset.GetStrobeIntensity();
	Settings::settings.SetStrobeIntensity(strobeIntensity);

	auto rotating = preset.GetRotating();

	if (rotating)
		SetRotating(*rotating);

	if (rotating && *rotating) {
		SetRotationSpeed(preset.GetRotationSpeed());
	} else {
		frameCount = 0; // Reset rotation
		SetRotationSpeed(6.0f /* default */);
	}

	auto blur = preset.GetBlur();
	SetBlur(blur && *blur);

	// Make sure we don't blow out any existing colors
	if ((blur && *blur) && (Settings::IsColorBlend(sourceFactor) != Settings::IsColorBlend(preset.GetSourceFactor()) ||
		Settings::IsColorBlend(destFactor) != Settings::IsColorBlend(preset.GetDestFactor())))
		ClearBlurFbo();

	sourceFactor = preset.GetSourceFactor();
	Settings::settings.SetSourceFactor(sourceFactor);
	destFactor = preset.GetDestFactor();
	Settings::settings.SetDestFactor(destFactor);
	if (blur && *blur) {
		SetBlurIntensity(preset.GetBlurIntensity());

		Settings::settings.SetBlurOpacity(preset.GetBlurOpacity());
		blurOpacity = preset.GetBlurOpacity();
	}

	Settings::settings.SetEffectIntensity(preset.GetEffectIntensity());
	Settings::settings.SetEffectXOffset(preset.GetEffectXOffset());
	Settings::settings.SetEffectYOffset(preset.GetEffectYOffset());
	Settings::settings.SetEffectRadiation(preset.GetEffectRadiation());
	Settings::settings.SetEffectHorizontalSpread(preset.GetEffectHorizontalSpread());
	Settings::settings.SetEffectVerticalSpread(preset.GetEffectVerticalSpread());
	Settings::settings.SetEffectRotation(preset.GetEffectRotation());

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