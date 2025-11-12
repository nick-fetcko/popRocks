#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "Serial/Node.hpp"
#include "Serial/Xml.hpp"

#include "Utils/Logger.hpp"
#include "Utils/Utils.hpp"

using namespace serial;
using namespace Fetcko;

class JPEG : public LoggableClass {
private:
	static inline constexpr std::string_view XMPNS = "http://ns.adobe.com/xap/1.0/";

	static inline const std::vector<std::string> SequencePath = {
		"x:xmpmeta",
		"rdf:RDF",
		"rdf:Description",
		"xmpTPg:PlateNames",
		"rdf:Seq",
		"rdf:li"
	};

	static inline const std::vector<std::string> ModePath = {
		"x:xmpmeta",
		"rdf:RDF",
		"rdf:Description",
		"@photoshop:ColorMode"
	};

public:
	JPEG(const std::filesystem::path &path) {
		inFile.open(path, std::ios::in | std::ios::binary);
	}

	std::string GetColorMode() {
		std::string ret;

		// Make sure we start with SOI
		uint16_t soi;
		inFile.read(reinterpret_cast<char *>(&soi), sizeof(uint16_t));

		if (soi == 0xD8FF) {
			//LogDebug("Image started with SOI!");

			uint8_t marker;
			inFile.read(reinterpret_cast<char *>(&marker), sizeof(uint8_t));

			while (marker == 0xFF && inFile) {
				uint8_t markerNumber;
				inFile.read(reinterpret_cast<char *>(&markerNumber), sizeof(uint8_t));

				//LogDebug("Found marker ", std::hex, static_cast<int>(markerNumber));

				uint16_t dataSize;
				inFile.read(reinterpret_cast<char *>(&dataSize), sizeof(uint16_t));

				// "dataSize" includes the 2 size bytes
				dataSize = Utils::LittleEndian(dataSize) - sizeof(uint16_t);

				char *data = new char[dataSize];
				inFile.read(data, dataSize);

				// Are we XMP?
				if (strstr(data, XMPNS.data())) {
					auto string = std::string(data + XMPNS.size() + 1, dataSize - (XMPNS.size() + 1));

					Node xml;
					xml.parseString<Xml>(string);

					Node *view = &xml;

					// Do we have a color mode?
					for (const auto &prop : ModePath) {
						if (view->has(prop)) {
							view = view->mutableProperty(prop);
						} else {
							view = &xml;
							break;
						}
					}

					if (view != &xml) {
						/* 
							https://developer.adobe.com/xmp/docs/XMPNamespaces/photoshop/

							0 = Bitmap
							1 = Gray scale
							2 = Indexed colour
							3 = RGB colour
							4 = CMYK colour
							7 = Multi-channel
							8 = Duotone
							9 = LAB colour
						*/
						switch (view->get<int>()) {
						case 3:
							ret = "RGB";
							break;
						case 4:
							ret = "CMYK";
							break;
						default:
							break;
						}
					} else {
						// Do we have a color sequence?
						for (const auto &prop : SequencePath) {
							if (view->has(prop)) {
								view = view->mutableProperty(prop);
							} else {
								view = &xml;
								break;
							}
						}

						if (view != &xml) {
							for (const auto &property : view->properties()) {
								auto str = property.second.get<std::string>();
								std::transform(str.begin(), str.end(), str.begin(), toupper);
								if (str == "BLACK")
									ret += "K";
								else
									ret += (*str.begin());
							}
						}
					}
				}

				delete[] data;

				if (ret.size()) return ret;

				inFile.read(reinterpret_cast<char *>(&marker), sizeof(uint8_t));
			}
		}

		return ret;
	}

private:
	std::ifstream inFile;
};