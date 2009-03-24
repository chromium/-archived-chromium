// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/location_bar_view.h"

#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/alternate_nav_url_fetcher.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/info_bubble.h"
#include "chrome/browser/views/first_run_bubble.h"
#include "chrome/browser/views/page_info_window.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/win_util.h"
#include "chrome/views/background.h"
#include "chrome/views/border.h"
#include "chrome/views/widget/root_view.h"
#include "chrome/views/widget/widget.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

using views::View;

const int LocationBarView::kVertMargin = 2;

const COLORREF LocationBarView::kBackgroundColorByLevel[] = {
  RGB(255, 245, 195),  // SecurityLevel SECURE: Yellow.
  RGB(255, 255, 255),  // SecurityLevel NORMAL: White.
  RGB(255, 255, 255),  // SecurityLevel INSECURE: White.
};

// Padding on the right and left of the entry field.
static const int kEntryPadding = 3;

// Padding between the entry and the leading/trailing views.
static const int kInnerPadding = 3;

static const SkBitmap* kBackground = NULL;

static const SkBitmap* kPopupBackground = NULL;

// The delay the mouse has to be hovering over the lock/warning icon before the
// info bubble is shown.
static const int kInfoBubbleHoverDelayMs = 500;

// The tab key image.
static const SkBitmap* kTabButtonBitmap = NULL;

// Returns the description for a keyword.
static std::wstring GetKeywordDescription(Profile* profile,
                                          const std::wstring& keyword) {
  // Make sure the TemplateURL still exists.
  // TODO(sky): Once LocationBarView adds a listener to the TemplateURLModel
  // to track changes to the model, this should become a DCHECK.
  const TemplateURL* template_url =
      profile->GetTemplateURLModel()->GetTemplateURLForKeyword(keyword);
  if (template_url) {
    std::wstring keyword_description;
    if (l10n_util::AdjustStringForLocaleDirection(template_url->short_name(),
                                                  &keyword_description))
      return keyword_description;
    return template_url->short_name();
  }
  return std::wstring();
}

LocationBarView::LocationBarView(Profile* profile,
                                 CommandUpdater* command_updater,
                                 ToolbarModel* model,
                                 Delegate* delegate,
                                 bool popup_window_mode)
    : profile_(profile),
      command_updater_(command_updater),
      model_(model),
      delegate_(delegate),
      disposition_(CURRENT_TAB),
      location_entry_view_(NULL),
      selected_keyword_view_(profile),
      keyword_hint_view_(profile),
      type_to_search_view_(l10n_util::GetString(IDS_OMNIBOX_EMPTY_TEXT)),
      security_image_view_(profile, model),
      rss_image_view_(model),
      popup_window_mode_(popup_window_mode),
      first_run_bubble_(this) {
  DCHECK(profile_);
  SetID(VIEW_ID_LOCATION_BAR);
  SetFocusable(true);

  if (!kBackground) {
    ResourceBundle &rb = ResourceBundle::GetSharedInstance();
    kBackground = rb.GetBitmapNamed(IDR_LOCATIONBG);
    kPopupBackground = rb.GetBitmapNamed(IDR_LOCATIONBG_POPUPMODE_CENTER);
  }
}

bool LocationBarView::IsInitialized() const {
  return location_entry_view_ != NULL;
}

void LocationBarView::Init() {
  if (popup_window_mode_) {
    font_ = ResourceBundle::GetSharedInstance().GetFont(
        ResourceBundle::BaseFont);
  } else {
    // Use a larger version of the system font.
    font_ = font_.DeriveFont(3);
  }

  // URL edit field.
  views::Widget* widget = GetWidget();
  location_entry_.reset(new AutocompleteEditViewWin(font_, this, model_, this,
                                                    widget->GetNativeView(),
                                                    profile_, command_updater_,
                                                    popup_window_mode_));

  // View container for URL edit field.
  location_entry_view_ = new views::HWNDView;
  DCHECK(location_entry_view_) << "LocationBarView::Init - OOM!";
  location_entry_view_->SetID(VIEW_ID_AUTOCOMPLETE);
  AddChildView(location_entry_view_);
  location_entry_view_->SetAssociatedFocusView(this);
  location_entry_view_->Attach(location_entry_->m_hWnd);

  AddChildView(&selected_keyword_view_);
  selected_keyword_view_.SetFont(font_);
  selected_keyword_view_.SetVisible(false);
  selected_keyword_view_.SetParentOwned(false);

  DWORD sys_color = GetSysColor(COLOR_GRAYTEXT);
  SkColor gray = SkColorSetRGB(GetRValue(sys_color), GetGValue(sys_color),
                               GetBValue(sys_color));

  AddChildView(&type_to_search_view_);
  type_to_search_view_.SetVisible(false);
  type_to_search_view_.SetFont(font_);
  type_to_search_view_.SetColor(gray);
  type_to_search_view_.SetParentOwned(false);

  AddChildView(&keyword_hint_view_);
  keyword_hint_view_.SetVisible(false);
  keyword_hint_view_.SetFont(font_);
  keyword_hint_view_.SetColor(gray);
  keyword_hint_view_.SetParentOwned(false);

  AddChildView(&rss_image_view_);
  rss_image_view_.SetVisible(false);
  rss_image_view_.SetParentOwned(false);

  AddChildView(&security_image_view_);
  security_image_view_.SetVisible(false);
  security_image_view_.SetParentOwned(false);

  AddChildView(&info_label_);
  info_label_.SetVisible(false);
  info_label_.SetParentOwned(false);

  // Notify us when any ancestor is resized.  In this case we want to tell the
  // AutocompleteEditView to close its popup.
  SetNotifyWhenVisibleBoundsInRootChanges(true);

  // Initialize the location entry. We do this to avoid a black flash which is
  // visible when the location entry has just been initialized.
  Update(NULL);

  OnChanged();
}

void LocationBarView::Update(const TabContents* tab_for_state_restoring) {
  SetSecurityIcon(model_->GetIcon());
  SetRssIconVisibility(model_->GetFeedList().get());
  std::wstring info_text, info_tooltip;
  SkColor text_color;
  model_->GetInfoText(&info_text, &text_color, &info_tooltip);
  SetInfoText(info_text, text_color, info_tooltip);
  location_entry_->Update(tab_for_state_restoring);
  Layout();
  SchedulePaint();
}

void LocationBarView::UpdateFeedIcon() {
  SetRssIconVisibility(model_->GetFeedList().get());
  Layout();
  SchedulePaint();
}

void LocationBarView::Focus() {
  ::SetFocus(location_entry_->m_hWnd);
}

void LocationBarView::SetProfile(Profile* profile) {
  DCHECK(profile);
  if (profile_ != profile) {
    profile_ = profile;
    location_entry_->model()->SetProfile(profile);
    selected_keyword_view_.set_profile(profile);
    keyword_hint_view_.set_profile(profile);
    security_image_view_.set_profile(profile);
  }
}

gfx::Size LocationBarView::GetPreferredSize() {
  return gfx::Size(0,
      (popup_window_mode_ ? kPopupBackground : kBackground)->height());
}

void LocationBarView::Layout() {
  DoLayout(true);
}

void LocationBarView::Paint(ChromeCanvas* canvas) {
  View::Paint(canvas);

  SkColor bg = SkColorSetRGB(
      GetRValue(kBackgroundColorByLevel[model_->GetSchemeSecurityLevel()]),
      GetGValue(kBackgroundColorByLevel[model_->GetSchemeSecurityLevel()]),
      GetBValue(kBackgroundColorByLevel[model_->GetSchemeSecurityLevel()]));

  const SkBitmap* background =
      popup_window_mode_ ? kPopupBackground : kBackground;
  canvas->TileImageInt(*background, 0, 0, 0, 0, width(), height());
  int top_margin = TopMargin();
  canvas->FillRectInt(bg, 0, top_margin, width(),
                      std::max(height() - top_margin - kVertMargin, 0));
}

bool LocationBarView::CanProcessTabKeyEvents() {
  // We want to receive tab key events when the hint is showing.
  return keyword_hint_view_.IsVisible();
}

void LocationBarView::VisibleBoundsInRootChanged() {
  location_entry_->ClosePopup();
}

bool LocationBarView::OnMousePressed(const views::MouseEvent& event) {
  UINT msg;
  if (event.IsLeftMouseButton()) {
    msg = (event.GetFlags() & views::MouseEvent::EF_IS_DOUBLE_CLICK) ?
        WM_LBUTTONDBLCLK : WM_LBUTTONDOWN;
  } else if (event.IsMiddleMouseButton()) {
    msg = (event.GetFlags() & views::MouseEvent::EF_IS_DOUBLE_CLICK) ?
        WM_MBUTTONDBLCLK : WM_MBUTTONDOWN;
  } else if (event.IsRightMouseButton()) {
    msg = (event.GetFlags() & views::MouseEvent::EF_IS_DOUBLE_CLICK) ?
        WM_RBUTTONDBLCLK : WM_RBUTTONDOWN;
  } else {
    NOTREACHED();
    return false;
  }
  OnMouseEvent(event, msg);
  return true;
}

bool LocationBarView::OnMouseDragged(const views::MouseEvent& event) {
  OnMouseEvent(event, WM_MOUSEMOVE);
  return true;
}

void LocationBarView::OnMouseReleased(const views::MouseEvent& event,
                                      bool canceled) {
  UINT msg;
  if (canceled) {
    msg = WM_CAPTURECHANGED;
  } else if (event.IsLeftMouseButton()) {
    msg = WM_LBUTTONUP;
  } else if (event.IsMiddleMouseButton()) {
    msg = WM_MBUTTONUP;
  } else if (event.IsRightMouseButton()) {
    msg = WM_RBUTTONUP;
  } else {
    NOTREACHED();
    return;
  }
  OnMouseEvent(event, msg);
}

void LocationBarView::OnAutocompleteAccept(
    const GURL& url,
    WindowOpenDisposition disposition,
    PageTransition::Type transition,
    const GURL& alternate_nav_url) {
  if (!url.is_valid())
    return;

  location_input_ = UTF8ToWide(url.spec());
  disposition_ = disposition;
  transition_ = transition;

  if (command_updater_) {
    if (!alternate_nav_url.is_valid()) {
      command_updater_->ExecuteCommand(IDC_OPEN_CURRENT_URL);
      return;
    }

    scoped_ptr<AlternateNavURLFetcher> fetcher(
        new AlternateNavURLFetcher(alternate_nav_url));
    // The AlternateNavURLFetcher will listen for the pending navigation
    // notification that will be issued as a result of the "open URL." It
    // will automatically install itself into that navigation controller.
    command_updater_->ExecuteCommand(IDC_OPEN_CURRENT_URL);
    if (fetcher->state() == AlternateNavURLFetcher::NOT_STARTED) {
      // I'm not sure this should be reachable, but I'm not also sure enough
      // that it shouldn't to stick in a NOTREACHED().  In any case, this is
      // harmless; we can simply let the fetcher get deleted here and it will
      // clean itself up properly.
    } else {
      fetcher.release();  // The navigation controller will delete the fetcher.
    }
  }
}

void LocationBarView::OnChanged() {
  DoLayout(false);
}

SkBitmap LocationBarView::GetFavIcon() const {
  DCHECK(delegate_);
  DCHECK(delegate_->GetTabContents());
  return delegate_->GetTabContents()->GetFavIcon();
}

std::wstring LocationBarView::GetTitle() const {
  DCHECK(delegate_);
  DCHECK(delegate_->GetTabContents());
  return UTF16ToWideHack(delegate_->GetTabContents()->GetTitle());
}

void LocationBarView::DoLayout(const bool force_layout) {
  if (!location_entry_.get())
    return;

  RECT formatting_rect;
  location_entry_->GetRect(&formatting_rect);
  RECT edit_bounds;
  location_entry_->GetClientRect(&edit_bounds);

  int entry_width = width() - (kEntryPadding * 2);

  gfx::Size rss_image_size;
  if (rss_image_view_.IsVisible()) {
    rss_image_size = rss_image_view_.GetPreferredSize();
    entry_width -= rss_image_size.width();
  }
  gfx::Size security_image_size;
  if (security_image_view_.IsVisible()) {
    security_image_size = security_image_view_.GetPreferredSize();
    entry_width -= security_image_size.width() + kInnerPadding;
  }
  gfx::Size info_label_size;
  if (info_label_.IsVisible()) {
    info_label_size = info_label_.GetPreferredSize();
    entry_width -= (info_label_size.width() + kInnerPadding);
  }

  const int max_edit_width = entry_width - formatting_rect.left -
                             (edit_bounds.right - formatting_rect.right);
  if (max_edit_width < 0)
    return;
  const int text_width = TextDisplayWidth();
  bool needs_layout = force_layout;
  needs_layout |= AdjustHints(text_width, max_edit_width);

  if (!needs_layout)
    return;

  // TODO(sky): baseline layout.
  int location_y = TopMargin();
  int location_height = std::max(height() - location_y - kVertMargin, 0);
  if (info_label_.IsVisible()) {
    info_label_.SetBounds(width() - kEntryPadding - info_label_size.width(),
                          location_y,
                          info_label_size.width(), location_height);
  }
  const int info_label_width = info_label_size.width() ?
      info_label_size.width() + kInnerPadding : 0;
  if (rss_image_view_.IsVisible()) {
    rss_image_view_.SetBounds(width() - kEntryPadding -
                                  info_label_width -
                                  security_image_size.width() -
                                  rss_image_size.width(),
                              location_y,
                              rss_image_size.width(),
                              location_height);
  }
  if (security_image_view_.IsVisible()) {
    security_image_view_.SetBounds(width() - kEntryPadding - info_label_width -
        security_image_size.width(), location_y, security_image_size.width(),
        location_height);
  }
  gfx::Rect location_bounds(kEntryPadding, location_y, entry_width,
                            location_height);
  if (selected_keyword_view_.IsVisible()) {
    LayoutView(true, &selected_keyword_view_, text_width, max_edit_width,
               &location_bounds);
  } else if (keyword_hint_view_.IsVisible()) {
    LayoutView(false, &keyword_hint_view_, text_width, max_edit_width,
               &location_bounds);
  } else if (type_to_search_view_.IsVisible()) {
    LayoutView(false, &type_to_search_view_, text_width, max_edit_width,
               &location_bounds);
  }

  location_entry_view_->SetBounds(location_bounds);
  if (!force_layout) {
    // If force_layout is false and we got this far it means one of the views
    // was added/removed or changed in size. We need to paint ourselves.
    SchedulePaint();
  }
}

int LocationBarView::TopMargin() const {
  return std::min(kVertMargin, height());
}

int LocationBarView::TextDisplayWidth() {
  POINT last_char_position =
      location_entry_->PosFromChar(location_entry_->GetTextLength());
  POINT scroll_position;
  location_entry_->GetScrollPos(&scroll_position);
  const int position_x = last_char_position.x + scroll_position.x;
  return UILayoutIsRightToLeft() ? width() - position_x : position_x;
}

bool LocationBarView::UsePref(int pref_width, int text_width, int max_width) {
  return (pref_width + kInnerPadding + text_width <= max_width);
}

bool LocationBarView::NeedsResize(View* view, int text_width, int max_width) {
  gfx::Size size = view->GetPreferredSize();
  if (!UsePref(size.width(), text_width, max_width))
    size = view->GetMinimumSize();
  return (view->width() != size.width());
}

bool LocationBarView::AdjustHints(int text_width, int max_width) {
  const std::wstring keyword(location_entry_->model()->keyword());
  const bool is_keyword_hint(location_entry_->model()->is_keyword_hint());
  const bool show_selected_keyword = !keyword.empty() && !is_keyword_hint;
  const bool show_keyword_hint = !keyword.empty() && is_keyword_hint;
  bool show_search_hint(location_entry_->model()->show_search_hint());
  DCHECK(keyword.empty() || !show_search_hint);

  if (show_search_hint) {
    // Only show type to search if all the text fits.
    gfx::Size view_pref = type_to_search_view_.GetPreferredSize();
    show_search_hint = UsePref(view_pref.width(), text_width, max_width);
  }

  // NOTE: This isn't just one big || statement as ToggleVisibility MUST be
  // invoked for each view.
  bool needs_layout = false;
  needs_layout |= ToggleVisibility(show_selected_keyword,
                                   &selected_keyword_view_);
  needs_layout |= ToggleVisibility(show_keyword_hint, &keyword_hint_view_);
  needs_layout |= ToggleVisibility(show_search_hint, &type_to_search_view_);
  if (show_selected_keyword) {
    if (selected_keyword_view_.keyword() != keyword) {
      needs_layout = true;
      selected_keyword_view_.SetKeyword(keyword);
    }
    needs_layout |= NeedsResize(&selected_keyword_view_, text_width, max_width);
  } else if (show_keyword_hint) {
    if (keyword_hint_view_.keyword() != keyword) {
      needs_layout = true;
      keyword_hint_view_.SetKeyword(keyword);
    }
    needs_layout |= NeedsResize(&keyword_hint_view_, text_width, max_width);
  }

  return needs_layout;
}

void LocationBarView::LayoutView(bool leading, views::View* view,
                                 int text_width, int max_width,
                                 gfx::Rect* bounds) {
  DCHECK(view && bounds);
  gfx::Size view_size = view->GetPreferredSize();
  if (!UsePref(view_size.width(), text_width, max_width))
    view_size = view->GetMinimumSize();
  if (view_size.width() + kInnerPadding < bounds->width()) {
    view->SetVisible(true);
    if (leading) {
      view->SetBounds(bounds->x(), bounds->y(), view_size.width(),
                      bounds->height());
      bounds->Offset(view_size.width() + kInnerPadding, 0);
    } else {
      view->SetBounds(bounds->right() - view_size.width(), bounds->y(),
                      view_size.width(), bounds->height());
    }
    bounds->set_width(bounds->width() - view_size.width() - kInnerPadding);
  } else {
    view->SetVisible(false);
  }
}

void LocationBarView::SetSecurityIcon(ToolbarModel::Icon icon) {
  switch (icon) {
    case ToolbarModel::LOCK_ICON:
      security_image_view_.SetImageShown(SecurityImageView::LOCK);
      security_image_view_.SetVisible(true);
      break;
    case ToolbarModel::WARNING_ICON:
      security_image_view_.SetImageShown(SecurityImageView::WARNING);
      security_image_view_.SetVisible(true);
      break;
    case ToolbarModel::NO_ICON:
      security_image_view_.SetVisible(false);
      break;
    default:
      NOTREACHED();
      security_image_view_.SetVisible(false);
      break;
  }
}

void LocationBarView::SetRssIconVisibility(FeedList* feeds) {
  bool show_rss = feeds && feeds->list().size() > 0;
  // TODO(finnur): Enable this when we have a good landing page to show feeds.
  rss_image_view_.SetVisible(false);
}

void LocationBarView::SetInfoText(const std::wstring& text,
                                  SkColor text_color,
                                  const std::wstring& tooltip_text) {
  info_label_.SetVisible(!text.empty());
  info_label_.SetText(text);
  info_label_.SetColor(text_color);
  info_label_.SetTooltipText(tooltip_text);
}

bool LocationBarView::ToggleVisibility(bool new_vis, View* view) {
  DCHECK(view);
  if (view->IsVisible() != new_vis) {
    view->SetVisible(new_vis);
    return true;
  }
  return false;
}

void LocationBarView::OnMouseEvent(const views::MouseEvent& event, UINT msg) {
  UINT flags = 0;
  if (event.IsControlDown())
    flags |= MK_CONTROL;
  if (event.IsShiftDown())
    flags |= MK_SHIFT;
  if (event.IsLeftMouseButton())
    flags |= MK_LBUTTON;
  if (event.IsMiddleMouseButton())
    flags |= MK_MBUTTON;
  if (event.IsRightMouseButton())
    flags |= MK_RBUTTON;

  gfx::Point screen_point(event.location());
  ConvertPointToScreen(this, &screen_point);

  location_entry_->HandleExternalMsg(msg, flags, screen_point.ToPOINT());
}

bool LocationBarView::GetAccessibleRole(VARIANT* role) {
  DCHECK(role);

  role->vt = VT_I4;
  role->lVal = ROLE_SYSTEM_GROUPING;
  return true;
}

// SelectedKeywordView -------------------------------------------------------

// The background is drawn using ImagePainter3. This is the left/center/right
// image names.
static const int kBorderImages[] = {
    IDR_LOCATION_BAR_SELECTED_KEYWORD_BACKGROUND_L,
    IDR_LOCATION_BAR_SELECTED_KEYWORD_BACKGROUND_C,
    IDR_LOCATION_BAR_SELECTED_KEYWORD_BACKGROUND_R };

// Insets around the label.
static const int kTopInset = 0;
static const int kBottomInset = 0;
static const int kLeftInset = 4;
static const int kRightInset = 4;

// Offset from the top the background is drawn at.
static const int kBackgroundYOffset = 2;

LocationBarView::SelectedKeywordView::SelectedKeywordView(Profile* profile)
    : background_painter_(kBorderImages),
      profile_(profile) {
  AddChildView(&full_label_);
  AddChildView(&partial_label_);
  // Full_label and partial_label are deleted by us, make sure View doesn't
  // delete them too.
  full_label_.SetParentOwned(false);
  partial_label_.SetParentOwned(false);
  full_label_.SetVisible(false);
  partial_label_.SetVisible(false);
  full_label_.set_border(
      views::Border::CreateEmptyBorder(kTopInset, kLeftInset, kBottomInset,
                                       kRightInset));
  partial_label_.set_border(
      views::Border::CreateEmptyBorder(kTopInset, kLeftInset, kBottomInset,
                                       kRightInset));
}

LocationBarView::SelectedKeywordView::~SelectedKeywordView() {
}

void LocationBarView::SelectedKeywordView::SetFont(const ChromeFont& font) {
  full_label_.SetFont(font);
  partial_label_.SetFont(font);
}

void LocationBarView::SelectedKeywordView::Paint(ChromeCanvas* canvas) {
  canvas->TranslateInt(0, kBackgroundYOffset);
  background_painter_.Paint(width(), height() - kTopInset, canvas);
  canvas->TranslateInt(0, -kBackgroundYOffset);
}

gfx::Size LocationBarView::SelectedKeywordView::GetPreferredSize() {
  return full_label_.GetPreferredSize();
}

gfx::Size LocationBarView::SelectedKeywordView::GetMinimumSize() {
  return partial_label_.GetMinimumSize();
}

void LocationBarView::SelectedKeywordView::Layout() {
  gfx::Size pref = GetPreferredSize();
  bool at_pref = (width() == pref.width());
  if (at_pref)
    full_label_.SetBounds(0, 0, width(), height());
  else
    partial_label_.SetBounds(0, 0, width(), height());
  full_label_.SetVisible(at_pref);
  partial_label_.SetVisible(!at_pref);
}

void LocationBarView::SelectedKeywordView::SetKeyword(
    const std::wstring& keyword) {
  keyword_ = keyword;
  if (keyword.empty())
    return;
  DCHECK(profile_);
  if (!profile_->GetTemplateURLModel())
    return;

  const std::wstring description = GetKeywordDescription(profile_, keyword);
  full_label_.SetText(l10n_util::GetStringF(IDS_OMNIBOX_KEYWORD_TEXT,
                                            description));
  const std::wstring min_string = CalculateMinString(description);
  if (!min_string.empty()) {
    partial_label_.SetText(
        l10n_util::GetStringF(IDS_OMNIBOX_KEYWORD_TEXT, min_string));
  } else {
    partial_label_.SetText(full_label_.GetText());
  }
}

std::wstring LocationBarView::SelectedKeywordView::CalculateMinString(
    const std::wstring& description) {
  // Chop at the first '.' or whitespace.
  const size_t dot_index = description.find(L'.');
  const size_t ws_index = description.find_first_of(kWhitespaceWide);
  size_t chop_index = std::min(dot_index, ws_index);
  std::wstring min_string;
  if (chop_index == std::wstring::npos) {
    // No dot or whitespace, truncate to at most 3 chars.
    min_string = l10n_util::TruncateString(description, 3);
  } else {
    min_string = description.substr(0, chop_index);
  }
  l10n_util::AdjustStringForLocaleDirection(min_string, &min_string);
  return min_string;
}

// KeywordHintView -------------------------------------------------------------

// Amount of space to offset the tab image from the top of the view by.
static const int kTabImageYOffset = 4;

LocationBarView::KeywordHintView::KeywordHintView(Profile* profile)
    : profile_(profile) {
  AddChildView(&leading_label_);
  AddChildView(&trailing_label_);

  if (!kTabButtonBitmap) {
    kTabButtonBitmap = ResourceBundle::GetSharedInstance().
        GetBitmapNamed(IDR_LOCATION_BAR_KEYWORD_HINT_TAB);
  }
}

LocationBarView::KeywordHintView::~KeywordHintView() {
  // Labels are freed by us. Remove them so that View doesn't
  // try to free them too.
  RemoveChildView(&leading_label_);
  RemoveChildView(&trailing_label_);
}

void LocationBarView::KeywordHintView::SetFont(const ChromeFont& font) {
  leading_label_.SetFont(font);
  trailing_label_.SetFont(font);
}

void LocationBarView::KeywordHintView::SetColor(const SkColor& color) {
  leading_label_.SetColor(color);
  trailing_label_.SetColor(color);
}

void LocationBarView::KeywordHintView::SetKeyword(const std::wstring& keyword) {
  keyword_ = keyword;
  if (keyword_.empty())
    return;
  DCHECK(profile_);
  if (!profile_->GetTemplateURLModel())
    return;

  std::vector<size_t> content_param_offsets;
  const std::wstring keyword_hint(l10n_util::GetStringF(
      IDS_OMNIBOX_KEYWORD_HINT, std::wstring(),
      GetKeywordDescription(profile_, keyword), &content_param_offsets));
  if (content_param_offsets.size() == 2) {
    leading_label_.SetText(keyword_hint.substr(0,
                                               content_param_offsets.front()));
    trailing_label_.SetText(keyword_hint.substr(content_param_offsets.front()));
  } else {
    // See comments on an identical NOTREACHED() in search_provider.cc.
    NOTREACHED();
  }
}

void LocationBarView::KeywordHintView::Paint(ChromeCanvas* canvas) {
  int image_x = leading_label_.IsVisible() ? leading_label_.width() : 0;

  // Since we paint the button image directly on the canvas (instead of using a
  // child view), we must mirror the button's position manually if the locale
  // is right-to-left.
  gfx::Rect tab_button_bounds(image_x,
                              kTabImageYOffset,
                              kTabButtonBitmap->width(),
                              kTabButtonBitmap->height());
  tab_button_bounds.set_x(MirroredLeftPointForRect(tab_button_bounds));
  canvas->DrawBitmapInt(*kTabButtonBitmap,
                        tab_button_bounds.x(),
                        tab_button_bounds.y());
}

gfx::Size LocationBarView::KeywordHintView::GetPreferredSize() {
  // TODO(sky): currently height doesn't matter, once baseline support is
  // added this should check baselines.
  gfx::Size prefsize = leading_label_.GetPreferredSize();
  int width = prefsize.width();
  width += kTabButtonBitmap->width();
  prefsize = trailing_label_.GetPreferredSize();
  width += prefsize.width();
  return gfx::Size(width, prefsize.height());
}

gfx::Size LocationBarView::KeywordHintView::GetMinimumSize() {
  // TODO(sky): currently height doesn't matter, once baseline support is
  // added this should check baselines.
  return gfx::Size(kTabButtonBitmap->width(), 0);
}

void LocationBarView::KeywordHintView::Layout() {
  // TODO(sky): baseline layout.
  bool show_labels = (width() != kTabButtonBitmap->width());

  leading_label_.SetVisible(show_labels);
  trailing_label_.SetVisible(show_labels);
  int x = 0;
  gfx::Size pref;

  if (show_labels) {
    pref = leading_label_.GetPreferredSize();
    leading_label_.SetBounds(x, 0, pref.width(), height());

    x += pref.width() + kTabButtonBitmap->width();
    pref = trailing_label_.GetPreferredSize();
    trailing_label_.SetBounds(x, 0, pref.width(), height());
  }
}

// We don't translate accelerators for ALT + numpad digit, they are used for
// entering special characters.
bool LocationBarView::ShouldLookupAccelerators(const views::KeyEvent& e) {
  if (!e.IsAltDown())
    return true;

  return !win_util::IsNumPadDigit(e.GetCharacter(), e.IsExtendedKey());
}

// ShowInfoBubbleTask-----------------------------------------------------------

class LocationBarView::ShowInfoBubbleTask : public Task {
 public:
  explicit ShowInfoBubbleTask(
      LocationBarView::LocationBarImageView* image_view);
  virtual void Run();
  void Cancel();

 private:
  LocationBarView::LocationBarImageView* image_view_;
  bool cancelled_;

  DISALLOW_EVIL_CONSTRUCTORS(ShowInfoBubbleTask);
};

LocationBarView::ShowInfoBubbleTask::ShowInfoBubbleTask(
    LocationBarView::LocationBarImageView* image_view)
    : cancelled_(false),
      image_view_(image_view) {
}

void LocationBarView::ShowInfoBubbleTask::Run() {
  if (cancelled_)
    return;

  if (!image_view_->GetWidget()->IsActive()) {
    // The browser is no longer active.  Let's not show the info bubble, this
    // would make the browser the active window again.  Also makes sure we NULL
    // show_info_bubble_task_ to prevent the SecurityImageView from keeping a
    // dangling pointer.
    image_view_->show_info_bubble_task_ = NULL;
    return;
  }

  image_view_->ShowInfoBubble();
}

void LocationBarView::ShowInfoBubbleTask::Cancel() {
  cancelled_ = true;
}

// -----------------------------------------------------------------------------

void LocationBarView::ShowFirstRunBubbleInternal() {
  if (!location_entry_view_)
    return;
  if (!location_entry_view_->GetWidget()->IsActive()) {
    // The browser is no longer active.  Let's not show the info bubble, this
    // would make the browser the active window again.
    return;
  }

  gfx::Point location;

  // If the UI layout is RTL, the coordinate system is not transformed and
  // therefore we need to adjust the X coordinate so that bubble appears on the
  // right hand side of the location bar.
  if (UILayoutIsRightToLeft())
    location.Offset(width(), 0);
  views::View::ConvertPointToScreen(this, &location);

  // We try to guess that 20 pixels offset is a good place for the first
  // letter in the OmniBox.
  gfx::Rect bounds(location.x(), location.y(), 20, height());

  // Moving the bounds "backwards" so that it appears within the location bar
  // if the UI layout is RTL.
  if (UILayoutIsRightToLeft())
    bounds.set_x(location.x() - 20);

  FirstRunBubble::Show(profile_,
      location_entry_view_->GetRootView()->GetWidget()->GetNativeView(),
      bounds);
}

// LocationBarImageView---------------------------------------------------------

LocationBarView::LocationBarImageView::LocationBarImageView()
  : show_info_bubble_task_(NULL),
    info_bubble_(NULL) {
}

LocationBarView::LocationBarImageView::~LocationBarImageView() {
  if (show_info_bubble_task_)
    show_info_bubble_task_->Cancel();

  if (info_bubble_) {
    // We are going to be invalid, make sure the InfoBubble does not keep a
    // pointer to us.
    info_bubble_->SetDelegate(NULL);
  }
}

void LocationBarView::LocationBarImageView::OnMouseMoved(
    const views::MouseEvent& event) {
  if (show_info_bubble_task_) {
    show_info_bubble_task_->Cancel();
    show_info_bubble_task_ = NULL;
  }

  if (info_bubble_) {
    // If an info bubble is currently showing, nothing to do.
    return;
  }

  show_info_bubble_task_ = new ShowInfoBubbleTask(this);
  MessageLoop::current()->PostDelayedTask(FROM_HERE, show_info_bubble_task_,
      kInfoBubbleHoverDelayMs);
}

void LocationBarView::LocationBarImageView::OnMouseExited(
    const views::MouseEvent& event) {
  if (show_info_bubble_task_) {
    show_info_bubble_task_->Cancel();
    show_info_bubble_task_ = NULL;
  }

  if (info_bubble_)
    info_bubble_->Close();
}

void LocationBarView::LocationBarImageView::InfoBubbleClosing(
    InfoBubble* info_bubble, bool closed_by_escape) {
  info_bubble_ = NULL;
}

void LocationBarView::LocationBarImageView::ShowInfoBubbleImpl(
    const std::wstring& text, SkColor text_color) {
  gfx::Point location;
  views::View::ConvertPointToScreen(this, &location);
  gfx::Rect bounds(location.x(), location.y(), width(), height());

  views::Label* label = new views::Label(text);
  label->SetMultiLine(true);
  label->SetColor(text_color);
  label->SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::BaseFont).DeriveFont(2));
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  label->SizeToFit(0);
  DCHECK(info_bubble_ == NULL);
  info_bubble_ = InfoBubble::Show(GetRootView()->GetWidget()->GetNativeView(),
                                  bounds, label, this);
  show_info_bubble_task_ = NULL;
}

// SecurityImageView------------------------------------------------------------

// static
SkBitmap* LocationBarView::SecurityImageView::lock_icon_ = NULL;
SkBitmap* LocationBarView::SecurityImageView::warning_icon_ = NULL;

LocationBarView::SecurityImageView::SecurityImageView(Profile* profile,
                                                      ToolbarModel* model)
  : LocationBarImageView(),
    profile_(profile),
    model_(model) {
  if (!lock_icon_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    lock_icon_ = rb.GetBitmapNamed(IDR_LOCK);
    warning_icon_ = rb.GetBitmapNamed(IDR_WARNING);
  }
  SetImageShown(LOCK);
}

LocationBarView::SecurityImageView::~SecurityImageView() {
}

void LocationBarView::SecurityImageView::SetImageShown(Image image) {
  switch (image) {
    case LOCK:
      ImageView::SetImage(lock_icon_);
      break;
    case WARNING:
      ImageView::SetImage(warning_icon_);
      break;
    default:
      NOTREACHED();
      break;
  }
}

bool LocationBarView::SecurityImageView::OnMousePressed(
    const views::MouseEvent& event) {
  NavigationEntry* nav_entry =
      BrowserList::GetLastActive()->GetSelectedTabContents()->
          controller()->GetActiveEntry();
  if (!nav_entry) {
    NOTREACHED();
    return true;
  }
  PageInfoWindow::CreatePageInfo(profile_,
                                 nav_entry,
                                 GetRootView()->GetWidget()->GetNativeView(),
                                 PageInfoWindow::SECURITY);
  return true;
}

void LocationBarView::SecurityImageView::ShowInfoBubble() {
  std::wstring text;
  SkColor text_color;
  model_->GetIconHoverText(&text, &text_color);

  ShowInfoBubbleImpl(text, text_color);
}

// RssImageView------------------------------------------------------------

// static
SkBitmap* LocationBarView::RssImageView::rss_icon_ = NULL;

LocationBarView::RssImageView::RssImageView(ToolbarModel* model)
  : model_(model),
    LocationBarImageView() {
  if (!rss_icon_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    rss_icon_ = rb.GetBitmapNamed(IDR_RSS_ICON);
  }
  ImageView::SetImage(rss_icon_);
}

LocationBarView::RssImageView::~RssImageView() {
}

bool LocationBarView::RssImageView::OnMousePressed(
    const views::MouseEvent& event) {
  NavigationEntry* entry =
      BrowserList::GetLastActive()->GetSelectedTabContents()->
      controller()->GetActiveEntry();
  if (!entry) {
    NOTREACHED();
    return true;
  }

  // Navigate to the first item in the feed list.
  scoped_refptr<FeedList> feeds = model_->GetFeedList();
  DCHECK(feeds.get() && feeds->list().size() > 0);

  // TODO(finnur): Make this do more than just display the XML in the browser.
  BrowserList::GetLastActive()->OpenURL(feeds->list()[0].url, GURL(),
                                        CURRENT_TAB, PageTransition::LINK);
  return true;
}

void LocationBarView::RssImageView::ShowInfoBubble() {
  // TODO(finnur): Get this string from the resources.
  std::wstring text = L"Subscribe to this feed";
  SkColor text_color = SK_ColorBLUE;
  ShowInfoBubbleImpl(text, text_color);
}

bool LocationBarView::OverrideAccelerator(
    const views::Accelerator& accelerator)  {
  return location_entry_->OverrideAccelerator(accelerator);
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView, LocationBar implementation:

void LocationBarView::ShowFirstRunBubble() {
  // We wait 30 milliseconds to open. It allows less flicker.
  Task* task = first_run_bubble_.NewRunnableMethod(
      &LocationBarView::ShowFirstRunBubbleInternal);
  MessageLoop::current()->PostDelayedTask(FROM_HERE, task, 30);
}

std::wstring LocationBarView::GetInputString() const {
  return location_input_;
}

WindowOpenDisposition LocationBarView::GetWindowOpenDisposition() const {
  return disposition_;
}

PageTransition::Type LocationBarView::GetPageTransition() const {
  return transition_;
}

void LocationBarView::AcceptInput() {
  location_entry_->model()->AcceptInput(CURRENT_TAB, false);
}

void LocationBarView::FocusLocation() {
  location_entry_->SetFocus();
  location_entry_->SelectAll(true);
}

void LocationBarView::FocusSearch() {
  location_entry_->SetUserText(L"?");
  location_entry_->SetFocus();
}

void LocationBarView::SaveStateToContents(TabContents* contents) {
  location_entry_->SaveStateToTab(contents);
}
