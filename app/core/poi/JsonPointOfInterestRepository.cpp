#include "core/poi/JsonPointOfInterestRepository.hpp"

#include <cstddef>
#include <cstdio>
#include <memory>

#include "core/poi/PointOfInterestJsonParser.hpp"

namespace mars {

	bool JsonPointOfInterestRepository::readFile(const char* path)
	{
		const auto close = [](std::FILE* file) { std::fclose(file); };
		const std::unique_ptr<std::FILE, decltype(close)> file{std::fopen(path, "rb"), close};
		if (!file || std::fseek(file.get(), 0, SEEK_END) != 0) {
			return false;
		}
		const long size = std::ftell(file.get());
		if (size <= 0 || size > kMaxFileBytes || std::fseek(file.get(), 0, SEEK_SET) != 0) {
			return false;
		}
		text_.resize(static_cast<std::size_t>(size));
		return std::fread(text_.data(), 1, text_.size(), file.get()) == text_.size();
	}

	bool JsonPointOfInterestRepository::load(const char* path)
	{
		points_.clear();
		if (!readFile(path) || !PointOfInterestJsonParser::parse(text_.data(), text_.data() + text_.size(), points_)) {
			points_.clear();
			text_.clear();
			return false;
		}
		points_.shrink_to_fit();
		return true;
	}

}
