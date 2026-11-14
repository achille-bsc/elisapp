#pragma once

#include <cstdint>

namespace mars::json {

	/// Minimal pull parser for JSON text (RFC 8259 / ECMA-404).
	///
	/// Works in place on a mutable buffer and never allocates: strings are decoded
	/// where they stand and null-terminated, so the pointers it returns stay valid
	/// for as long as the buffer does. After the first error every call returns false.
	class Reader {
	public:
		/// `end` must be a writable position (one past the text, e.g. a std::string's
		/// terminator) because the buffer is modified while reading.
		Reader(char* begin, char* end) noexcept : cursor_{begin}, end_{end} {}

		/// Consumes '{'. Then call nextKey() until it returns false.
		[[nodiscard]] bool beginObject() noexcept;
		/// Reads the next member name and its ':'. False at the closing '}' or on error.
		[[nodiscard]] bool nextKey(const char*& key) noexcept;

		/// Consumes '['. Then call nextElement() until it returns false.
		[[nodiscard]] bool beginArray() noexcept;
		/// True if another element follows. False at the closing ']' or on error.
		[[nodiscard]] bool nextElement() noexcept;

		[[nodiscard]] bool readString(const char*& text) noexcept;
		[[nodiscard]] bool readNumber(double& value) noexcept;
		/// Skips any value, including nested objects and arrays.
		[[nodiscard]] bool skipValue() noexcept;

		/// True once the whole text has been consumed without error.
		[[nodiscard]] bool atEnd() noexcept;
		[[nodiscard]] bool failed() const noexcept { return failed_; }

	private:
		static constexpr std::uint8_t kMaxDepth = 32;

		void skipWhitespace() noexcept;
		[[nodiscard]] bool consume(char expected) noexcept;
		[[nodiscard]] bool open(char bracket) noexcept;
		/// Handles the separator before a member or element. `more` is false at `close`.
		[[nodiscard]] bool advance(char close, bool& more) noexcept;
		[[nodiscard]] bool skipLiteral(const char* literal) noexcept;
		[[nodiscard]] bool readHex4(std::uint32_t& value) noexcept;
		bool fail() noexcept;

		char* cursor_;
		char* end_;
		std::uint32_t hasElement_{0};  ///< One bit per nesting level.
		std::uint8_t depth_{0};
		bool failed_{false};
	};

}
