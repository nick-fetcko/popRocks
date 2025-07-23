#include "CApp.h"

#include "FFTLineRenderer.hpp"
#include "OscilloscopeRenderer.hpp"
#include "FFTRenderer.hpp"

void CApp::AddCommands() {
	// Command template:
	/*
	{
		L"command", [&](const std::vector<std::wstring> &args) {
			// args[0] is the command itself
			// args[1..n - 1] are the actual arguments

			// actions
		}
	},
	*/

	// Give console our commands
	CConsole::Console.AddCommands({
		{
			L"load", [&](const std::vector<std::wstring> &args) {
				auto temp = args[1];

				for (std::wstring::iterator iter = temp.begin(); iter != temp.end(); iter++) {
					if (*iter == L'\"')
						iter = temp.erase(iter);

					if (iter == temp.end())
						break;
				}
				LoadFile(temp);
			}
		},
		{
			L"play", [&](const std::vector<std::wstring> &args) {
				PlayPreparedFile();
			}
		},
		{
			L"fftline", [&](const std::vector<std::wstring> &args) {
				auto oldRenderer = renderer;

				if (oldRenderer) {
					if (auto lineRenderer = dynamic_cast<LineRenderer *>(oldRenderer))
						renderer = new FFTLineRenderer(std::move(*lineRenderer));
					else
						renderer = new FFTLineRenderer(std::move(*oldRenderer));
				} else {
					renderer = new FFTLineRenderer(&dynamicGain, &albumArt);
					renderer->OnInit(windowWidth, windowHeight);
					renderer->SetBuffer(buffer, maxLength);
					renderer->SetBufferLength(bufferLength, true);
				}

				delete oldRenderer;
			}
		},
		{
			L"fft", [&](const std::vector<std::wstring> &args) {
				if (args.size() == 1) {
					auto oldRenderer = renderer;
					if (oldRenderer) {
						renderer = new FFTRenderer(std::move(*oldRenderer));
					} else {
						renderer = new FFTRenderer(&dynamicGain, &albumArt);
						renderer->OnInit(windowWidth, windowHeight);
						renderer->SetBuffer(buffer, maxLength);
						renderer->SetBufferLength(bufferLength, true);
					}

					// Presets only really affect the FFT renderer for now
					LoadPreset(presetIndex);

					delete oldRenderer;
				} else {
					try {
						SetFftLength(std::stoi(args[1]));
					} catch (std::exception &e) {
						CConsole::Console.Print(std::string("Could not set FFT length: ") + e.what(), MSG_ERROR);
					}
				}
			}
		},
		{
			L"osc", [&](const std::vector<std::wstring> &args) {
				auto oldRenderer = renderer;

				if (oldRenderer) {
					if (auto lineRenderer = dynamic_cast<LineRenderer *>(oldRenderer))
						renderer = new OscilloscopeRenderer(std::move(*lineRenderer));
					else
						renderer = new OscilloscopeRenderer(std::move(*oldRenderer));
				} else {
					renderer = new OscilloscopeRenderer(&dynamicGain, &albumArt);
					renderer->OnInit(windowWidth, windowHeight);
					renderer->SetBuffer(buffer, maxLength);
					renderer->SetBufferLength(bufferLength, true);
				}

				delete oldRenderer;
			}
		},
		{
			L"color", [&](const std::vector<std::wstring> &args) {
				if (args.size() >= 4) {
					try {
						SetColor(
							std::stoi(args[1]),
							std::stoi(args[2]),
							std::stoi(args[3])
						);
					} catch (std::exception &e) {
						CConsole::Console.Print(std::string("Could not set color: ") + e.what(), MSG_ERROR);
					}
				} else {
					overrideColor = false;
				}
			}
		},
		{
			L"light intensity", [&](const std::vector<std::wstring> &args) {
				lightPack.SetLightType(LightPack::LightType::Intensity);
				CConsole::Console.Print("Set to light intensity", MSG_DIAG);
			}
		},
		{
			L"light color intensity", [&](const std::vector<std::wstring> &args) {
				lightPack.SetLightType(LightPack::LightType::ColorIntensity);
				CConsole::Console.Print("Setting to light color intensity", MSG_DIAG);
			}
		},
		{
			L"light color", [&](const std::vector<std::wstring> &args) {
				lightPack.SetLightType(LightPack::LightType::Color);
				CConsole::Console.Print("Set to light color", MSG_DIAG);
			}
		},
		{
			L"mapping default", [&](const std::vector<std::wstring> &args) {
				lightPack.SetMapping(Mappings::DEFAULT);
				CConsole::Console.Print("Set to default", MSG_DIAG);
			}
		},
		{
			L"mapping mine", [&](const std::vector<std::wstring> &args) {
				lightPack.SetMapping(Mappings::MINE);
				CConsole::Console.Print("Set to mine", MSG_DIAG);
			}
		},
		{
			L"mapping btt", [&](const std::vector<std::wstring> &args) {
				lightPack.SetMapping(Mappings::BOTTOM_TO_TOP);
				CConsole::Console.Print("Set to bottom to top", MSG_DIAG);
			}
		},
		{
			L"mapping ttb", [&](const std::vector<std::wstring> &args) {
				lightPack.SetMapping(Mappings::TOP_TO_BOTTOM);
				CConsole::Console.Print("Set to top to bottom", MSG_DIAG);
			}
		},
		{
			L"FOCUS_AREA_SUBBASS", [&](const std::vector<std::wstring> &args) {
				lightPack.SetFocusArea(
					LightPack::FocusArea::SubBass
				);
				CConsole::Console.Print("Setting focus area to subbass", MSG_DIAG);
			}
		},
		{
			L"FOCUS_AREA_BASS_AND_MID", [&](const std::vector<std::wstring> &args) {
				lightPack.SetFocusArea(
					LightPack::FocusArea::BassAndMid
				);
				CConsole::Console.Print("Setting focus area to bass and mid", MSG_DIAG);
			}
		},
		{
			L"FOCUS_AREA_HALF_NYQUIST", [&](const std::vector<std::wstring> &args) {
				lightPack.SetFocusArea(
					LightPack::FocusArea::HalfNyquist
				);
				CConsole::Console.Print("Setting focus area to half Nyquist", MSG_DIAG);
			}
		},
		{
			L"FOCUS_AREA_NYQUIST", [&](const std::vector<std::wstring> &args) {
				lightPack.SetFocusArea(
					LightPack::FocusArea::Nyquist
				);
				CConsole::Console.Print("Setting focus area to Nyquist", MSG_DIAG);
			}
		},
		{
			L"FOCUS_AREA_SUPER_BASS", [&](const std::vector<std::wstring> &args) {
				lightPack.SetFocusArea(
					LightPack::FocusArea::SuperBass
				);
				CConsole::Console.Print("Setting focus area to SUPER bass", MSG_DIAG);
			}
		},
		{
			L"FOCUS_AREA_BASS_MID_AND_A_LITTLE_HIGH_END", [&](const std::vector<std::wstring> &args) {
				lightPack.SetFocusArea(
					LightPack::FocusArea::BassMidAndHigh
				);
				CConsole::Console.Print("Setting focus area to bass, mid, and a little high end", MSG_DIAG);
			}
		},
		{
			L"FOCUS_AREA_BASS", [&](const std::vector<std::wstring> &args) {
				lightPack.SetFocusArea(
					LightPack::FocusArea::Bass
				);
				CConsole::Console.Print("Setting focus area to bass", MSG_DIAG);
			}
		},
		{
			L"listen", [&](const std::vector<std::wstring> &args) {
				Listen();
				CConsole::Console.Print("Now listening to primary recording device", MSG_DIAG);
			}
		},
		{
			L"x", [&](const std::vector<std::wstring> &args) {
				if (auto fftRenderer = dynamic_cast<FFTRenderer*>(renderer))
					fftRenderer->ToggleXRot();
			}
		},
		{
			L"y", [&](const std::vector<std::wstring> &args) {
				if (auto fftRenderer = dynamic_cast<FFTRenderer *>(renderer))
					fftRenderer->ToggleYRot();
			}
		},
		{
			L"z", [&](const std::vector<std::wstring> &args) {
				if (auto fftRenderer = dynamic_cast<FFTRenderer *>(renderer))
					fftRenderer->ToggleZRot();
			}
		},
		{
			L"dist", [&](const std::vector<std::wstring> &args) {
				if (args.size() > 1) {
					try {
						if (auto fftRenderer = dynamic_cast<FFTRenderer *>(renderer))
							fftRenderer->SetDistribution(std::stof(args[1]));
					} catch (std::exception &e) {
						CConsole::Console.Print(std::string("Could not set distribution: ") + e.what(), MSG_ERROR);
					}
				}
			}
		},
		{
			L"buffer", [&](const std::vector<std::wstring> &args) {
				if (args.size() > 1) {
					try {
						SetBufferLength(std::stoi(args[1]));
					} catch (std::exception &e) {
						CConsole::Console.Print(std::string("Could not set buffer length: ") + e.what(), MSG_ERROR);
					}
				}
			}
		},
		{
			L"rot", [&](const std::vector<std::wstring> &args) {
				if (args.size() > 1) {
					try {
						SetRotationSpeed(std::stof(args[1]));
					} catch (std::exception &e) {
						CConsole::Console.Print(std::string("Could not set rotation speed: ") + e.what(), MSG_ERROR);
					}
				} else {
					SetRotating(!GetRotating());
				}
			}
		},
		{
			L"decay", [&](const std::vector<std::wstring> &args) {
				if (args.size() > 1) {
					try {
						SetDecayTime(
							std::chrono::duration<double> {
								std::stod(args[1])
							}
						);
					} catch (std::exception &e) {
						CConsole::Console.Print(std::string("Could not set decay: ") + e.what(), MSG_ERROR);
					}
				}
			}
		},
		{
			L"fade", [&](const std::vector<std::wstring> &args) {
				if (args.size() > 1) {
					try {
						SetFadeTime(
							std::chrono::duration<double> {
								std::stod(args[1])
							}
						);
					} catch (std::exception &e) {
						CConsole::Console.Print(std::string("Could not set fade: ") + e.what(), MSG_ERROR);
					}
				}
			}
		},
		{
			L"gain", [&](const std::vector<std::wstring> &args) {
				if (args.size() > 1) {
					try {
						SetGain(
							std::stof(args[1])
						);
					} catch (std::exception &e) {
						CConsole::Console.Print(std::string("Could not set gain: ") + e.what(), MSG_ERROR);
					}
				}
			}
		},
		{
			L"tri", [&](const std::vector<std::wstring> &args) {
				if (auto fftRenderer = dynamic_cast<FFTRenderer *>(renderer))
					fftRenderer->SetIndexBuffer(&Buffer::TriangleBuffer);
			}
		},
		{
			L"squ", [&](const std::vector<std::wstring> &args) {
				if (auto fftRenderer = dynamic_cast<FFTRenderer *>(renderer))
					fftRenderer->SetIndexBuffer(&Buffer::SquareBuffer);
			}
		},
		{
			L"strobe", [&](const std::vector<std::wstring> &args) {
				if (args.size() > 1) {
					try {
						SetStrobeFrequency(
							std::chrono::duration<double> {
								std::stod(args[1])
							}
						);
					} catch (std::exception &e) {
						CConsole::Console.Print(std::string("Could not set strobe frequency: ") + e.what(), MSG_ERROR);
					}
				} else {
					SetStrobe(!GetStrobe());
				}
			}
		},
		{
			L"rpm", [&](const std::vector<std::wstring> &args) {
				if (args.size() > 1) {
					try {
						SetRotationSpeed(std::stof(args[1]) * (360.0f / 60.0f) /* 6 */);
					} catch (std::exception &e) {
						CConsole::Console.Print(std::string("Could not set RPM: ") + e.what(), MSG_ERROR);
					}
				}
			}
		},
		{
			L"smooth", [&](const std::vector<std::wstring> &args) {
				if (args.size() > 1) {
					try {
						uint8_t smooth = std::clamp(std::stoi(args[1]), 0, 255);
						lightPack.SetSmooth(smooth);
					} catch (std::exception &e) {
						CConsole::Console.Print(std::string("Could not set smooth: ") + e.what(), MSG_ERROR);
					}
				}
			}
		},
		{
			L"gamma", [&](const std::vector<std::wstring> &args) {
				if (args.size() > 1) {
					try {
						lightPack.SetGamma(std::stof(args[1]));
					} catch (std::exception &e) {
						CConsole::Console.Print(std::string("Could not set gamma: ") + e.what(), MSG_ERROR);
					}
				}
			}
		},
		{
			L"blur", [&](const std::vector<std::wstring> &args) {
				if (args.size() > 1) {
					try {
						SetBlurIntensity(std::stof(args[1]));
					} catch (std::exception &e) {
						CConsole::Console.Print(std::string("Could not set blur factor: ") + e.what(), MSG_ERROR);
					}
				} else {
					ToggleBlur();
				}
			}
		},
		{
			L"radius", [&](const std::vector<std::wstring> &args) {
				if (args.size() > 1) {
					try {
						auto radius = std::stof(args[1]);

						albumArt.SetRadius(radius);
						albumArt.Scale(true);
						controls.GetVolume().SetRadius(radius);
					} catch (std::exception &e) {
						CConsole::Console.Print(std::string("Could not set radius: ") + e.what(), MSG_ERROR);
					}
				}
			}
		},
		{
			L"bpm", [&](const std::vector<std::wstring> &args) {
				for (auto &detector : beatDetectors)
					detector.ToggleDetection();

				if (beatDetect->IsDetecting() && !loadedFile.empty()) {
					for (auto &detector : beatDetectors)
						detector.Cancel();

					LoadBeats(
						OpenWithFlags(loadedFile, loadedFileExtension, BASS_STREAM_PRESCAN | BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT),
						loadedFile
					);
				}
			}
		},
		{
			L"width", [&](const std::vector<std::wstring> &args) {
				if (args.size() > 1) {
					try {
						if (auto lineRenderer = dynamic_cast<LineRenderer*>(renderer))
							lineRenderer->SetWidth(std::stof(args[1]));
					}
					catch (std::exception &e) {
						CConsole::Console.Print(std::string("Could not set width: ") + e.what(), MSG_ERROR);
					}
				}
			}
		},
		{
			L"rgb", [&](const std::vector<std::wstring> &args) {
				lightPack.SetMethod(LightPack::Method::RGB);
			}
		},
		{
			L"hsv", [&](const std::vector<std::wstring> &args) {
				lightPack.SetMethod(LightPack::Method::HSV);
			}
		},
		{
			L"sat", [&](const std::vector<std::wstring> &args) {
				if (args.size() > 1) {
					try {
						lightPack.SetSaturationMultiplier(std::stof(args[1]));
					} catch (std::exception &e) {
						CConsole::Console.Print(std::string("Could not set saturation multiplier: ") + e.what(), MSG_ERROR);
					}
				}
			}
		},
		{
			L"pulse", [&](const std::vector<std::wstring> &args) {
				if (args.size() == 1) {
					renderer->TogglePulse();
				} else {
					try {
						renderer->SetPulseTime(
							std::chrono::duration<double> {
								std::stod(args[1])
							}
						);
					} catch (std::exception &e) {
						CConsole::Console.Print(std::string("Could not set pulse time: ") + e.what(), MSG_ERROR);
					}
				}
			}
		}
	});
}