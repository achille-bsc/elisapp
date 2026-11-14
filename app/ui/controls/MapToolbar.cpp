#include "ui/controls/MapToolbar.hpp"

#include "ui/Theme.hpp"
#include "ui/Widgets.hpp"

namespace mars::ui {

	namespace {

		constexpr std::int32_t kSearchWidth = 584;
		constexpr std::int32_t kSearchPadding = 26;
		constexpr std::int32_t kButtonGap = 14;

		lv_obj_t* makeRoundButton(lv_obj_t* parent, const char* symbol, lv_event_cb_t onClick, void* context)
		{
			lv_obj_t* const button = widgets::button(parent, &theme::styles().pill, onClick, context);
			lv_obj_set_size(button, theme::kTouchTarget, theme::kTouchTarget);
			lv_obj_center(widgets::label(button, symbol));
			return button;
		}

	}

	MapToolbar::MapToolbar(lv_obj_t* parent, IToolbarListener& listener)
		: listener_{listener}
		, root_{widgets::box(parent)}
	{
		const theme::Styles& styles = theme::styles();

		lv_obj_set_size(root_, lv_pct(100), LV_SIZE_CONTENT);
		widgets::flex(root_, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_CENTER, kButtonGap);

		layers_ = makeRoundButton(root_, LV_SYMBOL_LIST, onLayers, this);

		search_ = widgets::button(root_, &styles.pill, onSearch, this);
		lv_obj_set_size(search_, kSearchWidth, theme::kTouchTarget);
		lv_obj_set_style_pad_hor(search_, kSearchPadding, LV_PART_MAIN);
		searchLabel_ = widgets::label(search_, "");
		lv_obj_add_style(searchLabel_, &styles.hint, LV_PART_MAIN);
		lv_obj_align(searchLabel_, LV_ALIGN_LEFT_MID, 0, 0);

		recenter_ = makeRoundButton(root_, LV_SYMBOL_GPS, onRecenter, this);
	}

	void MapToolbar::setCompact(bool compact)
	{
		widgets::setVisible(search_, !compact);
		widgets::setVisible(recenter_, !compact);
		lv_obj_set_style_flex_main_place(root_, compact ? LV_FLEX_ALIGN_START : LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
	}

	std::int32_t MapToolbar::layersButtonOffset() const
	{
		lv_obj_update_layout(root_);
		return lv_obj_get_x(layers_);
	}

	void MapToolbar::setSearchText(const char* text)
	{
		lv_label_set_text_static(searchLabel_, text);
	}

	void MapToolbar::onLayers(lv_event_t* event)
	{
		static_cast<MapToolbar*>(lv_event_get_user_data(event))->listener_.onLayersPressed();
	}

	void MapToolbar::onSearch(lv_event_t* event)
	{
		static_cast<MapToolbar*>(lv_event_get_user_data(event))->listener_.onSearchPressed();
	}

	void MapToolbar::onRecenter(lv_event_t* event)
	{
		static_cast<MapToolbar*>(lv_event_get_user_data(event))->listener_.onRecenterPressed();
	}

}
