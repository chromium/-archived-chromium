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
#include "chrome/common/resource_bundle.h"
#include "chrome/views/widget/widget.h"
#include "grit/theme_resources.h"
#include "skia/include/SkShader.h"

static const SkColor kTransparentColor = SkColorSetARGB(0, 0, 0, 0);
static const int kPopupTransparency = 235;
static const int kHoverRowAlpha = 0x40;

class AutocompleteResultView : public views::View {
 public:
  AutocompleteResultView(AutocompleteResultViewModel* model,
                         int model_index,
                         const ChromeFont& font);
  virtual ~AutocompleteResultView();

  // Overridden from views::View:
  virtual void Paint(ChromeCanvas* canvas);
  virtual gfx::Size GetPreferredSize();
  virtual void OnMouseEntered(const views::MouseEvent& event);
  virtual void OnMouseMoved(const views::MouseEvent& event);
  virtual void OnMouseExited(const views::MouseEvent& event);
  virtual bool OnMousePressed(const views::MouseEvent& event);
  virtual void OnMouseReleased(const views::MouseEvent& event, bool canceled);
  virtual bool OnMouseDragged(const views::MouseEvent& event);

 private:
  // Paint the result view in different ways.
  void PaintAsSearchDefault(ChromeCanvas* canvas);
  void PaintAsURLSuggestion(ChromeCanvas* canvas);
  void PaintAsQuerySuggestion(ChromeCanvas* canvas);
  void PaintAsMoreRow(ChromeCanvas* canvas);

  // Get colors for row backgrounds and text for different row states.
  SkColor GetBackgroundColor() const;
  SkColor GetTextColor() const;

  // This row's model and model index.
  AutocompleteResultViewModel* model_;
  size_t model_index_;

  // True if the mouse is over this row.
  bool hot_;

  // The font used to derive fonts for rendering the text in this row.
  ChromeFont font_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteResultView);
};

AutocompleteResultView::AutocompleteResultView(
    AutocompleteResultViewModel* model,
    int model_index,
    const ChromeFont& font)
    : model_(model),
      model_index_(model_index),
      hot_(false),
      font_(font) {
}

AutocompleteResultView::~AutocompleteResultView() {
}

void AutocompleteResultView::Paint(ChromeCanvas* canvas) {
  canvas->FillRectInt(GetBackgroundColor(), 0, 0, width(), height());

  const AutocompleteMatch& match = model_->GetMatchAtIndex(model_index_);
  canvas->DrawStringInt(match.contents, font_, GetTextColor(), 0, 0, width(),
                        height());
}

gfx::Size AutocompleteResultView::GetPreferredSize() {
  return gfx::Size(0, 30);
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

void AutocompleteResultView::PaintAsSearchDefault(ChromeCanvas* canvas) {

}

void AutocompleteResultView::PaintAsURLSuggestion(ChromeCanvas* canvas) {

}

void AutocompleteResultView::PaintAsQuerySuggestion(ChromeCanvas* canvas) {

}

void AutocompleteResultView::PaintAsMoreRow(ChromeCanvas* canvas) {

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

void AutocompletePopupContentsView::SetAutocompleteResult(
    const AutocompleteResult& result) {
  RemoveAllChildViews(true);
  for (size_t i = 0; i < result.size(); ++i)
    AddChildView(new AutocompleteResultView(this, i, edit_font_));
  // Update the popup's size by calling Show. This will cause us to be laid out
  // again at the new size.
  popup_->Show();
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
  SetAutocompleteResult(result);
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

