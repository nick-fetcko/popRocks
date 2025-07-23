#include "MP4.hpp"

#include "ID3V2.hpp"

#include "CConsole.h"

MP4::Atom::Atom(std::ifstream &file) : file(file) {

}

MP4::Atom::Atom(Atom &&other) noexcept : file(std::move(other.file)) {
	size = std::move(other.size);
	name = std::move(other.name);
	version = std::move(other.version);
	memcpy(&flags, &other.flags, sizeof(flags));
	memset(&other.flags, 0, sizeof(flags));
	dataSize = std::move(other.dataSize);
	data = std::move(other.data);
	other.data = nullptr; // null out to prevent deletion on destruction
	mimeType = std::move(other.mimeType);
}

MP4::Atom::~Atom() {
	delete[] data;
}

void MP4::Atom::Read() {
	char name[4];
	file.read(reinterpret_cast<char *>(&size), sizeof(size));
	// Convert from Big Endian to Little Endian
	ID3V2::Fix32Bit(&size, false);
	file.read(name, sizeof(name));
	this->name = std::string(name, name + sizeof(name));

	bytes = sizeof(size) + sizeof(name);
}

constexpr int32_t MP4::Atom::GetExtrasSize() {
	return sizeof(uint8_t) + sizeof(flags);
}

void MP4::Atom::ReadExtras() {
	file.read(reinterpret_cast<char *>(&version), sizeof(uint8_t));
	file.read(reinterpret_cast<char *>(flags), sizeof(flags));

	bytes += GetExtrasSize();
}

void MP4::Atom::ReadData() {
	// FIXME: Make this test more robust
	if (name != "data") return;

	ReadExtras();

	// There are currently 4 reserved
	// bytes in "data" atoms
	file.seekg(4, std::ios::cur);

	mimeType = "image/";
	switch (flags[2]) {
	case 13:
		mimeType += "jpeg";
		break;
	case 14:
		mimeType += "png";
		break;
	default:
		break;
	}

	// 16 bytes in total before the data proper:
	//		4 bytes for atom size
	//		4 bytes for atom name
	//		1 byte for version
	//		3 bytes for flag
	//		4 bytes reserved
	dataSize = size - 16;

	data = new uint8_t[dataSize];
	file.read(reinterpret_cast<char *>(data), dataSize);
}

bool MP4::Atom::IsValid() const {
	constexpr auto isValid = [](unsigned char c /* implicit cast to unsigned */) {
		// iTunes tags can also sometimes
		// just be 4 dashes
		// ...or contain spaces (e.g. "xid ")
		return !std::isalpha(c) && c != '-' && c != ' ';
	};

	if (std::find_if(
		// iTunes tags can begin with '©'
		*name.begin() == '©' ? name.begin() + 1 : name.begin(),
		name.end(),
		isValid
	) != name.end())
		return false;

	if (!size)
		return false;

	return true;
}

int32_t MP4::Atom::GetBytes() const {
	return bytes;
}

MP4::MP4(const std::filesystem::path &path) {
	file.open(path, std::ios::in | std::ios::binary);
}

std::optional<MP4::Atom> MP4::GetAtomAtPath(const std::vector<std::string> &path) {
	std::optional<Atom> ret = std::nullopt;

	for (const auto &name : path) {
		auto atom = SeekToAtom(name, ret);
		if (atom) ret.emplace(std::move(*atom));
		else return std::nullopt;
	}

	return ret;
}

std::optional<MP4::Atom> MP4::SeekToAtom(const std::string &name, const std::optional<Atom> &parent) {
	Atom ret(file);

	while (file) {
		ret.Read();

		if (!ret.IsValid()) {
			file.seekg(-(ret.GetBytes() * 2), std::ios::cur);

			ret.Read();
			ret.ReadExtras();

			// It's possible that, after this,
			// we're back to our parent atom.
			//
			// When that happens, we won't
			// seek at all this iteration
			// so that we get our first child
			// on the next.
		}

		if (ret.name == name)
			return ret;
		else if (file && (!parent || (parent && parent->name != ret.name)))
			file.seekg(ret.size - ret.GetBytes(), std::ios::cur);
	}

	return std::nullopt;
}