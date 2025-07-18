#include "ID3V2.hpp"

#include "CConsole.h"
#include "Utils.hpp"

// ===============================================
// ================ ID3V2::Header ================
// ===============================================
ID3V2::Header::Header(ID3V2 &parent) : parent(parent) {

}

ID3V2::ExtendedHeader *ID3V2::Header::Read(const char **tag) {
	auto *start = *tag;

	memcpy(id, *tag, sizeof(id));
	*tag += sizeof(id);

	majorVersion = **tag;
	*tag += sizeof(majorVersion);

	parent.frameIdBytes = parent.frameSizeBytes = (majorVersion == 2 ? 3 : 4);

	revisionNumber = **tag;
	*tag += sizeof(revisionNumber);

	flags = **tag;
	*tag += sizeof(flags);

	size = *reinterpret_cast<const uint32_t *>(*tag);
	Fix32Bit(&size);
	*tag += sizeof(size);

	parent.currentOffset += (*tag - start);

	return HasExtendedHeader() ? new ExtendedHeader() : nullptr;
}

bool ID3V2::Header::HasExtendedHeader() const {
	auto ret = flags & 0x40;
	if (ret) throw std::runtime_error("extended headers not yet supported");
	return ret;
}

// ===============================================
// ================= ID3V2::Art ==================
// ===============================================
ID3V2::Art::Art(ID3V2::Frame &parent) : parent(parent) {

}

ID3V2::Art::~Art() {
	delete[] data;
}

bool ID3V2::Art::Read(const char **tag) {
	auto *start = *tag;

	parent.ReadEncoding(tag, &textEncoding);

	// MIME types were added in v3
	if (parent.parent.header.majorVersion > 2) {
		// This one's null terminated, so it can
		// implicitly be converted to a std::string
		mimeType = *tag;
		*tag += mimeType.size() + 1;
	} else {
		// We'll convert the 3-letter v2 format
		// to a MIME type
		mimeType = "image/";

		if ((*tag)[0] == 'P' && (*tag)[1] == 'N' && (*tag)[2] == 'G')
			mimeType += "png";
		else if ((*tag)[0] == 'J' && (*tag)[1] == 'P' && (*tag)[2] == 'G')
			mimeType += "jpeg";

		*tag += 3;
	}

	type = static_cast<Type>(**tag);
	*tag += sizeof(type);

	// This one's also null terminated
	description = *tag;

	// Even when the encoding is Latin, there's
	// sometimes a double terminator
	while ((*tag)[0] == '\0')
		*tag += 1;

	auto offset = static_cast<uint32_t>(*tag - start);
	dataLength = parent.size - offset;

	data = new uint8_t[dataLength];
	memcpy(data, *tag, dataLength);
	
	parent.size -= offset;
	parent.parent.currentOffset += offset;

	return true;
}

// ===============================================
// ================ ID3V2::Frame =================
// ===============================================
ID3V2::Frame::Frame(ID3V2 &parent) : parent(parent) {

}

ID3V2::Frame::Frame(Frame &&other) noexcept : parent(std::move(other.parent)) {
	id = std::move(other.id);
	size = std::move(other.size);
	flags = std::move(other.flags);
	data = std::move(other.data);
	other.data = nullptr;
	textData = std::move(other.textData);
	artData = std::move(other.artData);
	other.artData = nullptr; // null out to prevent deletion on destruction
	encoding = std::move(other.encoding);
}

ID3V2::Frame::~Frame() {
	delete[] data;
	delete artData;
}

bool ID3V2::Frame::Read(const char **tag, bool textOnly) {
	bool ret = false;

	auto *start = *tag;

	uint8_t idChars[4];
	memcpy(idChars, *tag, parent.frameIdBytes);
	*tag += parent.frameIdBytes;

	id = std::string(idChars, idChars + parent.frameIdBytes);

	size = *reinterpret_cast<const uint32_t *>(*tag);

	// Fix BEFORE possible shift
	Fix32Bit(&size, parent.header.majorVersion >= 4); // no synch bits in versions below 4

	// Versions lower than 3 only have a 3-byte size
	if (parent.header.majorVersion < 3)
		size >>= 8;
	
	*tag += parent.frameSizeBytes;

	// Flags were introduced in version 3
	if (parent.header.majorVersion > 2) {
		flags = *reinterpret_cast<const uint16_t *>(*tag);
		*tag += sizeof(flags);
	}

	// Only allocate data if it's a frame we care about
	if (RelevantFrames.find(id) != RelevantFrames.end()) {
		// Text has an encoding byte
		if (id[0] == 'T') {
			ReadEncoding(tag);

			textData = ToUTF8(*tag, size, encoding);
		} else if (!textOnly && id == "APIC" || id == "PIC") {
			artData = new Art(*this);
			if (!artData->Read(tag)) {
				delete artData;
				artData = nullptr;
			}
		}

		ret = true;
	}

	*tag += size;

	parent.currentOffset += (*tag - start);

	return ret;
}

void ID3V2::Frame::ReadEncoding(const char **tag, Encoding *encoding) {
	if (encoding) {
		*encoding = static_cast<Encoding>(**tag);
	} else {
		this->encoding = static_cast<Encoding>(**tag);
		size -= 1;
	}

	*tag += 1;
}

// ===============================================
// ==================== ID3V2 ====================
// ===============================================
ID3V2::ID3V2() : header(*this) {

}

ID3V2::~ID3V2() {
	delete extendedHeader;
}

std::map<std::string, std::string> ID3V2::Read(const char **tag, bool textOnly) {
	std::map<std::string, std::string> ret;

	extendedHeader = header.Read(tag);
	while (currentOffset < header.size) {
		Frame frame(*this);
		if (frame.Read(tag, textOnly)) {
			auto &[friendlyName, mutator] = Frame::RelevantFrames.at(frame.id);
			ret.emplace(std::make_pair(friendlyName, mutator(frame.textData)));
			frames.emplace(std::make_pair(friendlyName, std::move(frame)));
		} else {
			if (frame.id.empty() || frame.id[0] == '\0') // we hit padding
				break;

			CConsole::Console.Print("Skipping frame " + frame.id, MSG_DIAG);
		}
	}

	return ret;
}

void ID3V2::Fix32Bit(uint32_t *num, bool unsynch) {
	// Big Endian to Little Endian
	*num =
		((*num & 0xFF000000) >> 24) |
		((*num & 0x00FF0000) >> 8) |
		((*num & 0x0000FF00) << 8) |
		((*num & 0x000000FF) << 24);

	if (unsynch) {
		// Mask and shift out the synch bits
		*num =
			((*num & 0x7F000000) >> 3) |
			((*num & 0x007F0000) >> 2) |
			((*num & 0x00007F00) >> 1) |
			((*num & 0x0000007F));
	}
}

std::string ID3V2::ToUTF8(const char *tag, uint32_t size, Encoding encoding) {
	// Size needs to be rounded to the nearest _even_ number,
	// since we divide by 2 later on.
	// 
	// FIXME: A better option might be to use an istringstream
	//        by default and _only_ switch to a wistringstream when
	//        we encounter UTF-16.
	bool rounded = false;
	if (auto round = static_cast<uint32_t>(std::round(std::round(size) / sizeof(wchar_t)) * sizeof(wchar_t));
		round != size) {
		size = round;
		rounded = true;
	}

	std::wistringstream stream(
		std::wstring(
			reinterpret_cast<const wchar_t *>(tag),
			reinterpret_cast<const wchar_t *>(tag + size)
		),
		std::ios::in | std::ios::binary
	);

	// Imbue with UTF-8 to start with
	stream.imbue(
		std::locale(
			stream.getloc(),
			new std::codecvt_utf8<wchar_t>
		)
	);

	if (encoding == Encoding::UTF_16) {
		if (auto bom = Fetcko::Utils::GetBom(stream)) {
			switch (*bom) {
			case Fetcko::Utils::BOM::UTF_16_BE:
				encoding = Encoding::UTF_16_BE;
				stream.imbue(
					std::locale(
						stream.getloc(),
						new std::codecvt_utf16<wchar_t, 0x10ffff>
					)
				);

				// UTF-16 BOMs are two bytes wide
				size -= 2;
				break;
			case Fetcko::Utils::BOM::UTF_16_LE:
				stream.imbue(
					std::locale(
						stream.getloc(),
						new std::codecvt_utf16<wchar_t, 0x10ffff, std::little_endian>
					)
				);

				// UTF-16 BOMs are two bytes wide
				size -= 2;
				break;
			case Fetcko::Utils::BOM::UTF_8:
				// UTF-8 BOM is three bytes wide
				size -= 3;
				[[fallthrough]];
			default:
				encoding = Encoding::UTF_8;
				break;
			}
		}
	}

	size /= sizeof(wchar_t);

	if (encoding == Encoding::Latin || encoding == Encoding::UTF_8) {
		auto *chars = new char[size * sizeof(wchar_t) + (rounded ? 0 : 1)];
		stream.read(reinterpret_cast<wchar_t*>(chars), size);

		// The spec _says_ these should already
		// be null-terminated, but I've found
		// multiple examples of no terminator
		chars[size * sizeof(wchar_t) - (rounded ? 1 : 0)] = '\0';

		auto ret = std::string(chars, chars + size * sizeof(wchar_t));
		delete[] chars;

		return ret;
	} else {
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

		wchar_t *wideChars = new wchar_t[size + 1];
		stream.read(wideChars, size);

		// The spec _says_ these should already
		// be null-terminated, but I've found
		// multiple examples of no terminator
		wideChars[size] = L'\0';

		auto ret = converter.to_bytes(wideChars);
		delete[] wideChars;

		return ret;
	}
}