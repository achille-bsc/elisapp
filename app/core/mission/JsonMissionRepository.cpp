#include "core/mission/JsonMissionRepository.hpp"

#include "core/json/TextFile.hpp"
#include "core/mission/MissionJsonParser.hpp"

namespace mars {

	bool JsonMissionRepository::load(const char* path)
	{
		missions_.clear();
		if (!json::readTextFile(path, kMaxFileBytes, text_)
			|| !MissionJsonParser::parse(text_.data(), text_.data() + text_.size(), missions_)) {
			missions_.clear();
			text_.clear();
			return false;
		}
		missions_.shrink_to_fit();
		return true;
	}

}
