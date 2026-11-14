#pragma once

#include <string>

namespace mars::json {

	/// Reads the whole file at `path` into `text`. False (and `text` empty) if
	/// it is missing, empty or larger than `maxBytes`.
	[[nodiscard]] bool readTextFile(const char* path, long maxBytes, std::string& text);

}
