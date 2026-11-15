#include "ui/controls/LayerSwitcher.hpp"

#include <cstddef>

#include "ui/Theme.hpp"
#include "ui/Widgets.hpp"

namespace mars::ui {

	namespace {

		constexpr std::int32_t kSegmentPadding = 22;

		/// Segment titles, indexed by MapLayer.
		constexpr std::array<const char*, kMapLayerCount> kTitles{
			LV_SYMBOL_IMAGE "  Terrain", LV_SYMBOL_SHUFFLE "  Weather", LV_SYMBOL_WARNING "  Risk", LV_SYMBOL_CHARGE "  Radiation"};

	}

	LayerSwitcher::LayerSwitcher(lv_obj_t* parent, ILayerListener& listener)
		: listener_{listener}
		, root_{widgets::box(parent, &theme::styles().pill)}
	{
		const theme::Styles& styles = theme::styles();

		lv_obj_set_size(root_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
		widgets::flex(root_, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_START, 0);

		for (std::size_t index = 0; index < segments_.size(); ++index) {
			lv_obj_t* const segment = widgets::button(root_, &styles.pill, onSegment, this);
			lv_obj_add_style(segment, &styles.checked, LV_STATE_CHECKED);
			lv_obj_set_size(segment, LV_SIZE_CONTENT, theme::kTouchTarget);
			lv_obj_set_style_pad_hor(segment, kSegmentPadding, LV_PART_MAIN);
			widgets::flex(segment, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_CENTER, 0);
			widgets::label(segment, kTitles[index]);
			segments_[index] = segment;
		}
		setSelected(MapLayer::Terrain);
	}

	void LayerSwitcher::setVisible(bool visible)
	{
		widgets::setVisible(root_, visible);
	}

	void LayerSwitcher::setOffset(std::int32_t offset)
	{
		lv_obj_set_style_translate_x(root_, offset, LV_PART_MAIN);
	}

	bool LayerSwitcher::isVisible() const
	{
		return !lv_obj_is_hidden(root_);
	}

	void LayerSwitcher::setSelected(MapLayer layer)
	{
		for (std::size_t index = 0; index < segments_.size(); ++index) {
			lv_obj_set_checked(segments_[index], index == static_cast<std::size_t>(layer));
		}
	}

	void LayerSwitcher::onSegment(lv_event_t* event)
	{
		auto* const self = static_cast<LayerSwitcher*>(lv_event_get_user_data(event));
		const lv_obj_t* const pressed = lv_event_get_current_target_obj(event);
		for (std::size_t index = 0; index < self->segments_.size(); ++index) {
			if (self->segments_[index] == pressed) {
				const auto layer = static_cast<MapLayer>(index);
				self->setSelected(layer);
				self->listener_.onLayerSelected(layer);
				return;
			}
		}
	}

}
