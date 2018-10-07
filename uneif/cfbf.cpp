#include "cfbf.h"
#include <algorithm>
#include <fstream>
#include <iostream>

namespace uneif {

namespace impl {

template <class IOS>
class [[nodiscard]] IOStreamExceptionGuard {
public:
	IOStreamExceptionGuard() = delete;
	IOStreamExceptionGuard(const IOStreamExceptionGuard&) = delete;
	IOStreamExceptionGuard& operator=(const IOStreamExceptionGuard&) = delete;
	IOStreamExceptionGuard(IOStreamExceptionGuard&&) = delete;
	IOStreamExceptionGuard& operator=(IOStreamExceptionGuard&&) = delete;

	explicit IOStreamExceptionGuard(IOS& stream, std::ios_base::iostate except = std::ios_base::badbit | std::ios_base::failbit)
		: stream(stream), oldExcept(stream.exceptions())
	{
		stream.exceptions(except);
	}

	~IOStreamExceptionGuard() {
		try {
			stream.exceptions(oldExcept);
		} catch (...) {}
	}

private:
	IOS& stream;
	std::ios_base::iostate oldExcept;
};

}

EifFile::EifFile(std::istream& fin) : fin(fin) {
	impl::IOStreamExceptionGuard iosGuard(fin);
	try {
		// Read header
		fin.read(reinterpret_cast<char*>(&header), SectSize);
		if (!checkHeader(header)) {
			throw EifFormatError {};
		}

		// Read DIFAT
		dif.reserve(FatEntriesInHeader + FatEntriesPerSect * header.difSectNum);
		dif.insert(dif.end(), std::begin(header.fatSects), std::end(header.fatSects));
		if (header.difSectNum > 0) {
			for (;;) {
				size_t curr = dif.size();
				dif.resize(curr + FatEntriesPerSect);
				uint32_t next;
				fin.read(reinterpret_cast<char*>(dif.data() + curr), SectSize - sizeof(next));
				fin.read(reinterpret_cast<char*>(&next), sizeof(next));
				if (next == SectId::EndOfChain) {
					break;
				}
				fin.seekg((next + 1) * SectSize);
			}
		}

		// Read FAT
		fat.resize(header.fatSectNum * SectSize / sizeof(uint32_t));
		for (uint32_t i = 0; i < header.fatSectNum; i++) {
			fin.seekg((dif.at(i) + 1) * SectSize);
			fin.read(reinterpret_cast<char*>(fat.data() + (i * SectSize / sizeof(uint32_t))), SectSize);
		}

		// Read mini FAT
		miniFat.reserve(header.miniFatSectNum * SectSize / sizeof(uint32_t));
		for (uint32_t i = header.miniFatBeginSect; i != SectId::EndOfChain; i = fat.at(i)) {
			size_t curr = miniFat.size();
			miniFat.resize(curr + SectSize / sizeof(uint32_t));
			fin.seekg((i + 1) * SectSize);
			fin.read(reinterpret_cast<char*>(miniFat.data() + curr), SectSize);
		}

		// Read dir entries
		dirs.reserve(header.dirSectNum * SectSize / sizeof(DirEntry));
		for (uint32_t i = header.dirBeginSect; i != SectId::EndOfChain; i = fat.at(i)) {
			size_t curr = dirs.size();
			dirs.resize(curr + SectSize / sizeof(DirEntry));
			fin.seekg((i + 1) * SectSize);
			fin.read(reinterpret_cast<char*>(dirs.data() + curr), SectSize);
		}

		// Read mini stream
		if (dirs.empty() || dirs.front().objType != ObjType::RootStorage) {
			throw EifFormatError {};
		}
		miniStream.reserve(dirs.front().streamSize);
		for (uint32_t i = dirs.front().startSect; i != SectId::EndOfChain; i = fat.at(i)) {
			size_t curr = miniStream.size();
			miniStream.resize(curr + SectSize);
			fin.seekg((i + 1) * SectSize);
			fin.read(miniStream.data() + curr, SectSize);
		}
	} catch (std::ios_base::failure&) {
		if (fin.bad()) {
			throw;
		} else {
			throw EifFormatError {};
		}
	}
}

bool EifFile::checkHeader(const Header& header) {
	const Header defaultHeader;
	if (memcmp(header.magic, defaultHeader.magic, sizeof(header.magic)) != 0) {
		return false;
	}
	if (header.bom != defaultHeader.bom) {
		return false;
	}
	if (header.log2SectorSize != defaultHeader.log2SectorSize) {
		return false;
	}
	if (header.log2MiniSectorSize != defaultHeader.log2MiniSectorSize) {
		return false;
	}
	return true;
}

void EifFile::unpack(const fs::path& outDir) {
	unpackNode(0, outDir);
}

void EifFile::unpackNode(uint32_t sid, const fs::path& outDir) {
	if (sid == StreamID::NoStream) {
		return;
	}
	const DirEntry& node = dirs.at(sid);
	const fs::path curPath = (node.objType == ObjType::RootStorage ? outDir : outDir / node.name);

	switch (node.objType) {
	case ObjType::RootStorage:
	case ObjType::Storage:
		fs::create_directory(curPath);
		unpackNode(node.childId, curPath);
		break;
	case ObjType::Stream:
		unpackFile(node, curPath);
		break;
	default:
		break;
	}

	unpackNode(node.leftSiblingId, outDir);
	unpackNode(node.rightSiblingId, outDir);
}

void EifFile::unpackFile(const DirEntry& node, const fs::path& outPath) {
	std::ofstream fout(outPath, std::ios_base::binary);
	if (!fout) {
		throw IOError("Failed to create file " + outPath.u8string());
	}
	char buf[SectSize];
	uint64_t remaining = node.streamSize;
	if (node.streamSize < header.miniSectorCutoff) {
		for (uint32_t i = node.startSect; remaining > 0; i = miniFat.at(i)) {
			if (i == SectId::EndOfChain) {
				throw EifFormatError {};
			}
			uint64_t readSize = std::min<uint64_t>(remaining, MiniSectSize);
			if (i * MiniSectSize + readSize > miniStream.size()) {
				throw EifFormatError {};
			}
			fout.write(miniStream.data() + i * MiniSectSize, readSize);
			remaining -= readSize;
		}
	} else {
		for (uint32_t i = node.startSect; remaining > 0; i = fat.at(i)) {
			if (i == SectId::EndOfChain) {
				throw EifFormatError {};
			}
			uint64_t readSize = std::min<uint64_t>(remaining, SectSize);
			fin.seekg((i + 1) * SectSize);
			fin.read(buf, readSize);
			if (!fin) {
				throw EifFormatError {};
			}
			fout.write(buf, readSize);
			remaining -= readSize;
		}
	}
	if (!fout) {
		throw IOError("Failed to write file " + outPath.u8string());
	}
}

}
