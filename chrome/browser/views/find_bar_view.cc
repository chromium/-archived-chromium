// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/find_bar_view.h"

#include <algorithm>

#include "app/gfx/canvas.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/string_util.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/find_bar_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/find_bar_win.h"
#include "chrome/browser/view_ids.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "views/background.h"
#include "views/controls/button/image_button.h"
#include "views/controls/label.h"

// The amount of whitespace to have before the find button.
static const int kWhiteSpaceAfterMatchCountLabel = 1;

// The margins around the search field and the close button.
static const int kMarginLeftOfCloseButton = 3;
static const int kMarginRightOfCloseButton = 7;
static const int kMarginLeftOfFindTextfield = 12;

// The margins around the match count label (We add extra space so that the
// background highlight extends beyond just the text).
static const int kMatchCountExtraWidth = 9;

// Minimum width for the match count label.
static const int kMatchCountMinWidth = 30;

// The text color for the match count label.
static const SkColor kTextColorMatchCount = SkColorSetRGB(178, 178, 178);

// The text color for the match count label when no matches are found.
static const SkColor kTextColorNoMatch = SK_ColorBLACK;

// The background color of the match count label when results are found.
static const SkColor kBackgroundColorMatch = SkColorSetRGB(255, 255, 255);

// The background color of the match count label when no results are found.
static const SkColor kBackgroundColorNoMatch = SkColorSetRGB(255, 102, 102);

// The background images for the dialog. They are split into a left, a middle
// and a right part. The middle part determines the height of the dialog. The
// middle part is stretched to fill any remaining part between the left and the
// right image, after sizing the dialog to kWindowWidth.
static const SkBitmap* kDialog_left = NULL;
static const SkBitmap* kDialog_middle = NULL;
static const SkBitmap* kDialog_right = NULL;

// When we are animating, we draw only the top part of the left and right
// edges to give the illusion that the find dialog is attached to the
// window during this animation; this is the height of the items we draw.
static const int kAnimatingEdgeHeight = 5;

// The background image for the Find text box, which we draw behind the Find box
// to provide the Chrome look to the edge of the text box.
static const SkBitmap* kBackground = NULL;

// The rounded edge on the left side of the Find text box.
static const SkBitmap* kBackground_left = NULL;

// The default number of average characters that the text box will be. This
// number brings the width on a "regular fonts" system to about 300px.
static const int kDefaultCharWidth = 43;

////////////////////////////////////////////////////////////////////////////////
// FindBarView, public:

FindBarView::FindBarView(FindBarWin* container)
    : container_(container),
      find_text_(NULL),
      match_count_text_(NULL),
      focus_forwarder_view_(NULL),
      find_previous_button_(NULL),
      find_next_button_(NULL),
      close_button_(NULL),
      animation_offset_(0) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  find_text_ = new views::Textfield();
  find_text_->SetID(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD);
  find_text_->SetFont(rb.GetFont(ResourceBundle::BaseFont));
  find_text_->set_default_width_in_chars(kDefaultCharWidth);
  find_text_->SetController(this);
  AddChildView(find_text_);

  match_count_text_ = new views::Label();
  match_count_text_->SetFont(rb.GetFont(ResourceBundle::BaseFont));
  match_count_text_->SetColor(kTextColorMatchCount);
  match_count_text_->SetHorizontalAlignment(views::Label::ALIGN_CENTER);
  AddChildView(match_count_text_);

  // Create a focus forwarder view which sends focus to find_text_.
  focus_forwarder_view_ = new FocusForwarderView(find_text_);
  AddChildView(focus_forwarder_view_);

  find_previous_button_ = new views::ImageButton(this);
  find_previous_button_->set_tag(FIND_PREVIOUS_TAG);
  find_previous_button_->SetEnabled(false);
  find_previous_button_->SetFocusable(true);
  find_previous_button_->SetImage(views::CustomButton::BS_NORMAL,
      rb.GetBitmapNamed(IDR_FINDINPAGE_PREV));
  find_previous_button_->SetImage(views::CustomButton::BS_HOT,
      rb.GetBitmapNamed(IDR_FINDINPAGE_PREV_H));
  find_previous_button_->SetImage(views::CustomButton::BS_DISABLED,
      rb.GetBitmapNamed(IDR_FINDINPAGE_PREV_P));
  find_previous_button_->SetTooltipText(
      l10n_util::GetString(IDS_FIND_IN_PAGE_PREVIOUS_TOOLTIP));
  AddChildView(find_previous_button_);

  find_next_button_ = new views::ImageButton(this);
  find_next_button_->set_tag(FIND_NEXT_TAG);
  find_next_button_->SetEnabled(false);
  find_next_button_->SetFocusable(true);
  find_next_button_->SetImage(views::CustomButton::BS_NORMAL,
      rb.GetBitmapNamed(IDR_FINDINPAGE_NEXT));
  find_next_button_->SetImage(views::CustomButton::BS_HOT,
      rb.GetBitmapNamed(IDR_FINDINPAGE_NEXT_H));
  find_next_button_->SetImage(views::CustomButton::BS_DISABLED,
      rb.GetBitmapNamed(IDR_FINDINPAGE_NEXT_P));
  find_next_button_->SetTooltipText(
      l10n_util::GetString(IDS_FIND_IN_PAGE_NEXT_TOOLTIP));
  AddChildView(find_next_button_);

  close_button_ = new views::ImageButton(this);
  close_button_->set_tag(CLOSE_TAG);
  close_button_->SetFocusable(true);
  close_button_->SetImage(views::CustomButton::BS_NORMAL,
      rb.GetBitmapNamed(IDR_CLOSE_BAR));
  close_button_->SetImage(views::CustomButton::BS_HOT,
      rb.GetBitmapNamed(IDR_CLOSE_BAR_H));
  close_button_->SetImage(views::CustomButton::BS_PUSHED,
      rb.GetBitmapNamed(IDR_CLOSE_BAR_P));
  close_button_->SetTooltipText(
      l10n_util::GetString(IDS_FIND_IN_PAGE_CLOSE_TOOLTIP));
  AddChildView(close_button_);

  if (kDialog_left == NULL) {
    // Background images for the dialog.
    kDialog_left = rb.GetBitmapNamed(IDR_FIND_DIALOG_LEFT);
    kDialog_middle = rb.GetBitmapNamed(IDR_FIND_DIALOG_MIDDLE);
    kDialog_right = rb.GetBitmapNamed(IDR_FIND_DIALOG_RIGHT);

    // Background images for the Find edit box.
    kBackground = rb.GetBitmapNamed(IDR_FIND_BOX_BACKGROUND);
    if (UILayoutIsRightToLeft())
      kBackground_left = rb.GetBitmapNamed(IDR_FIND_BOX_BACKGROUND_LEFT_RTL);
    else
      kBackground_left = rb.GetBitmapNamed(IDR_FIND_BOX_BACKGROUND_LEFT);
  }
}

FindBarView::~FindBarView() {
}

void FindBarView::SetFindText(const string16& find_text) {
  find_text_->SetText(UTF16ToWide(find_text));
}

void FindBarView::UpdateForResult(const FindNotificationDetails& result,
                                  const string16& find_text) {
  bool have_valid_range =
      result.number_of_matches() != -1 && result.active_match_ordinal() != -1;

  // If we don't have any results and something was passed in, then that means
  // someone pressed F3 while the Find box was closed. In that case we need to
  // repopulate the Find box with what was passed in.
  std::wstring search_string = find_text_->text();
  if (search_string.empty() && !find_text.empty()) {
    find_text_->SetText(UTF16ToWide(find_text));
    find_text_->SelectAll();
  }

  if (!search_string.empty() && have_valid_range) {
    match_count_text_->SetText(
        l10n_util::GetStringF(IDS_FIND_IN_PAGE_COUNT,
                              IntToWString(result.active_match_ordinal()),
                              IntToWString(result.number_of_matches())));
  } else {
    // If there was no text entered, we don't show anything in the result count
    // area.
    match_count_text_->SetText(std::wstring());
  }

  if (search_string.empty() || result.number_of_matches() > 0 ||
      !have_valid_range) {
    // If there was no text entered or there were results, the match_count label
    // should have a normal background color. We also reset the background if
    // we don't have_valid_range, so that the text field will not show red
    // background when reopened after an unsuccessful find.
    ResetMatchCountBackground();
  } else if (result.final_update()) {
    // Otherwise we show an error background behind the match_count label.
    match_count_text_->set_background(
        views::Background::CreateSolidBackground(kBackgroundColorNoMatch));
    match_count_text_->SetColor(kTextColorNoMatch);
  }

  // Make sure Find Next and Find Previous are enabled if we found any matches.
  find_previous_button_->SetEnabled(result.number_of_matches() > 0);
  find_next_button_->SetEnabled(result.number_of_matches() > 0);

  // The match_count label may have increased/decreased in size so we need to
  // do a layout and repaint the dialog so that the find text field doesn't
  // partially overlap the match-count label when it increases on no matches.
  Layout();
  SchedulePaint();
}

void FindBarView::SetFocusAndSelection() {
  find_text_->RequestFocus();
  if (!find_text_->text().empty()) {
    find_text_->SelectAll();

    find_previous_button_->SetEnabled(true);
    find_next_button_->SetEnabled(true);
  }
}

///////////////////////////////////////////////////////////////////////////////
// FindBarView, views::View overrides:

void FindBarView::Paint(gfx::Canvas* canvas) {
  SkPaint paint;

  // Get the local bounds so that we now how much to stretch the background.
  gfx::Rect lb = GetLocalBounds(true);

  // First, we draw the background image for the whole dialog (3 images: left,
  // middle and right). Note, that the window region has been set by the
  // controller, so the whitespace in the left and right background images is
  // actually outside the window region and is therefore not drawn. See
  // FindInPageWidgetWin::CreateRoundedWindowEdges() for details.
  ThemeProvider* tp = GetThemeProvider();
  gfx::Rect bounds;
  container_->GetThemePosition(&bounds);
  canvas->TileImageInt(*tp->GetBitmapNamed(IDR_THEME_TOOLBAR),
                       bounds.x(), bounds.y(), 0, 0, lb.width(), lb.height());

  canvas->DrawBitmapInt(*kDialog_left, 0, 0);

  // Stretch the middle background to cover all of the area between the two
  // other images.
  canvas->TileImageInt(*kDialog_middle,
                        kDialog_left->width(),
                        0,
                        lb.width() -
                            kDialog_left->width() -
                            kDialog_right->width(),
                        kDialog_middle->height());

  canvas->DrawBitmapInt(*kDialog_right, lb.right() - kDialog_right->width(), 0);

  // Then we draw the background image for the Find Textfield. We start by
  // calculating the position of background images for the Find text box.
  gfx::Rect find_text_rect;
  gfx::Rect back_button_rect;
  int x = 0;   // x coordinate of the curved edge background image.
  int w = 0;   // width of the background image for the text field.
  if (UILayoutIsRightToLeft()) {
    find_text_rect = find_text_->GetBounds(APPLY_MIRRORING_TRANSFORMATION);
    back_button_rect =
        find_previous_button_->GetBounds(APPLY_MIRRORING_TRANSFORMATION);
    x = find_text_rect.right();
    w = find_text_rect.right() - back_button_rect.right();
  } else {
    find_text_rect = find_text_->bounds();
    back_button_rect = find_previous_button_->bounds();
    x = find_text_rect.x() - kBackground_left->width();
    w = back_button_rect.x() - find_text_rect.x();
  }

  // Draw the image to the left that creates a curved left edge for the box
  // (drawn on the right for RTL languages).
  canvas->TileImageInt(*kBackground_left,
                       x,
                       back_button_rect.y(),
                       kBackground_left->width(),
                       kBackground_left->height());

  // Draw the top and bottom border for whole text box (encompasses both the
  // find_text_ edit box and the match_count_text_ label).
  int background_height = kBackground->height();
  canvas->TileImageInt(*kBackground,
                       UILayoutIsRightToLeft() ?
                           back_button_rect.right() : find_text_rect.x(),
                       back_button_rect.y(),
                       w,
                       background_height);

  if (animation_offset_ > 0) {
    // While animating we draw the curved edges at the point where the
    // controller told us the top of the window is: |animation_offset_|.
    canvas->TileImageInt(*kDialog_left,
                         lb.x(),
                         animation_offset_,
                         kDialog_left->width(),
                         kAnimatingEdgeHeight);
    canvas->TileImageInt(*kDialog_right,
                         lb.right() - kDialog_right->width(),
                         animation_offset_,
                         kDialog_right->width(),
                         kAnimatingEdgeHeight);
  }
}

void FindBarView::Layout() {
  gfx::Size panel_size = GetPreferredSize();

  // First we draw the close button on the far right.
  gfx::Size sz = close_button_->GetPreferredSize();
  close_button_->SetBounds(panel_size.width() - sz.width() -
                               kMarginRightOfCloseButton,
                           (height() - sz.height()) / 2,
                           sz.width(),
                           sz.height());

  // Next, the FindNext button to the left the close button.
  sz = find_next_button_->GetPreferredSize();
  find_next_button_->SetBounds(close_button_->x() -
                                   find_next_button_->width() -
                                   kMarginLeftOfCloseButton,
                               (height() - sz.height()) / 2,
                                sz.width(),
                                sz.height());

  // Then, the FindPrevious button to the left the FindNext button.
  sz = find_previous_button_->GetPreferredSize();
  find_previous_button_->SetBounds(find_next_button_->x() -
                                       find_previous_button_->width(),
                                   (height() - sz.height()) / 2,
                                   sz.width(),
                                   sz.height());

  // Then the label showing the match count number.
  sz = match_count_text_->GetPreferredSize();
  // We want to make sure the red "no-match" background almost completely fills
  // up the amount of vertical space within the text box. We therefore fix the
  // size relative to the button heights. We use the FindPrev button, which has
  // a 1px outer whitespace margin, 1px border and we want to appear 1px below
  // the border line so we subtract 3 for top and 3 for bottom.
  sz.set_height(find_previous_button_->height() - 6);  // Subtract 3px x 2.

  // We extend the label bounds a bit to give the background highlighting a bit
  // of breathing room (margins around the text).
  sz.Enlarge(kMatchCountExtraWidth, 0);
  sz.set_width(std::max(kMatchCountMinWidth, static_cast<int>(sz.width())));
  int match_count_x = find_previous_button_->x() -
                      kWhiteSpaceAfterMatchCountLabel -
                      sz.width();
  match_count_text_->SetBounds(match_count_x,
                               (height() - sz.height()) / 2,
                               sz.width(),
                               sz.height());

  // And whatever space is left in between, gets filled up by the find edit box.
  sz = find_text_->GetPreferredSize();
  sz.set_width(match_count_x - kMarginLeftOfFindTextfield);
  find_text_->SetBounds(match_count_x - sz.width(),
                        (height() - sz.height()) / 2 + 1,
                        sz.width(),
                        sz.height());

  // The focus forwarder view is a hidden view that should cover the area
  // between the find text box and the find button so that when the user clicks
  // in that area we focus on the find text box.
  int find_text_edge = find_text_->x() + find_text_->width();
  focus_forwarder_view_->SetBounds(find_text_edge,
                                   find_previous_button_->y(),
                                   find_previous_button_->x() -
                                       find_text_edge,
                                   find_previous_button_->height());
}

void FindBarView::ViewHierarchyChanged(bool is_add, View *parent, View *child) {
  if (is_add && child == this) {
    find_text_->SetHorizontalMargins(3, 3);  // Left and Right margins.
    find_text_->RemoveBorder();  // We draw our own border (a background image).
  }
}

gfx::Size FindBarView::GetPreferredSize() {
  gfx::Size prefsize = find_text_->GetPreferredSize();
  prefsize.set_height(kDialog_middle->height());

  // Add up all the preferred sizes and margins of the rest of the controls.
  prefsize.Enlarge(kMarginLeftOfCloseButton + kMarginRightOfCloseButton +
                       kMarginLeftOfFindTextfield,
                   0);
  prefsize.Enlarge(find_previous_button_->GetPreferredSize().width(), 0);
  prefsize.Enlarge(find_next_button_->GetPreferredSize().width(), 0);
  prefsize.Enlarge(close_button_->GetPreferredSize().width(), 0);
  return prefsize;
}

////////////////////////////////////////////////////////////////////////////////
// FindBarView, views::ButtonListener implementation:

void FindBarView::ButtonPressed(views::Button* sender) {
  switch (sender->tag()) {
    case FIND_PREVIOUS_TAG:
    case FIND_NEXT_TAG:
      if (!find_text_->text().empty()) {
        container_->GetFindBarController()->tab_contents()->StartFinding(
            WideToUTF16(find_text_->text()),
            sender->tag() == FIND_NEXT_TAG,
            false);  // Not case sensitive.
      }
      // Move the focus back to the text-field, we don't want the button
      // focused.
      // TODO(jcampan): http://crbug.com/9867 we should not change the focus
      //                when teh button was pressed by pressing a key.
      find_text_->RequestFocus();
      break;
    case CLOSE_TAG:
      container_->GetFindBarController()->EndFindSession();
      break;
    default:
      NOTREACHED() << L"Unknown button";
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// FindBarView, views::Textfield::Controller implementation:

void FindBarView::ContentsChanged(views::Textfield* sender,
                                  const std::wstring& new_contents) {
  FindBarController* controller = container_->GetFindBarController();
  DCHECK(controller);
  // We must guard against a NULL tab_contents, which can happen if the text
  // in the Find box is changed right after the tab is destroyed. Otherwise, it
  // can lead to crashes, as exposed by automation testing in issue 8048.
  if (!controller->tab_contents())
    return;

  // When the user changes something in the text box we check the contents and
  // if the textbox contains something we set it as the new search string and
  // initiate search (even though old searches might be in progress).
  if (!new_contents.empty()) {
    // The last two params here are forward (true) and case sensitive (false).
    controller->tab_contents()->StartFinding(WideToUTF16(new_contents),
                                             true, false);
  } else {
    // The textbox is empty so we reset.  true = clear selection on page.
    controller->tab_contents()->StopFinding(true);
    UpdateForResult(controller->tab_contents()->find_result(), string16());
  }
}

bool FindBarView::HandleKeystroke(views::Textfield* sender,
                                  const views::Textfield::Keystroke& key) {
  // If the dialog is not visible, there is no reason to process keyboard input.
  if (!container_->IsVisible())
    return false;

  // TODO(port): Handle this for other platforms.
  #if defined(OS_WIN)
  if (container_->MaybeForwardKeystrokeToWebpage(key.message, key.key,
                                                 key.flags))
    return true;  // Handled, we are done!

  if (views::Textfield::IsKeystrokeEnter(key)) {
    // Pressing Return/Enter starts the search (unless text box is empty).
    std::wstring find_string = find_text_->text();
    if (!find_string.empty()) {
      // Search forwards for enter, backwards for shift-enter.
      container_->GetFindBarController()->tab_contents()->StartFinding(
          find_string,
          GetKeyState(VK_SHIFT) >= 0,
          false);  // Not case sensitive.
    }
  }
  #endif

  return false;
}

void FindBarView::ResetMatchCountBackground() {
  match_count_text_->set_background(
      views::Background::CreateSolidBackground(kBackgroundColorMatch));
  match_count_text_->SetColor(kTextColorMatchCount);
}

bool FindBarView::FocusForwarderView::OnMousePressed(
    const views::MouseEvent& event) {
  if (view_to_focus_on_mousedown_) {
    view_to_focus_on_mousedown_->ClearSelection();
    view_to_focus_on_mousedown_->RequestFocus();
  }
  return true;
}
