#include "CApp.h"

#include "FFTLineRenderer.hpp"
#include "OscilloscopeRenderer.hpp"
#include "FFTRenderer.hpp"

void CApp::AddCommands() {
	// Command template:
	/*
	{
		"command", [&](const std::vector<std::string> &args) {
			// args[0] is the command itself
			// args[1..n - 1] are the actual arguments

			// actions
		}
	},
	*/

	constexpr auto IsDefault = [](std::string string) {
		std::transform(string.begin(), string.end(), string.begin(), tolower);
		return string == "default";
	};

	// Give console our commands
	Logger::AddCommands({
		{
			"load", [&](const std::vector<std::string> &args) {
				auto temp = args[1];

				for (std::string::iterator iter = temp.begin(); iter != temp.end(); iter++) {
					if (*iter == L'\"')
						iter = temp.erase(iter);

					if (iter == temp.end())
						break;
				}
				LoadFile(temp);
			}
		},
		{
			"play", [&](const std::vector<std::string> &args) {
				PlayPreparedFile();
			}
		},
		{
			"fftline", [&](const std::vector<std::string> &args) {
				renderer = RendererFactory::Build(
					args[0],
					&dynamicGain,
					&albumArt,
					renderer,
					windowWidth,
					windowHeight,
					buffer,
					maxLength,
					bufferLength
				);

				Settings::settings.SetRenderer(args[0]);
			}
		},
		{
			"fft", [&](const std::vector<std::string> &args) {
				if (args.size() == 1) {
					renderer = RendererFactory::Build(
						args[0],
						&dynamicGain,
						&albumArt,
						renderer,
						windowWidth,
						windowHeight,
						buffer,
						maxLength,
						bufferLength
					);

					Settings::settings.SetRenderer(args[0]);

					// Presets only really affect the FFT renderer for now
					LoadPreset(presetIndex);
				} else {
					try {
						if (IsDefault(args[1]))
							SetFftLength(8192);
						else
							SetFftLength(std::stoi(args[1]));
					} catch (std::exception &e) {
						logger.LogError("Could not set FFT length: ", e.what());
					}
				}
			}
		},
		{
			"osc", [&](const std::vector<std::string> &args) {
				renderer = RendererFactory::Build(
					args[0],
					&dynamicGain,
					&albumArt,
					renderer,
					windowWidth,
					windowHeight,
					buffer,
					maxLength,
					bufferLength
				);

				Settings::settings.SetRenderer(args[0]);
			}
		},
		{
			"color", [&](const std::vector<std::string> &args) {
				if (args.size() >= 4) {
					try {
						SetColor(
							std::stoi(args[1]),
							std::stoi(args[2]),
							std::stoi(args[3])
						);
					} catch (std::exception &e) {
						logger.LogError("Could not set color: ", e.what());
					}
				} else {
					overrideColor = false;
				}
			}
		},
		{
			"light intensity", [&](const std::vector<std::string> &args) {
				lightPack.SetLightType(LightPack::LightType::Intensity);
				logger.LogDebug("Set to light intensity");
			}
		},
		{
			"light color intensity", [&](const std::vector<std::string> &args) {
				lightPack.SetLightType(LightPack::LightType::ColorIntensity);
				logger.LogDebug("Setting to light color intensity");
			}
		},
		{
			"light color", [&](const std::vector<std::string> &args) {
				lightPack.SetLightType(LightPack::LightType::Color);
				logger.LogDebug("Set to light color");
			}
		},
		{
			"mapping default", [&](const std::vector<std::string> &args) {
				lightPack.SetMapping(Mappings::DEFAULT);
				logger.LogDebug("Set to default");
			}
		},
		{
			"mapping mine", [&](const std::vector<std::string> &args) {
				lightPack.SetMapping(Mappings::MINE);
				logger.LogDebug("Set to mine");
			}
		},
		{
			"mapping btt", [&](const std::vector<std::string> &args) {
				lightPack.SetMapping(Mappings::BOTTOM_TO_TOP);
				logger.LogDebug("Set to bottom to top");
			}
		},
		{
			"mapping ttb", [&](const std::vector<std::string> &args) {
				lightPack.SetMapping(Mappings::TOP_TO_BOTTOM);
				logger.LogDebug("Set to top to bottom");
			}
		},
		{
			"FOCUS_AREA_SUBBASS", [&](const std::vector<std::string> &args) {
				lightPack.SetFocusArea(
					LightPack::FocusArea::SubBass
				);
				logger.LogDebug("Setting focus area to subbass");
			}
		},
		{
			"FOCUS_AREA_BASS_AND_MID", [&](const std::vector<std::string> &args) {
				lightPack.SetFocusArea(
					LightPack::FocusArea::BassAndMid
				);
				logger.LogDebug("Setting focus area to bass and mid");
			}
		},
		{
			"FOCUS_AREA_HALF_NYQUIST", [&](const std::vector<std::string> &args) {
				lightPack.SetFocusArea(
					LightPack::FocusArea::HalfNyquist
				);
				logger.LogDebug("Setting focus area to half Nyquist");
			}
		},
		{
			"FOCUS_AREA_NYQUIST", [&](const std::vector<std::string> &args) {
				lightPack.SetFocusArea(
					LightPack::FocusArea::Nyquist
				);
				logger.LogDebug("Setting focus area to Nyquist");
			}
		},
		{
			"FOCUS_AREA_SUPER_BASS", [&](const std::vector<std::string> &args) {
				lightPack.SetFocusArea(
					LightPack::FocusArea::SuperBass
				);
				logger.LogDebug("Setting focus area to SUPER bass");
			}
		},
		{
			"FOCUS_AREA_BASS_MID_AND_A_LITTLE_HIGH_END", [&](const std::vector<std::string> &args) {
				lightPack.SetFocusArea(
					LightPack::FocusArea::BassMidAndHigh
				);
				logger.LogDebug("Setting focus area to bass, mid, and a little high end");
			}
		},
		{
			"FOCUS_AREA_BASS", [&](const std::vector<std::string> &args) {
				lightPack.SetFocusArea(
					LightPack::FocusArea::Bass
				);
				logger.LogDebug("Setting focus area to bass");
			}
		},
		{
			"listen", [&](const std::vector<std::string> &args) {
				Listen();
				logger.LogDebug("Now listening to primary recording device");
			}
		},
		{
			"loopback", [&](const std::vector<std::string> &args) {
				Listen(true);
				logger.LogDebug("Now listening to primary output device");
			}
		},
		{
			"x", [&](const std::vector<std::string> &args) {
				if (auto fftRenderer = dynamic_cast<FFTRenderer*>(renderer))
					fftRenderer->ToggleXRot();
			}
		},
		{
			"y", [&](const std::vector<std::string> &args) {
				if (auto fftRenderer = dynamic_cast<FFTRenderer *>(renderer))
					fftRenderer->ToggleYRot();
			}
		},
		{
			"z", [&](const std::vector<std::string> &args) {
				if (auto fftRenderer = dynamic_cast<FFTRenderer *>(renderer))
					fftRenderer->ToggleZRot();
			}
		},
		{
			"dist", [&](const std::vector<std::string> &args) {
				if (args.size() > 1) {
					try {
						if (auto fftRenderer = dynamic_cast<FFTRenderer *>(renderer)) {
							if (IsDefault(args[1]))
								fftRenderer->SetDistribution(360.0f);
							else
								fftRenderer->SetDistribution(std::stof(args[1]));
						}
					} catch (std::exception &e) {
						logger.LogError("Could not set distribution: ", e.what());
					}
				}
			}
		},
		{
			"buffer", [&](const std::vector<std::string> &args) {
				if (args.size() > 1) {
					try {
						if (IsDefault(args[1]))
							SetBufferLength(2048);
						else
							SetBufferLength(std::stoi(args[1]));

						// We deviated from a preset
						LoadPreset(std::nullopt);
					} catch (std::exception &e) {
						logger.LogError("Could not set buffer length: ", e.what());
					}
				}
			}
		},
		{
			"rot", [&](const std::vector<std::string> &args) {
				if (args.size() > 1) {
					try {
						if (IsDefault(args[1]))
							SetRotationSpeed(6.0f);
						else
							SetRotationSpeed(std::stof(args[1]));

						// We deviated from a preset
						LoadPreset(std::nullopt);
					} catch (std::exception &e) {
						logger.LogError("Could not set rotation speed: ", e.what());
					}
				} else {
					SetRotating(!GetRotating());

					// We deviated from a preset
					LoadPreset(std::nullopt);
				}
			}
		},
		{
			"decay", [&](const std::vector<std::string> &args) {
				if (args.size() > 1) {
					try {
						if (IsDefault(args[1]))
							SetDecayTime(0.5s);
						else
							SetDecayTime(
								std::chrono::duration<double> {
									std::stod(args[1])
								}
							);

						// We deviated from a preset
						LoadPreset(std::nullopt);
					} catch (std::exception &e) {
						logger.LogError("Could not set decay: ", e.what());
					}
				}
			}
		},
		{
			"fade", [&](const std::vector<std::string> &args) {
				if (args.size() > 1) {
					try {
						if (IsDefault(args[1]))
							SetFadeTime(0.5s);
						else
							SetFadeTime(
								std::chrono::duration<double> {
									std::stod(args[1])
								}
							);

						// We deviated from a preset
						LoadPreset(std::nullopt);
					} catch (std::exception &e) {
						logger.LogError("Could not set fade: ", e.what());
					}
				}
			}
		},
		{
			"gain", [&](const std::vector<std::string> &args) {
				if (args.size() > 1) {
					try {
						if (IsDefault(args[1]))
							SetGain(20.0f);
						else
							SetGain(
								std::stof(args[1])
							);
					} catch (std::exception &e) {
						logger.LogError("Could not set gain: ", e.what());
					}
				}
			}
		},
		{
			"tri", [&](const std::vector<std::string> &args) {
				if (auto fftRenderer = dynamic_cast<FFTRenderer *>(renderer))
					fftRenderer->SetIndexBuffer(&Buffers::TriangleBuffer);
			}
		},
		{
			"squ", [&](const std::vector<std::string> &args) {
				if (auto fftRenderer = dynamic_cast<FFTRenderer *>(renderer))
					fftRenderer->SetIndexBuffer(&Buffers::SquareBuffer);
			}
		},
		{
			"strobe", [&](const std::vector<std::string> &args) {
				if (args.size() > 1) {
					try {
						if (IsDefault(args[1]))
							SetStrobeFrequency(1s);
						else
							SetStrobeFrequency(
								std::chrono::duration<double> {
									std::stod(args[1])
								}
							);
					} catch (std::exception &e) {
						logger.LogError("Could not set strobe frequency: ", e.what());
					}
				} else {
					SetStrobe(!GetStrobe());
				}
			}
		},
		{
			"rpm", [&](const std::vector<std::string> &args) {
				if (args.size() > 1) {
					try {
						if (IsDefault(args[1]))
							SetRotationSpeed(6.0f); // 1 RPM
						else
							SetRotationSpeed(std::stof(args[1]) * (360.0f / 60.0f) /* 6 */);

						// We deviated from a preset
						LoadPreset(std::nullopt);
					} catch (std::exception &e) {
						logger.LogError("Could not set RPM: ", e.what());
					}
				}
			}
		},
		{
			"smooth", [&](const std::vector<std::string> &args) {
				if (args.size() > 1) {
					try {
						if (IsDefault(args[1])) {
							lightPack.SetSmooth(0);
						} else {
							uint8_t smooth = std::clamp(std::stoi(args[1]), 0, 255);
							lightPack.SetSmooth(smooth);
						}
					} catch (std::exception &e) {
						logger.LogError("Could not set smooth: ", e.what());
					}
				}
			}
		},
		{
			"gamma", [&](const std::vector<std::string> &args) {
				if (args.size() > 1) {
					try {
						if (IsDefault(args[1]))
							lightPack.SetGamma(1.0f);
						else
							lightPack.SetGamma(std::stof(args[1]));
					} catch (std::exception &e) {
						logger.LogError("Could not set gamma: ", e.what());
					}
				}
			}
		},
		{
			"blur", [&](const std::vector<std::string> &args) {
				if (args.size() > 1) {
					try {
						if (IsDefault(args[1]))
							SetBlurIntensity(0.88f);
						else
							SetBlurIntensity(std::stof(args[1]));

						// We deviated from a preset
						LoadPreset(std::nullopt);
					} catch (std::exception &e) {
						logger.LogError("Could not set blur factor: ", e.what());
					}
				} else {
					ToggleBlur();

					// We deviated from a preset
					LoadPreset(std::nullopt);
				}
			}
		},
		{
			"radius", [&](const std::vector<std::string> &args) {
				if (args.size() > 1) {
					try {
						auto radius = IsDefault(args[1]) ? 200.0f : std::stof(args[1]);

						albumArt.SetRadius(radius);
						albumArt.Scale(true);
						controls.GetVolume().SetRadius(radius);
					} catch (std::exception &e) {
						logger.LogError("Could not set radius: ", e.what());
					}
				}
			}
		},
		{
			"bpm", [&](const std::vector<std::string> &args) {
				for (auto &detector : beatDetectors)
					detector.SetDetecting(!detector.IsDetecting());

				Settings::settings.SetDetectBpm(beatDetect->IsDetecting());

				if (beatDetect->IsDetecting() && !loadedFile.empty()) {
					for (auto &detector : beatDetectors)
						detector.Cancel();

					LoadBeats(
						OpenWithFlags(loadedFile, loadedFileExtension, BASS_STREAM_PRESCAN | BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT),
						loadedFile,
						false // don't ping-pong when we toggle
					);
				}
			}
		},
		{
			"width", [&](const std::vector<std::string> &args) {
				if (args.size() > 1) {
					try {
						if (auto lineRenderer = dynamic_cast<LineRenderer *>(renderer)) {
							if (IsDefault(args[1]))
								lineRenderer->SetWidth(4.0f);
							else
								lineRenderer->SetWidth(std::stof(args[1]));
						}
					}
					catch (std::exception &e) {
						logger.LogError("Could not set width: ", e.what());
					}
				}
			}
		},
		{
			"rgb", [&](const std::vector<std::string> &args) {
				lightPack.SetMethod(LightPack::Method::RGB);
			}
		},
		{
			"hsv", [&](const std::vector<std::string> &args) {
				lightPack.SetMethod(LightPack::Method::HSV);
			}
		},
		{
			"sat", [&](const std::vector<std::string> &args) {
				if (args.size() > 1) {
					try {
						if (IsDefault(args[1]))
							lightPack.SetSaturationMultiplier(1.25f);
						else
							lightPack.SetSaturationMultiplier(std::stof(args[1]));
					} catch (std::exception &e) {
						logger.LogError("Could not set saturation multiplier: ", e.what());
					}
				}
			}
		},
		{
			"pulse", [&](const std::vector<std::string> &args) {
				if (args.size() == 1) {
					renderer->TogglePulse();

					// We deviated from a preset
					LoadPreset(std::nullopt);
				} else {
					try {
						if (IsDefault(args[1]))
							renderer->SetPulseTime(0.1s);
						else
							renderer->SetPulseTime(
								std::chrono::duration<double> {
									std::stod(args[1])
								}
							);

						// We deviated from a preset
						LoadPreset(std::nullopt);
					} catch (std::exception &e) {
						logger.LogError("Could not set pulse time: ", e.what());
					}
				}
			}
		},
		{
			"resetwindow", [&](const std::vector<std::string> &args) {
				Settings::settings.SetWindowWidth(1920);
				Settings::settings.SetWindowHeight(1080);
				Settings::settings.SetWindowX(SDL_WINDOWPOS_CENTERED);
				Settings::settings.SetWindowY(SDL_WINDOWPOS_CENTERED);

				SDL_SetWindowSize(sdlWindow, Settings::settings.GetWindowWidth(), Settings::settings.GetWindowHeight());
				SDL_SetWindowPosition(sdlWindow, Settings::settings.GetWindowX(), Settings::settings.GetWindowY());
			}
		}
	});
}