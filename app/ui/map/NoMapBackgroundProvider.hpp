#pragma once

#include "ui/map/IMapBackgroundProvider.hpp"

namespace mars::ui {

	/// Placeholder used until the imagery APIs are connected: never any picture.
	class NoMapBackgroundProvider final : public IMapBackgroundProvider {
	public:
		[[nodiscard]] const void* background(MapLayer, const MapProjection&) override { return nullptr; }
	};

}
