#include "core/json/JsonReader.hpp"

#include <charconv>
#include <system_error>

namespace mars::json {

	namespace {

		/// Appends a code point as UTF-8 and returns the new write position.
		char* appendUtf8(char* out, std::uint32_t codePoint) noexcept
		{
			if (codePoint < 0x80) {
				*out++ = static_cast<char>(codePoint);
			} else if (codePoint < 0x800) {
				*out++ = static_cast<char>(0xC0 | (codePoint >> 6));
				*out++ = static_cast<char>(0x80 | (codePoint & 0x3F));
			} else if (codePoint < 0x10000) {
				*out++ = static_cast<char>(0xE0 | (codePoint >> 12));
				*out++ = static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
				*out++ = static_cast<char>(0x80 | (codePoint & 0x3F));
			} else {
				*out++ = static_cast<char>(0xF0 | (codePoint >> 18));
				*out++ = static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
				*out++ = static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
				*out++ = static_cast<char>(0x80 | (codePoint & 0x3F));
			}
			return out;
		}

	}

	bool Reader::fail() noexcept
	{
		failed_ = true;
		return false;
	}

	void Reader::skipWhitespace() noexcept
	{
		while (cursor_ != end_ && (*cursor_ == ' ' || *cursor_ == '\n' || *cursor_ == '\r' || *cursor_ == '\t')) {
			++cursor_;
		}
	}

	bool Reader::consume(char expected) noexcept
	{
		skipWhitespace();
		if (failed_ || cursor_ == end_ || *cursor_ != expected) {
			return fail();
		}
		++cursor_;
		return true;
	}

	bool Reader::open(char bracket) noexcept
	{
		if (depth_ == kMaxDepth || !consume(bracket)) {
			return fail();
		}
		hasElement_ &= ~(std::uint32_t{1} << depth_);
		++depth_;
		return true;
	}

	bool Reader::advance(char close, bool& more) noexcept
	{
		more = false;
		skipWhitespace();
		if (failed_ || depth_ == 0 || cursor_ == end_) {
			return fail();
		}
		const std::uint32_t levelBit = std::uint32_t{1} << (depth_ - 1U);
		if (*cursor_ == close) {
			++cursor_;
			--depth_;
			return true;
		}
		if ((hasElement_ & levelBit) != 0U && !consume(',')) {
			return false;
		}
		hasElement_ |= levelBit;
		skipWhitespace();
		if (cursor_ == end_ || *cursor_ == close) {
			return fail();  // trailing comma
		}
		more = true;
		return true;
	}

	bool Reader::beginObject() noexcept { return open('{'); }

	bool Reader::beginArray() noexcept { return open('['); }

	bool Reader::nextKey(const char*& key) noexcept
	{
		bool more = false;
		return advance('}', more) && more && readString(key) && consume(':');
	}

	bool Reader::nextElement() noexcept
	{
		bool more = false;
		return advance(']', more) && more;
	}

	bool Reader::readHex4(std::uint32_t& value) noexcept
	{
		if (end_ - cursor_ < 4) {
			return fail();
		}
		const auto result = std::from_chars(cursor_, cursor_ + 4, value, 16);
		if (result.ec != std::errc{} || result.ptr != cursor_ + 4) {
			return fail();
		}
		cursor_ += 4;
		return true;
	}

	bool Reader::readString(const char*& text) noexcept
	{
		if (!consume('"')) {
			return false;
		}
		char* out = cursor_;
		text = out;
		while (cursor_ != end_) {
			const char c = *cursor_++;
			if (c == '"') {
				*out = '\0';  // overwrites the closing quote or an already consumed character
				return true;
			}
			if (static_cast<unsigned char>(c) < 0x20) {
				return fail();
			}
			if (c != '\\') {
				*out++ = c;
				continue;
			}
			if (cursor_ == end_) {
				return fail();
			}
			switch (*cursor_++) {
				case '"': *out++ = '"'; break;
				case '\\': *out++ = '\\'; break;
				case '/': *out++ = '/'; break;
				case 'b': *out++ = '\b'; break;
				case 'f': *out++ = '\f'; break;
				case 'n': *out++ = '\n'; break;
				case 'r': *out++ = '\r'; break;
				case 't': *out++ = '\t'; break;
				case 'u': {
					std::uint32_t codePoint = 0;
					if (!readHex4(codePoint)) {
						return false;
					}
					if (codePoint >= 0xD800 && codePoint <= 0xDBFF) {
						std::uint32_t low = 0;
						if (end_ - cursor_ < 2 || cursor_[0] != '\\' || cursor_[1] != 'u') {
							return fail();
						}
						cursor_ += 2;
						if (!readHex4(low) || low < 0xDC00 || low > 0xDFFF) {
							return fail();
						}
						codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
					} else if (codePoint >= 0xDC00 && codePoint <= 0xDFFF) {
						return fail();
					}
					if (codePoint == 0) {
						return fail();  // would truncate the null-terminated result
					}
					out = appendUtf8(out, codePoint);
					break;
				}
				default: return fail();
			}
		}
		return fail();
	}

	bool Reader::readNumber(double& value) noexcept
	{
		skipWhitespace();
		if (failed_ || cursor_ == end_ || (*cursor_ != '-' && (*cursor_ < '0' || *cursor_ > '9'))) {
			return fail();
		}
		const auto result = std::from_chars(cursor_, end_, value);
		if (result.ec != std::errc{}) {
			return fail();
		}
		cursor_ += result.ptr - cursor_;
		return true;
	}

	bool Reader::skipLiteral(const char* literal) noexcept
	{
		for (; *literal != '\0'; ++literal, ++cursor_) {
			if (cursor_ == end_ || *cursor_ != *literal) {
				return fail();
			}
		}
		return true;
	}

	bool Reader::skipValue() noexcept
	{
		skipWhitespace();
		if (failed_ || cursor_ == end_) {
			return fail();
		}
		const char* ignoredText = nullptr;
		double ignoredNumber = 0.0;
		switch (*cursor_) {
			case '{':
				if (!beginObject()) {
					return false;
				}
				while (nextKey(ignoredText)) {
					if (!skipValue()) {
						return false;
					}
				}
				return !failed_;
			case '[':
				if (!beginArray()) {
					return false;
				}
				while (nextElement()) {
					if (!skipValue()) {
						return false;
					}
				}
				return !failed_;
			case '"': return readString(ignoredText);
			case 't': return skipLiteral("true");
			case 'f': return skipLiteral("false");
			case 'n': return skipLiteral("null");
			default: return readNumber(ignoredNumber);
		}
	}

	bool Reader::atEnd() noexcept
	{
		skipWhitespace();
		return !failed_ && depth_ == 0 && cursor_ == end_;
	}

}
