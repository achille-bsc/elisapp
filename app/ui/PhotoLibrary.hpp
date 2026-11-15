#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include <lvgl.h>

#include "core/storage/IFileMapper.hpp"
#include "ui/panels/PointOfInterestCard.hpp"

namespace mars::ui {

	/// Place photos (data/photos/*.bin, RGB565), mapped on first use.
	class PhotoLibrary final : public IPlacePhotos {
	public:
		static constexpr std::uint32_t kMagic = 0x544F4850;  // "PHOT"
		static constexpr std::uint16_t kMaxSide = 2048;

		/// `files` must outlive the library.
		explicit PhotoLibrary(IFileMapper& files) noexcept : files_{files} {}

		[[nodiscard]] const void* photo(const PointOfInterest& point) override;

	private:
		struct Header {
			std::uint32_t magic;
			std::uint16_t width;
			std::uint16_t height;
		};

		IFileMapper& files_;
		std::map<std::string, std::unique_ptr<lv_draw_buf_t>> photos_;
	};

}
