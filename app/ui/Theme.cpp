#include "ui/Theme.hpp"

namespace mars::ui::theme {

	namespace {

		Styles build()
		{
			Styles s;

			lv_style_init(&s.screen);
			lv_style_set_bg_color(&s.screen, lv_color_hex(kSurface));
			lv_style_set_bg_opa(&s.screen, LV_OPA_COVER);
			lv_style_set_text_color(&s.screen, lv_color_hex(kText));
			lv_style_set_text_font(&s.screen, &lv_font_montserrat_18);

			lv_style_init(&s.pill);
			lv_style_set_bg_color(&s.pill, lv_color_hex(kSurface));
			lv_style_set_bg_opa(&s.pill, LV_OPA_COVER);
			lv_style_set_radius(&s.pill, LV_RADIUS_CIRCLE);
			lv_style_set_text_color(&s.pill, lv_color_hex(kText));

			lv_style_init(&s.pressed);
			lv_style_set_bg_color(&s.pressed, lv_color_hex(kSurfacePressed));

			lv_style_init(&s.checked);
			lv_style_set_bg_color(&s.checked, lv_color_hex(kAccent));

			lv_style_init(&s.sheet);
			lv_style_set_bg_color(&s.sheet, lv_color_hex(kAccent));
			lv_style_set_bg_opa(&s.sheet, LV_OPA_COVER);
			lv_style_set_radius(&s.sheet, kSheetRadius);
			lv_style_set_text_color(&s.sheet, lv_color_hex(kText));

			lv_style_init(&s.card);
			lv_style_set_bg_color(&s.card, lv_color_hex(kSurface));
			lv_style_set_bg_opa(&s.card, LV_OPA_COVER);
			lv_style_set_radius(&s.card, kCardRadius);
			lv_style_set_text_color(&s.card, lv_color_hex(kText));

			lv_style_init(&s.backButton);
			lv_style_set_bg_color(&s.backButton, lv_color_hex(kBack));
			lv_style_set_bg_opa(&s.backButton, LV_OPA_COVER);
			lv_style_set_radius(&s.backButton, LV_RADIUS_CIRCLE);
			lv_style_set_text_color(&s.backButton, lv_color_hex(kSurface));

			lv_style_init(&s.hint);
			lv_style_set_text_color(&s.hint, lv_color_hex(kHint));

			lv_style_init(&s.smallText);
			lv_style_set_text_font(&s.smallText, &lv_font_montserrat_14);

			lv_style_init(&s.onMapText);
			lv_style_set_text_color(&s.onMapText, lv_color_hex(kOnMap));
			lv_style_set_text_font(&s.onMapText, &lv_font_montserrat_14);

			lv_style_init(&s.onMapTitle);
			lv_style_set_text_color(&s.onMapTitle, lv_color_hex(kOnMap));
			lv_style_set_text_font(&s.onMapTitle, &lv_font_montserrat_18);

			// A row "dot + text", vertically centred on its position.
			lv_style_init(&s.marker);
			lv_style_set_width(&s.marker, LV_SIZE_CONTENT);
			lv_style_set_height(&s.marker, LV_SIZE_CONTENT);
			lv_style_set_layout(&s.marker, LV_LAYOUT_FLEX);
			lv_style_set_flex_flow(&s.marker, LV_FLEX_FLOW_ROW);
			lv_style_set_flex_cross_place(&s.marker, LV_FLEX_ALIGN_CENTER);
			lv_style_set_pad_column(&s.marker, kMarkerLabelGap);
			lv_style_set_translate_y(&s.marker, lv_pct(-50));

			lv_style_init(&s.markerDot);
			lv_style_set_bg_color(&s.markerDot, lv_color_hex(kOnMap));
			lv_style_set_bg_opa(&s.markerDot, LV_OPA_COVER);
			lv_style_set_radius(&s.markerDot, LV_RADIUS_CIRCLE);

			return s;
		}

	}

	const Styles& styles()
	{
		static const Styles instance = build();
		return instance;
	}

}
