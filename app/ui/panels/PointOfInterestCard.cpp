#include "ui/panels/PointOfInterestCard.hpp"

#include "ui/Theme.hpp"
#include "ui/Widgets.hpp"

namespace mars::ui {

	namespace {

		constexpr std::int32_t kWidth = 300;
		constexpr std::int32_t kPadding = 18;
		constexpr std::int32_t kGap = 10;
		constexpr std::int32_t kBackWidth = 84;
		constexpr std::int32_t kBackHeight = 36;
		constexpr std::int32_t kFrameHeight = 180;
		constexpr std::int32_t kFrameRadius = 20;

		/// Extends the small "Back" pill to a full-size touch target.
		constexpr std::int32_t kBackTouchSlop = (theme::kTouchTarget - kBackHeight) / 2;

	}

	PointOfInterestCard::PointOfInterestCard(lv_obj_t* parent, IPointOfInterestCardListener& listener)
		: listener_{listener}
		, root_{widgets::box(parent, &theme::styles().card)}
	{
		const theme::Styles& styles = theme::styles();

		lv_obj_set_size(root_, kWidth, LV_SIZE_CONTENT);
		lv_obj_align(root_, LV_ALIGN_LEFT_MID, theme::kScreenMargin, 0);
		lv_obj_set_style_pad_all(root_, kPadding, LV_PART_MAIN);
		lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
		lv_obj_set_style_pad_row(root_, kGap, LV_PART_MAIN);
		// The card swallows presses instead of letting them reach the map.
		lv_obj_set_clickable(root_, true);

		lv_obj_t* const back = widgets::button(root_, &styles.backButton, onBack, this, &styles.backPressed);
		lv_obj_set_size(back, kBackWidth, kBackHeight);
		lv_obj_set_ext_click_area(back, kBackTouchSlop);
		lv_obj_add_style(back, &styles.smallText, LV_PART_MAIN);
		lv_obj_center(widgets::label(back, "Back"));

		frame_ = widgets::box(root_);
		lv_obj_set_size(frame_, lv_pct(100), kFrameHeight);
		lv_obj_set_style_radius(frame_, kFrameRadius, LV_PART_MAIN);
		lv_obj_set_style_bg_color(frame_, lv_color_hex(theme::kImagePlaceholder), LV_PART_MAIN);
		lv_obj_set_style_bg_opa(frame_, LV_OPA_COVER, LV_PART_MAIN);
		lv_obj_set_style_clip_corner(frame_, true, LV_PART_MAIN);

		credit_ = widgets::label(root_, "");
		lv_obj_set_width(credit_, lv_pct(100));
		lv_obj_add_style(credit_, &styles.smallText, LV_PART_MAIN);
		lv_obj_add_style(credit_, &styles.hint, LV_PART_MAIN);
		name_ = widgets::label(root_, "");
		lv_obj_set_width(name_, lv_pct(100));

		description_ = widgets::label(root_, "");
		lv_obj_set_width(description_, lv_pct(100));
		lv_obj_add_style(description_, &styles.smallText, LV_PART_MAIN);

		coordinates_ = widgets::label(root_, "");
		lv_obj_add_style(coordinates_, &styles.smallText, LV_PART_MAIN);
		lv_obj_add_style(coordinates_, &styles.hint, LV_PART_MAIN);

		hide();
	}

	void PointOfInterestCard::show(const PointOfInterest& point, const void* picture)
	{
		lv_label_set_text_static(name_, point.name);
		lv_label_set_text_static(description_, point.description);
		lv_label_set_text_static(credit_, point.photoCredit != nullptr ? point.photoCredit : "");
		widgets::setVisible(credit_, picture != nullptr && point.photoCredit != nullptr && point.photoCredit[0] != '\0');
		coordinatesText_ = formatPosition(point.position);
		lv_label_set_text_static(coordinates_, coordinatesText_.data());

		if (picture != nullptr && picture_ == nullptr) {
			// Created on demand: no image object as long as no picture is available.
			picture_ = lv_image_create(frame_);
			lv_obj_remove_style_all(picture_);
			lv_obj_set_size(picture_, lv_pct(100), lv_pct(100));
			lv_image_set_inner_align(picture_, LV_IMAGE_ALIGN_COVER);
		}
		if (picture_ != nullptr) {
			lv_image_set_src(picture_, picture);
			widgets::setVisible(picture_, picture != nullptr);
		}
		widgets::setVisible(root_, true);
	}

	void PointOfInterestCard::hide()
	{
		widgets::setVisible(root_, false);
	}

	bool PointOfInterestCard::isVisible() const
	{
		return !lv_obj_is_hidden(root_);
	}

	void PointOfInterestCard::onBack(lv_event_t* event)
	{
		static_cast<PointOfInterestCard*>(lv_event_get_user_data(event))->listener_.onPointOfInterestClosed();
	}

}
