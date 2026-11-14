#include "core/json/TextFile.hpp"

#include <cstddef>
#include <cstdio>
#include <memory>

namespace mars::json {

	bool readTextFile(const char* path, long maxBytes, std::string& text)
	{
		text.clear();
		const auto close = [](std::FILE* file) { std::fclose(file); };
		const std::unique_ptr<std::FILE, decltype(close)> file{std::fopen(path, "rb"), close};
		if (!file || std::fseek(file.get(), 0, SEEK_END) != 0) {
			return false;
		}
		const long size = std::ftell(file.get());
		if (size <= 0 || size > maxBytes || std::fseek(file.get(), 0, SEEK_SET) != 0) {
			return false;
		}
		text.resize(static_cast<std::size_t>(size));
		if (std::fread(text.data(), 1, text.size(), file.get()) != text.size()) {
			text.clear();
			return false;
		}
		return true;
	}

}
