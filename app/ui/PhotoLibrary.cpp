#include "ui/PhotoLibrary.hpp"

#include <cstring>

namespace mars::ui {

	const void* PhotoLibrary::photo(const PointOfInterest& point)
	{
		if (point.photo == nullptr || point.photo[0] == '\0') {
			return nullptr;
		}
		const auto found = photos_.find(point.photo);
		if (found != photos_.end()) {
			return found->second.get();
		}
		std::uint32_t size = 0;
		const void* const blob = files_.mapFile(point.photo, size);
		Header header{};
		if (blob == nullptr || size < sizeof header) {
			photos_[point.photo] = nullptr;  // not asked again
			return nullptr;
		}
		std::memcpy(&header, blob, sizeof header);
		constexpr std::uint32_t kPixelBytes = 2;
		const std::uint32_t stride = std::uint32_t{header.width} * kPixelBytes;
		if (header.magic != kMagic || header.width == 0 || header.height == 0 || header.width > kMaxSide
			|| header.height > kMaxSide || size != sizeof header + stride * header.height) {
			photos_[point.photo] = nullptr;
			return nullptr;
		}
		auto image = std::make_unique<lv_draw_buf_t>();
		// LVGL only reads image sources
		void* const pixels = const_cast<std::uint8_t*>(static_cast<const std::uint8_t*>(blob) + sizeof header);
		if (lv_draw_buf_init(image.get(), header.width, header.height, LV_COLOR_FORMAT_RGB565, stride, pixels,
				stride * header.height)
			!= LV_RESULT_OK) {
			photos_[point.photo] = nullptr;
			return nullptr;
		}
		const void* const source = image.get();
		photos_[point.photo] = std::move(image);
		return source;
	}

	static_assert(sizeof(PhotoLibrary::kMagic) == 4);

}
