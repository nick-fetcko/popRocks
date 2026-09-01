#include "MP4.hpp"

#include <cstring>

#include "ID3V2.hpp"

MP4::Atom::Atom(std::ifstream *file) : file(file) {

}

MP4::Atom::Atom(Atom &&other) noexcept : file(std::move(other.file)) {
	size = std::move(other.size);
	name = std::move(other.name);
	version = std::move(other.version);
	memcpy(&flags, &other.flags, sizeof(flags));
	memset(&other.flags, 0, sizeof(flags));
	dataSize = std::move(other.dataSize);
	data = std::move(other.data);
	mimeType = std::move(other.mimeType);
}

MP4::Atom::~Atom() {
}

void MP4::Atom::Read() {
	char name[4];
	file->read(reinterpret_cast<char *>(&size), sizeof(size));
	// Convert from Big Endian to Little Endian
	ID3V2::Fix32Bit(&size, false);
	file->read(name, sizeof(name));
	this->name = std::string(name, name + sizeof(name));

	std::transform(this->name.begin(), this->name.end(), this->name.begin(), tolower);

	bytes = sizeof(size) + sizeof(name);
}

constexpr int32_t MP4::Atom::GetExtrasSize() {
	return sizeof(uint8_t) + sizeof(flags);
}

void MP4::Atom::ReadExtras() {
	file->read(reinterpret_cast<char *>(&version), sizeof(uint8_t));
	file->read(reinterpret_cast<char *>(flags), sizeof(flags));

	bytes += GetExtrasSize();
}

bool MP4::Atom::ReadData(bool textOnly) {
	bool ret = false;

	// FIXME: Make this test more robust
	if (name != "data") return ret;

	ReadExtras();

	// There are currently 4 reserved
	// bytes in "data" atoms
	file->seekg(4, std::ios::cur);

	switch (flags[2]) {
	case 0:
		mimeType = "application/octet-stream";
		break;
	case 1:
		mimeType = "text/plain";
		break;
	case 13:
		mimeType = "image/jpeg";
		if (textOnly) return ret;
		ret = true;
		break;
	case 14:
		mimeType = "image/png";
		if (textOnly) return ret;
		ret = true;
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

	data.resize(dataSize);
	file->read(reinterpret_cast<char *>(data.data()), dataSize);

	return ret;
}

bool MP4::Atom::IsValid() const {
	constexpr auto isValid = [](unsigned char c /* implicit cast to unsigned */) {
		// iTunes tags can also sometimes
		// just be 4 dashes
		// ...or contain spaces (e.g. "xid ")
		return !std::isalpha(c) && c != '-' && c != ' ';
	};

	if (std::find_if(
		// iTunes tags can begin with '�' (copyright sign)
		// which is 0xA9 in Windows-1252
		*name.begin() == static_cast<char>(0xA9) ? name.begin() + 1 : name.begin(),
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
	Atom ret(&file);

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

std::map<std::string, std::string> MP4::GetTags(bool textOnly) {
	std::map<std::string, std::string> ret;

	auto atom = GetAtomAtPath({ "moov", "udta", "meta", "ilst" });

	int64_t size = atom->size;
	auto pos = file.tellg();
	
	while (file.tellg() < pos + static_cast<std::streampos>(size)) {
		atom->Read();
		if (!atom->IsValid()) {
			file.seekg(-(atom->GetBytes() * 2), std::ios::cur);

			atom->Read();
			atom->ReadExtras();
		}

		if (auto iter = RelevantAtoms.find(atom->name); iter != RelevantAtoms.end()) {
			// "data" atom comes next
			atom->Read();
			if (atom->ReadData(textOnly)) {
				artAtom.emplace(std::move(*atom));
			} else if (atom->flags[2] == 1) { // text/plain
				ret[iter->second] = std::string(atom->data.begin(), atom->data.end());
			} else if (atom->flags[2] == 0) { // application/octet-stream
				for (uint32_t i = 0; i < atom->dataSize; ++i) {
					if (atom->data[i] != 0) {
						ret[iter->second] = std::to_string(static_cast<uint16_t>(atom->data[i]));

						// We only care about the _first_ non-zero for now.
						// Just getting "disc 1" is fine... we don't need
						// "disc 1 of x"
						break;
					}
				}
			}
		} else file.seekg(atom->size - atom->GetBytes(), std::ios::cur);
	}

	return ret;
}