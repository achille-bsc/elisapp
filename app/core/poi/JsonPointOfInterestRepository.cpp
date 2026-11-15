#include "core/poi/JsonPointOfInterestRepository.hpp"

#include "core/json/TextFile.hpp"
#include "core/poi/PointOfInterestJsonParser.hpp"

namespace mars {

	bool JsonPointOfInterestRepository::load(const char* path)
	{
		points_.clear();
		if (!json::readTextFile(path, kMaxFileBytes, text_) || !PointOfInterestJsonParser::parse(text_.data(), text_.data() + text_.size(), points_)) {
			points_.clear();
			text_.clear();
			return false;
		}
		points_.shrink_to_fit();
		return true;
	}

}
