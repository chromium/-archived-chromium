// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/views/autocomplete/autocomplete_popup_contents_view.h"

#include <dwmapi.h>

#include "chrome/browser/autocomplete/autocomplete_edit_view_win.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/views/autocomplete/autocomplete_popup_win.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/color_utils.h"
#include "chrome/common/gfx/insets.h"
#include "chrome/common/gfx/path.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/widget/widget.h"
#include "grit/theme_resources.h"
#include "skia/include/SkShader.h"
#include "third_party/icu38/public/common/unicode/ubidi.h"

static const SkColor kTransparentColor = SkColorSetARGB(0, 0, 0, 0);
static const SkColor kStandardURLColor = SkColorSetRGB(0, 0x80, 0);
static const SkColor kHighlightURLColor = SkColorSetRGB(0xD0, 0xFF, 0xD0);
static const int kPopupTransparency = 235;
static const int kHoverRowAlpha = 0x40;
// The minimum distance between the top and bottom of the icon and the top or
// bottom of the row. "Minimum" is used because the vertical padding may be
// larger, depending on the size of the text.
static const int kIconVerticalPadding = 2;
// The minimum distance between the top and bottom of the text and the top or
// bottom of the row. See comment about the use of "minimum" for
// kIconVerticalPadding.
static const int kTextVerticalPadding = 3;
// The padding at the left edge of the row, left of the icon.
static const int kRowLeftPadding = 6;
// The padding on the right edge of the row, right of the text.
static const int kRowRightPadding = 3;
// The horizontal distance between the right edge of the icon and the left edge
// of the text.
static const int kIconTextSpacing = 9;

class AutocompleteResultView : public views::View {
 public:
  AutocompleteResultView(AutocompleteResultViewModel* model,
                         int model_index,
                         const ChromeFont& font);
  virtual ~AutocompleteResultView();

  // Overridden from views::View:
  virtual void Paint(ChromeCanvas* canvas);
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();
  virtual void OnMouseEntered(const views::MouseEvent& event);
  virtual void OnMouseMoved(const views::MouseEvent& event);
  virtual void OnMouseExited(const views::MouseEvent& event);
  virtual bool OnMousePressed(const views::MouseEvent& event);
  virtual void OnMouseReleased(const views::MouseEvent& event, bool canceled);
  virtual bool OnMouseDragged(const views::MouseEvent& event);

 private:
  // Get colors for row backgrounds and text for different row states.
  SkColor GetBackgroundColor() const;
  SkColor GetTextColor() const;

  SkBitmap* GetIcon() const;

  // Draws the specified |text| into the canvas, using highlighting provided by
  // |classifications|.
  void DrawString(ChromeCanvas* canvas,
                  const std::wstring& text,
                  const ACMatchClassifications& classifications,
                  int x,
                  int y);

  // Draws an individual sub-fragment with the specified style.
  int DrawStringFragment(ChromeCanvas* canvas,
                         const std::wstring& text,
                         int style,
                         int x,
                         int y);

  // Gets the font and text color for a fragment with the specified style.
  ChromeFont GetFragmentFont(int style) const;
  SkColor GetFragmentTextColor(int style) const;

  // This row's model and model index.
  AutocompleteResultViewModel* model_;
  size_t model_index_;

  // True if the mouse is over this row.
  bool hot_;

  // The font used to derive fonts for rendering the text in this row.
  ChromeFont font_;

  // A context used for mirroring regions.
  class MirroringContext;
  scoped_ptr<MirroringContext> mirroring_context_;

  // Layout rects for various sub-components of the view.
  gfx::Rect icon_bounds_;
  gfx::Rect text_bounds_;

  // Icons for rows.
  static SkBitmap* icon_url_;
  static SkBitmap* icon_history_;
  static SkBitmap* icon_search_;
  static SkBitmap* icon_more_;
  static SkBitmap* icon_star_;
  static int icon_size_;
  
  static bool initialized_;
  static void InitClass();

  DISALLOW_COPY_AND_ASSIGN(AutocompleteResultView);
};

// static
SkBitmap* AutocompleteResultView::icon_url_ = NULL;
SkBitmap* AutocompleteResultView::icon_history_ = NULL;
SkBitmap* AutocompleteResultView::icon_search_ = NULL;
SkBitmap* AutocompleteResultView::icon_star_ = NULL;
SkBitmap* AutocompleteResultView::icon_more_ = NULL;
int AutocompleteResultView::icon_size_ = 0;
bool AutocompleteResultView::initialized_ = false;

// This class implements a utility used for mirroring x-coordinates when the
// application language is a right-to-left one.
class AutocompleteResultView::MirroringContext {
 public:
  MirroringContext() : min_x_(0), center_x_(0), max_x_(0), enabled_(false) { }

  // Initializes the bounding region used for mirroring coordinates.
  // This class uses the center of this region as an axis for calculating
  // mirrored coordinates.
  void Initialize(int x1, int x2, bool enabled);

  // Return the "left" side of the specified region.
  // When the application language is a right-to-left one, this function
  // calculates the mirrored coordinates of the input region and returns the
  // left side of the mirrored region.
  // The input region must be in the bounding region specified in the
  // Initialize() function.
  int GetLeft(int x1, int x2) const;

  // Returns whether or not we are mirroring the x coordinate.
  bool enabled() const {
    return enabled_;
  }

 private:
  int min_x_;
  int center_x_;
  int max_x_;
  bool enabled_;

  DISALLOW_EVIL_CONSTRUCTORS(MirroringContext);
};

void AutocompleteResultView::MirroringContext::Initialize(int x1, int x2,
                                                          bool enabled) {
  min_x_ = std::min(x1, x2);
  max_x_ = std::max(x1, x2);
  center_x_ = min_x_ + (max_x_ - min_x_) / 2;
  enabled_ = enabled;
}

int AutocompleteResultView::MirroringContext::GetLeft(int x1, int x2) const {
  return enabled_ ?
      (center_x_ + (center_x_ - std::max(x1, x2))) : std::min(x1, x2);
}

AutocompleteResultView::AutocompleteResultView(
    AutocompleteResultViewModel* model,
    int model_index,
    const ChromeFont& font)
    : model_(model),
      model_index_(model_index),
      hot_(false),
      font_(font),
      mirroring_context_(new MirroringContext()) {
  InitClass();
}

AutocompleteResultView::~AutocompleteResultView() {
}

void AutocompleteResultView::Paint(ChromeCanvas* canvas) {
  canvas->FillRectInt(GetBackgroundColor(), 0, 0, width(), height());

  // Paint the icon.
  canvas->DrawBitmapInt(*GetIcon(), icon_bounds_.x(), icon_bounds_.y());

  // Paint the text.
  const AutocompleteMatch& match = model_->GetMatchAtIndex(model_index_);
  DrawString(canvas, match.contents, match.contents_class,
             text_bounds_.x(), text_bounds_.y());

  // Paint the description.
  // TODO(beng): do this.
}

void AutocompleteResultView::Layout() {
  icon_bounds_.SetRect(kRowLeftPadding, (height() - icon_size_) / 2,
                       icon_size_, icon_size_);
  int text_x = icon_bounds_.right() + kIconTextSpacing;
  text_bounds_.SetRect(text_x, (height() - font_.height()) / 2,
                       bounds().right() - text_x - kRowRightPadding,
                       font_.height());
}

gfx::Size AutocompleteResultView::GetPreferredSize() {
  int text_height = font_.height() + 2 * kTextVerticalPadding;
  int icon_height = icon_size_ + 2 * kIconVerticalPadding;
  return gfx::Size(0, std::max(icon_height, text_height));
}

void AutocompleteResultView::OnMouseEntered(const views::MouseEvent& event) {
  hot_ = true;
  SchedulePaint();
}

void AutocompleteResultView::OnMouseMoved(const views::MouseEvent& event) {
  if (!hot_) {
    hot_ = true;
    SchedulePaint();
  }
}

void AutocompleteResultView::OnMouseExited(const views::MouseEvent& event) {
  hot_ = false;
  SchedulePaint();
}

bool AutocompleteResultView::OnMousePressed(const views::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton()) {
    model_->SetHoveredLine(model_index_);
    model_->SetSelectedLine(model_index_, false);
  } else if (event.IsOnlyMiddleMouseButton()) {
    model_->SetHoveredLine(model_index_);
  }
  return true;
}

void AutocompleteResultView::OnMouseReleased(const views::MouseEvent& event,
                                             bool canceled) {
  if (canceled)
    return;
  if (event.IsOnlyMiddleMouseButton())
    model_->OpenIndex(model_index_, NEW_BACKGROUND_TAB);
  else if (event.IsOnlyLeftMouseButton())
    model_->OpenIndex(model_index_, CURRENT_TAB);
}

bool AutocompleteResultView::OnMouseDragged(const views::MouseEvent& event) {
  // TODO(beng): move all message handling into the contents view and override
  //             GetViewForPoint.
  return false;
}

SkColor AutocompleteResultView::GetBackgroundColor() const {
  if (model_->IsSelectedIndex(model_index_))
    return color_utils::GetSysSkColor(COLOR_HIGHLIGHT);
  if (hot_) {
    COLORREF color = GetSysColor(COLOR_HIGHLIGHT);
    return SkColorSetARGB(kHoverRowAlpha, GetRValue(color), GetGValue(color),
                          GetBValue(color));
  }
  return kTransparentColor;
}

SkColor AutocompleteResultView::GetTextColor() const {
  if (model_->IsSelectedIndex(model_index_))
    return color_utils::GetSysSkColor(COLOR_HIGHLIGHTTEXT);
  return color_utils::GetSysSkColor(COLOR_WINDOWTEXT);
}

SkBitmap* AutocompleteResultView::GetIcon() const {
  switch (model_->GetMatchAtIndex(model_index_).type) {
    case AutocompleteMatch::URL_WHAT_YOU_TYPED:
    case AutocompleteMatch::HISTORY_URL:
    case AutocompleteMatch::NAVSUGGEST:
      return icon_url_;
    case AutocompleteMatch::HISTORY_TITLE:
    case AutocompleteMatch::HISTORY_BODY:
    case AutocompleteMatch::HISTORY_KEYWORD:
      return icon_history_;
    case AutocompleteMatch::SEARCH_WHAT_YOU_TYPED:
    case AutocompleteMatch::SEARCH_HISTORY:
    case AutocompleteMatch::SEARCH_SUGGEST:
    case AutocompleteMatch::SEARCH_OTHER_ENGINE:
      return icon_search_;
    case AutocompleteMatch::OPEN_HISTORY_PAGE:
      return icon_more_;
    default:
      NOTREACHED();
      break;
  }
  return NULL;
}

void AutocompleteResultView::DrawString(
    ChromeCanvas* canvas,
    const std::wstring& text,
    const ACMatchClassifications& classifications,
    int x,
    int y) {
  if (!text.length())
    return;

  // Check whether or not this text is a URL string.
  // A URL string is basically in English with possible included words in
  // Arabic or Hebrew. For such case, ICU provides a special algorithm and we
  // should use it.
  bool url = false;
  for (ACMatchClassifications::const_iterator i = classifications.begin();
       i != classifications.end(); ++i) {
    if (i->style & ACMatchClassification::URL)
      url = true;
  }

  // Initialize a bidirectional line iterator of ICU and split the text into
  // visual runs. (A visual run is consecutive characters which have the same
  // display direction and should be displayed at once.)
  l10n_util::BiDiLineIterator bidi_line;
  if (!bidi_line.Open(text, mirroring_context_->enabled(), url))
    return;
  const int runs = bidi_line.CountRuns();

  // Draw the visual runs.
  // This loop splits each run into text fragments with the given
  // classifications and draws the text fragments.
  // When the direction of a run is right-to-left, we have to mirror the
  // x-coordinate of this run and render the fragments in the right-to-left
  // reading order. To handle this display order independently from the one of
  // this popup window, this loop renders a run with the steps below:
  // 1. Create a local display context for each run;
  // 2. Render the run into the local display context, and;
  // 3. Copy the local display context to the one of the popup window.
  int run_x = x;
  for (int run = 0; run < runs; ++run) {
    int run_start = 0;
    int run_length = 0;

    // The index we pass to GetVisualRun corresponds to the position of the run
    // in the displayed text. For example, the string "Google in HEBREW" (where
    // HEBREW is text in the Hebrew language) has two runs: "Google in " which
    // is an LTR run, and "HEBREW" which is an RTL run. In an LTR context, the
    // run "Google in " has the index 0 (since it is the leftmost run
    // displayed). In an RTL context, the same run has the index 1 because it
    // is the rightmost run. This is why the order in which we traverse the
    // runs is different depending on the locale direction.
    //
    // Note that for URLs we always traverse the runs from lower to higher
    // indexes because the return order of runs for a URL always matches the
    // physical order of the context.
    int current_run =
        (mirroring_context_->enabled() && !url) ? (runs - run - 1) : run;
    const UBiDiDirection run_direction = bidi_line.GetVisualRun(current_run,
                                                                &run_start,
                                                                &run_length);
    const int run_end = run_start + run_length;

    // Split this run with the given classifications and draw the fragments
    // into the local display context.
    for (ACMatchClassifications::const_iterator i = classifications.begin();
         i != classifications.end(); ++i) {
      const int text_start = std::max(run_start, static_cast<int>(i->offset));
      const int text_end = std::min(run_end, (i != classifications.end() - 1) ?
          static_cast<int>((i + 1)->offset) : run_end);
      x += DrawStringFragment(canvas,
                              text.substr(text_start, text_end - text_start),
                              i->style, x, y);
    }
  }
}

int AutocompleteResultView::DrawStringFragment(
    ChromeCanvas* canvas,
    const std::wstring& text,
    int style,
    int x,
    int y) {
  ChromeFont display_font = GetFragmentFont(style);
  int string_width = display_font.GetStringWidth(text);
  canvas->DrawStringInt(text, GetFragmentFont(style),
                        GetFragmentTextColor(style), x, y, string_width,
                        display_font.height());
  return string_width;
}

ChromeFont AutocompleteResultView::GetFragmentFont(int style) const {
  if (style & ACMatchClassification::MATCH)
    return font_.DeriveFont(0, ChromeFont::BOLD);
  return font_;
}

SkColor AutocompleteResultView::GetFragmentTextColor(int style) const {
  if (style & ACMatchClassification::URL) {
    // TODO(beng): bring over the contrast logic from the old popup and massage
    //             these values. See autocomplete_popup_view_win.cc and
    //             LuminosityContrast etc.
    return model_->IsSelectedIndex(model_index_) ? kHighlightURLColor
                                                 : kStandardURLColor;
  }

  if (style & ACMatchClassification::DIM)
    return SkColorSetA(GetTextColor(), 0xAA);
  return GetTextColor();
}

void AutocompleteResultView::InitClass() {
  if (!initialized_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    icon_url_ = rb.GetBitmapNamed(IDR_O2_GLOBE);
    icon_history_ = rb.GetBitmapNamed(IDR_O2_HISTORY);
    icon_search_ = rb.GetBitmapNamed(IDR_O2_SEARCH);
    icon_star_ = rb.GetBitmapNamed(IDR_O2_STAR);
    icon_more_ = rb.GetBitmapNamed(IDR_O2_MORE);
    // All icons are assumed to be square, and the same size.
    icon_size_ = icon_url_->width();
    initialized_ = true;
  }
}

class PopupBorder : public views::Border {
 public:
  PopupBorder() {
    InitClass();
  }
  virtual ~PopupBorder() {}

  // Returns the border radius of the edge of the popup.
  static int GetBorderRadius() {
    InitClass();
    return dropshadow_topleft_->width() - dropshadow_left_->width() - 1;
  }

  // Overridden from views::Border:
  virtual void Paint(const views::View& view, ChromeCanvas* canvas) const;
  virtual void GetInsets(gfx::Insets* insets) const;

 private:
  // Border graphics.
  static SkBitmap* dropshadow_left_;
  static SkBitmap* dropshadow_topleft_;
  static SkBitmap* dropshadow_top_;
  static SkBitmap* dropshadow_topright_;
  static SkBitmap* dropshadow_right_;
  static SkBitmap* dropshadow_bottomright_;
  static SkBitmap* dropshadow_bottom_;
  static SkBitmap* dropshadow_bottomleft_;

  static void InitClass();

  DISALLOW_COPY_AND_ASSIGN(PopupBorder);
};

// static
SkBitmap* PopupBorder::dropshadow_left_ = NULL;
SkBitmap* PopupBorder::dropshadow_topleft_ = NULL;
SkBitmap* PopupBorder::dropshadow_top_ = NULL;
SkBitmap* PopupBorder::dropshadow_topright_ = NULL;
SkBitmap* PopupBorder::dropshadow_right_ = NULL;
SkBitmap* PopupBorder::dropshadow_bottomright_ = NULL;
SkBitmap* PopupBorder::dropshadow_bottom_ = NULL;
SkBitmap* PopupBorder::dropshadow_bottomleft_ = NULL;

void PopupBorder::Paint(const views::View& view, ChromeCanvas* canvas) const {
  int ds_tl_width = dropshadow_topleft_->width();
  int ds_tl_height = dropshadow_topleft_->height();
  int ds_tr_width = dropshadow_topright_->width();
  int ds_tr_height = dropshadow_topright_->height();
  int ds_br_width = dropshadow_bottomright_->width();
  int ds_br_height = dropshadow_bottomright_->height();
  int ds_bl_width = dropshadow_bottomleft_->width();
  int ds_bl_height = dropshadow_bottomleft_->height();

  canvas->DrawBitmapInt(*dropshadow_topleft_, 0, 0);
  canvas->TileImageInt(*dropshadow_top_, ds_tl_width, 0,
                       view.width() - ds_tr_width - ds_tl_width,
                       dropshadow_top_->height());
  canvas->DrawBitmapInt(*dropshadow_topright_, view.width() - ds_tr_width, 0);
  canvas->TileImageInt(*dropshadow_right_,
                       view.width() - dropshadow_right_->width(),
                       ds_tr_height, dropshadow_right_->width(),
                       view.height() - ds_tr_height - ds_br_height);
  canvas->DrawBitmapInt(*dropshadow_bottomright_, view.width() - ds_br_width,
                        view.height() - ds_br_height);
  canvas->TileImageInt(*dropshadow_bottom_, ds_bl_width,
                       view.height() - dropshadow_bottom_->height(),
                       view.width() - ds_bl_width - ds_br_width,
                       dropshadow_bottom_->height());
  canvas->DrawBitmapInt(*dropshadow_bottomleft_, 0,
                        view.height() - dropshadow_bottomleft_->height());
  canvas->TileImageInt(*dropshadow_left_, 0, ds_tl_height,
                       dropshadow_left_->width(),
                       view.height() - ds_tl_height - ds_bl_height);
}

void PopupBorder::GetInsets(gfx::Insets* insets) const {
  // The left, right and bottom edge image sizes define our insets. The corner
  // images don't determine this because they can extend in both directions.
  insets->Set(dropshadow_top_->height(), dropshadow_left_->width(),
              dropshadow_bottom_->height(), dropshadow_right_->width());
}

void PopupBorder::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    dropshadow_left_ = rb.GetBitmapNamed(IDR_OMNIBOX_POPUP_DS_L);
    dropshadow_topleft_ = rb.GetBitmapNamed(IDR_OMNIBOX_POPUP_DS_TL);
    dropshadow_top_ = rb.GetBitmapNamed(IDR_OMNIBOX_POPUP_DS_T);
    dropshadow_topright_ = rb.GetBitmapNamed(IDR_OMNIBOX_POPUP_DS_TR);
    dropshadow_right_ = rb.GetBitmapNamed(IDR_OMNIBOX_POPUP_DS_R);
    dropshadow_bottomright_ = rb.GetBitmapNamed(IDR_OMNIBOX_POPUP_DS_BR);
    dropshadow_bottom_ = rb.GetBitmapNamed(IDR_OMNIBOX_POPUP_DS_B);
    dropshadow_bottomleft_ = rb.GetBitmapNamed(IDR_OMNIBOX_POPUP_DS_BL);
    initialized = true;
  }
}

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupContentsView, public:

AutocompletePopupContentsView::AutocompletePopupContentsView(
    const ChromeFont& font,
    AutocompleteEditViewWin* edit_view,
    AutocompleteEditModel* edit_model,
    Profile* profile,
    AutocompletePopupPositioner* popup_positioner)
    : popup_(new AutocompletePopupWin(this)),
      model_(new AutocompletePopupModel(this, edit_model, profile)),
      edit_view_(edit_view),
      popup_positioner_(popup_positioner),
      edit_font_(font) {
  set_border(new PopupBorder);
}

void AutocompletePopupContentsView::UpdateResultViewsFromResult(
    const AutocompleteResult& result) {
  RemoveAllChildViews(true);
  for (size_t i = 0; i < result.size(); ++i)
    AddChildView(new AutocompleteResultView(this, i, edit_font_));
}

gfx::Rect AutocompletePopupContentsView::GetPopupBounds() const {
  gfx::Insets insets;
  border()->GetInsets(&insets);
  gfx::Rect contents_bounds = popup_positioner_->GetPopupBounds();
  int child_count = GetChildViewCount();
  int height = 0;
  for (int i = 0; i < child_count; ++i)
    height += GetChildViewAt(i)->GetPreferredSize().height();
  contents_bounds.set_height(height);
  contents_bounds.Inset(-insets.left(), -insets.top(), -insets.right(),
                        -insets.bottom());
  return contents_bounds;
}

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupContentsView, AutocompletePopupView overrides:

bool AutocompletePopupContentsView::IsOpen() const {
  return popup_->IsWindow() && popup_->IsVisible();
}

void AutocompletePopupContentsView::InvalidateLine(size_t line) {
  GetChildViewAt(static_cast<int>(line))->SchedulePaint();
}

void AutocompletePopupContentsView::UpdatePopupAppearance() {
  const AutocompleteResult& result = model_->result();
  UpdateResultViewsFromResult(result);
  if (result.empty()) {
    // No matches, close any existing popup.
    if (popup_->IsWindow())
      popup_->Hide();
    return;
  }

  if (popup_->IsWindow())
    popup_->Show();
  else
    popup_->Init(edit_view_, this);
}

void AutocompletePopupContentsView::OnHoverEnabledOrDisabled(bool disabled) {
  // TODO(beng): remove this from the interface.
}

void AutocompletePopupContentsView::PaintUpdatesNow() {
  // TODO(beng): remove this from the interface.
}

AutocompletePopupModel* AutocompletePopupContentsView::GetModel() {
  return model_.get();
}

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupContentsView, AutocompleteResultViewModel implementation:

bool AutocompletePopupContentsView::IsSelectedIndex(size_t index) {
  return index == model_->selected_line();
}

const AutocompleteMatch& AutocompletePopupContentsView::GetMatchAtIndex(
    size_t index) {
  return model_->result().match_at(index);
}

void AutocompletePopupContentsView::OpenIndex(
    size_t index,
    WindowOpenDisposition disposition) {
  const AutocompleteMatch& match = model_->result().match_at(index);
  // OpenURL() may close the popup, which will clear the result set and, by
  // extension, |match| and its contents.  So copy the relevant strings out to
  // make sure they stay alive until the call completes.
  const GURL url(match.destination_url);
  std::wstring keyword;
  const bool is_keyword_hint = model_->GetKeywordForMatch(match, &keyword);
  edit_view_->OpenURL(url, disposition, match.transition, GURL(), index,
                      is_keyword_hint ? std::wstring() : keyword);
}

void AutocompletePopupContentsView::SetHoveredLine(size_t index) {
  model_->SetHoveredLine(index);
}

void AutocompletePopupContentsView::SetSelectedLine(size_t index,
                                                    bool revert_to_default) {
  model_->SetSelectedLine(index, revert_to_default);
}

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupContentsView, views::View overrides:

void AutocompletePopupContentsView::PaintChildren(ChromeCanvas* canvas) {
  // We paint our children in an unconventional way.
  //
  // Because the border of this view creates an anti-aliased round-rect region
  // for the contents, we need to render our rectangular result child views into
  // this round rect region. We can't use a simple clip because clipping is
  // 1-bit and we get nasty jagged edges.
  //
  // Instead, we paint all our children into a second canvas and use that as a
  // shader to fill a path representing the round-rect clipping region. This
  // yields a nice anti-aliased edge.
  gfx::Rect contents_rect = GetLocalBounds(false);
  ChromeCanvas contents_canvas(contents_rect.width(), contents_rect.height(),
                               true);
  contents_canvas.FillRectInt(color_utils::GetSysSkColor(COLOR_WINDOW), 0, 0,
                              contents_rect.width(), contents_rect.height());
  View::PaintChildren(&contents_canvas);
  // We want the contents background to be slightly transparent so we can see
  // the blurry glass effect on DWM systems behind. We do this _after_ we paint
  // the children since they paint text, and GDI will reset this alpha data if
  // we paint text after this call.
  MakeCanvasTransparent(&contents_canvas);

  // Now paint the contents of the contents canvas into the actual canvas.
  SkPaint paint;
  paint.setAntiAlias(true);

  SkShader* shader = SkShader::CreateBitmapShader(
      contents_canvas.getDevice()->accessBitmap(false),
      SkShader::kClamp_TileMode,
      SkShader::kClamp_TileMode);
  paint.setShader(shader);
  shader->unref();

  gfx::Path path;
  MakeContentsPath(&path, contents_rect);
  canvas->drawPath(path, paint);
}

void AutocompletePopupContentsView::Layout() {
  UpdateBlurRegion();

  // Size our children to the available content area.
  gfx::Rect contents_rect = GetLocalBounds(false);
  int child_count = GetChildViewCount();
  int top = contents_rect.y();
  for (int i = 0; i < child_count; ++i) {
    View* v = GetChildViewAt(i);
    v->SetBounds(contents_rect.x(), top, contents_rect.width(),
                 v->GetPreferredSize().height());
    top = v->bounds().bottom();
  }

  // We need to manually schedule a paint here since we are a layered window and
  // won't implicitly require painting until we ask for one.
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupContentsView, private:

void AutocompletePopupContentsView::MakeContentsPath(
    gfx::Path* path,
    const gfx::Rect& bounding_rect) {
  SkRect rect;
  rect.set(SkIntToScalar(bounding_rect.x()),
           SkIntToScalar(bounding_rect.y()),
           SkIntToScalar(bounding_rect.right()),
           SkIntToScalar(bounding_rect.bottom()));

  SkScalar radius = SkIntToScalar(PopupBorder::GetBorderRadius());
  path->addRoundRect(rect, radius, radius);
}

void AutocompletePopupContentsView::UpdateBlurRegion() {
  // Provide a blurred background effect within the contents region of the
  // popup.
  DWM_BLURBEHIND bb = {0};
  bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
  bb.fEnable = true;

  // Translate the contents rect into widget coordinates, since that's what
  // DwmEnableBlurBehindWindow expects a region in.
  gfx::Rect contents_rect = GetLocalBounds(false);
  gfx::Point origin(contents_rect.origin());
  views::View::ConvertPointToWidget(this, &origin);
  contents_rect.set_origin(origin);

  gfx::Path contents_path;
  MakeContentsPath(&contents_path, contents_rect);
  ScopedGDIObject<HRGN> popup_region;
  popup_region.Set(contents_path.CreateHRGN());
  bb.hRgnBlur = popup_region.Get();
  DwmEnableBlurBehindWindow(GetWidget()->GetNativeView(), &bb);
}

void AutocompletePopupContentsView::MakeCanvasTransparent(
    ChromeCanvas* canvas) {
  // Allow the window blur effect to show through the popup background.
  SkPaint paint;
  paint.setColor(SkColorSetARGB(kPopupTransparency, 255, 255, 255));
  paint.setPorterDuffXfermode(SkPorterDuff::kDstIn_Mode);
  paint.setStyle(SkPaint::kFill_Style);
  canvas->FillRectInt(0, 0, canvas->getDevice()->width(),
                      canvas->getDevice()->height(), paint);
}

