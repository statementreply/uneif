#include "cfbf.h"
#include <fstream>
#include <iostream>

using namespace std;
namespace fs = std::filesystem;

int u8main(int argc, char* argv[]) {
	try {
		if (argc < 2) {
			cout << "Usage: " << argv[0] << " FILE\n";
			return EXIT_FAILURE;
		}
		for (int i = 1; i < argc; i++) {
			fs::path inFile = fs::u8path(argv[i]);
			fs::path outDir = inFile;
			outDir.replace_extension();

			ifstream fin(inFile, ios_base::binary);
			if (!fin) {
				cerr << "Failed to open file " << argv[i] << '\n';
			}
			uneif::EifFile eif(fin);
			eif.unpack(outDir);
		}
	} catch (const exception& e) {
		cerr << e.what() << '\n';
		return EXIT_FAILURE;
	}
	return 0;
}


#if _WIN32

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>
#include <string_view>
#include <vector>

static string u16_to_u8(wstring_view sv) {
	int bufSize = WideCharToMultiByte(CP_UTF8, 0, sv.data(), sv.size(), nullptr, 0, nullptr, nullptr);
	if (bufSize <= 0) {
		throw runtime_error("WideCharToMultiByte failed");
	}
	string ret(bufSize, '\0');
	if (0 == WideCharToMultiByte(CP_UTF8, 0, sv.data(), sv.size(), ret.data(), bufSize, nullptr, nullptr)) {
		throw runtime_error("WideCharToMultiByte failed");
	}
	return ret;
}

int wmain(int argc, wchar_t* argv[]) {
	vector<string> u8args;
	vector<char*> u8argv;
	u8args.reserve(argc);
	u8argv.reserve(argc + 1);
	for (int i = 0; i < argc; i++) {
		u8args.push_back(u16_to_u8(argv[i]));
		u8argv.push_back(u8args.back().data());
	}
	u8argv.push_back(nullptr);
	return u8main(argc, u8argv.data());
}

#else

int main(int argc, char* argv[]) {
	return u8main(argc, argv);
}

#endif
