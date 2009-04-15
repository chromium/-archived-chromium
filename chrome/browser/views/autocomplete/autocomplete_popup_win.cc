// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/views/autocomplete/autocomplete_popup_win.h"

#include <dwmapi.h>

#include "chrome/browser/autocomplete/autocomplete_edit_view_win.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/insets.h"
#include "chrome/common/gfx/path.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "grit/theme_resources.h"

// The stroke color around the popup border.
static const SkColor kEdgeColor = SkColorSetRGB(183, 195, 219);
static const int kPopupTransparency = 235;

class PopupBorder : public views::Border {
 public:
  PopupBorder() {
    InitClass();
  }
  virtual ~PopupBorder() {}

  // Returns the border radius of the edge of the popup.
  static int GetBorderRadius() {
    InitClass();
    return dropshadow_topleft_->width() - dropshadow_left_->width();
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

class AutocompletePopupViewContents : public views::View {
 public:
  AutocompletePopupViewContents() {
    set_border(new PopupBorder);
  }
  virtual ~AutocompletePopupViewContents() {}

  // Overridden from views::View:
  virtual void Paint(ChromeCanvas* canvas);
  virtual void Layout();

 private:
  DISALLOW_COPY_AND_ASSIGN(AutocompletePopupViewContents);
};

void AutocompletePopupViewContents::Paint(ChromeCanvas* canvas) {
  gfx::Rect contents_rect = GetLocalBounds(false);

  {
    SkPaint paint;
    SkRect rect;
    rect.set(SkIntToScalar(contents_rect.x()),
             SkIntToScalar(contents_rect.y()),
             SkIntToScalar(contents_rect.right()),
             SkIntToScalar(contents_rect.bottom()));

    SkScalar radius = SkIntToScalar(PopupBorder::GetBorderRadius());
    paint.setColor(SK_ColorWHITE);
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kFill_Style);
    canvas->drawRoundRect(rect, radius, radius, paint);
  }

  // Allow the window blur effect to show through the popup background.
  SkPaint paint;
  paint.setColor(SkColorSetARGB(kPopupTransparency, 255, 255, 255));
  paint.setPorterDuffXfermode(SkPorterDuff::kDstIn_Mode);
  paint.setStyle(SkPaint::kFill_Style);
  canvas->FillRectInt(contents_rect.x(), contents_rect.y(),
                      contents_rect.width(), contents_rect.height(), paint);

  // Paint the dropshadow.
  PaintBorder(canvas);
}

void AutocompletePopupViewContents::Layout() {
  // Provide a blurred background effect within the contents region of the
  // popup.
  DWM_BLURBEHIND bb = {0};
  bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
  bb.fEnable = true;

  gfx::Rect contents_rect = GetLocalBounds(false);
  gfx::Point origin(contents_rect.origin());
  views::View::ConvertPointToWidget(this, &origin);
  contents_rect.set_origin(origin);

  ScopedGDIObject<HRGN> popup_region;
  popup_region.Set(CreateRectRgn(contents_rect.x(), contents_rect.y(),
                                 contents_rect.right(),
                                 contents_rect.bottom()));
  bb.hRgnBlur = popup_region.Get();
  DwmEnableBlurBehindWindow(GetWidget()->GetNativeView(), &bb);
}

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupWin, public:

AutocompletePopupWin::AutocompletePopupWin(
    const ChromeFont& font,
    AutocompleteEditViewWin* edit_view,
    AutocompleteEditModel* edit_model,
    Profile* profile,
    AutocompletePopupPositioner* popup_positioner)
    : model_(new AutocompletePopupModel(this, edit_model, profile)),
      edit_view_(edit_view),
      popup_positioner_(popup_positioner),
      contents_(new AutocompletePopupViewContents) {
  set_delete_on_destroy(false);
  set_window_style(WS_POPUP | WS_CLIPCHILDREN);
  set_window_ex_style(WS_EX_TOOLWINDOW | WS_EX_LAYERED);
  contents_ = new AutocompletePopupViewContents;
}

AutocompletePopupWin::~AutocompletePopupWin() {
}

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupWin, AutocompletePopupView overrides:

bool AutocompletePopupWin::IsOpen() const {
  return IsWindow() && IsVisible();
}

void AutocompletePopupWin::InvalidateLine(size_t line) {
}

void AutocompletePopupWin::UpdatePopupAppearance() {
  const AutocompleteResult& result = model_->result();
  if (result.empty()) {
    // No matches, close any existing popup.
    if (::IsWindow(GetNativeView()))
      Hide();
    return;
  }

  gfx::Rect popup_bounds = GetPopupBounds();
  if (!::IsWindow(GetNativeView())) {
    // Create the popup
    HWND parent_hwnd = edit_view_->parent_view()->GetWidget()->GetNativeView();
    Init(parent_hwnd, popup_bounds, false);
    SetContentsView(contents_);

    // When an IME is attached to the rich-edit control, retrieve its window
    // handle and show this popup window under the IME windows.
    // Otherwise, show this popup window under top-most windows.
    // TODO(hbono): http://b/1111369 if we exclude this popup window from the
    // display area of IME windows, this workaround becomes unnecessary.
    HWND ime_window = ImmGetDefaultIMEWnd(edit_view_->m_hWnd);
    SetWindowPos(ime_window ? ime_window : HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
  } else {
    // Move the popup
    int flags = SWP_NOACTIVATE;
    if (!IsVisible())
      flags |= SWP_SHOWWINDOW;
    win_util::SetChildBounds(GetNativeView(), NULL, NULL, popup_bounds, 0,
                             flags);
  }
}

void AutocompletePopupWin::OnHoverEnabledOrDisabled(bool disabled) {
}

void AutocompletePopupWin::PaintUpdatesNow() {
}

AutocompletePopupModel* AutocompletePopupWin::GetModel() {
  return model_.get();
}

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupWin, private:

gfx::Rect AutocompletePopupWin::GetPopupBounds() const {
  gfx::Insets insets;
  contents_->border()->GetInsets(&insets);
  gfx::Rect contents_bounds = popup_positioner_->GetPopupBounds();
  contents_bounds.set_height(100); // TODO(beng): size to contents (once we have
                                   //             contents!)
  contents_bounds.Inset(-insets.left(), -insets.top(), -insets.right(),
                        -insets.bottom());
  return contents_bounds;
}
