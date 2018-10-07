#pragma once

#include <cstdint>
#include <iosfwd>
#include <filesystem>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace uneif {

namespace fs = std::filesystem;

constexpr int SectSize = 512;
constexpr int DirEntrySize = 128;
constexpr int MiniSectSize = 64;
constexpr int FatEntriesInHeader = 109;
constexpr int FatEntriesPerSect = 127;

template <class Enum, class = std::enable_if_t<std::is_enum_v<Enum>>>
constexpr bool operator==(Enum a, std::underlying_type_t<Enum> b) {
	return static_cast<std::underlying_type_t<Enum>>(a) == b;
}

template <class Enum, class = std::enable_if_t<std::is_enum_v<Enum>>>
constexpr bool operator==(std::underlying_type_t<Enum> a, Enum b) { return b == a; }

template <class Enum, class = std::enable_if_t<std::is_enum_v<Enum>>>
constexpr bool operator!=(Enum a, std::underlying_type_t<Enum> b) { return !(a == b); }

template <class Enum, class = std::enable_if_t<std::is_enum_v<Enum>>>
constexpr bool operator!=(std::underlying_type_t<Enum> a, Enum b) { return !(a == b); }

using Clsid = std::uint8_t[16];

enum class SectId : std::uint32_t {
	Dif = 0xfffffffc,
	Fat = 0xfffffffd,
	EndOfChain = 0xfffffffe,
	Free = 0xffffffff,
};

struct Header {
	unsigned char magic[8] = {0xd0, 0xcf, 0x11, 0xe0, 0xa1, 0xb1, 0x1a, 0xe1};
	Clsid clsid = {0};
	std::uint16_t minorVersion = 33;
	std::uint16_t dllVersion = 3;
	std::uint16_t bom = 0xfffe;
	std::uint16_t log2SectorSize = 9;
	std::uint16_t log2MiniSectorSize = 6;
	std::uint16_t reserved0 = 0;
	std::uint32_t reserved1 = 0;
	std::uint32_t dirSectNum = 0;
	std::uint32_t fatSectNum = 0;
	std::uint32_t dirBeginSect = 0;
	std::uint32_t signature = 0;
	std::uint32_t miniSectorCutoff = 4096;
	std::uint32_t miniFatBeginSect = 0;
	std::uint32_t miniFatSectNum = 0;
	std::uint32_t difBeginSect = 0;
	std::uint32_t difSectNum = 0;
	std::uint32_t fatSects[FatEntriesInHeader];
};
static_assert(sizeof(Header) == SectSize);

enum class ObjType : std::uint8_t {
	Unknown = 0x00,
	Storage = 0x01,
	Stream = 0x02,
	RootStorage = 0x05,
};

enum class ColorFlag : std::uint8_t {
	Red = 0x00,
	Black = 0x01,
};

enum class StreamID : std::uint32_t {
	MaxRegSid = 0xfffffffa,
	NoStream = 0xffffffff,
};

using uint64_align32_t = std::uint32_t[2];

struct DirEntry {
	char16_t name[32];
	std::uint16_t nameLen;
	ObjType objType;
	ColorFlag colorFlag;
	std::uint32_t leftSiblingId;
	std::uint32_t rightSiblingId;
	std::uint32_t childId;
	Clsid clsid;
	std::uint32_t state;
	uint64_align32_t creationTime;
	uint64_align32_t modifiedTime;
	std::uint32_t startSect;
	std::uint64_t streamSize;
};
static_assert(sizeof(DirEntry) == DirEntrySize);


class EifFile {
public:
	explicit EifFile(std::istream& fin);
	void unpack(const fs::path& outDir);

private:
	static bool checkHeader(const Header& header);
	void unpackNode(std::uint32_t sid, const fs::path& outDir);
	void unpackFile(const DirEntry& node, const fs::path& outPath);

	std::istream& fin;
	Header header;
	std::vector<std::uint32_t> dif;
	std::vector<std::uint32_t> fat;
	std::vector<std::uint32_t> miniFat;
	std::vector<DirEntry> dirs;
	std::vector<char> miniStream;
};


class EifFormatError : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
	EifFormatError() : std::runtime_error("Invalid .eif file format") {}
};

class IOError : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

}
