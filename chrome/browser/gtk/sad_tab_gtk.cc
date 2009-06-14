// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/sad_tab_gtk.h"

#include <string>

#include "app/gfx/canvas_paint.h"
#include "app/gfx/font.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/gfx/size.h"
#include "base/lazy_instance.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils.h"
#include "third_party/skia/include/effects/SkGradientShader.h"

namespace {

// The y offset from the center at which to paint the icon.
const int kSadTabOffset = -64;
// The spacing between the icon and the title.
const int kIconTitleSpacing = 20;
// The spacing between the title and the message.
const int kTitleMessageSpacing = 15;
const SkColor kTitleTextColor = SK_ColorWHITE;
const SkColor kMessageTextColor = SK_ColorWHITE;
const SkColor kBackgroundColor = SkColorSetRGB(35, 48, 64);
const SkColor kBackgroundEndColor = SkColorSetRGB(35, 48, 64);

struct SadTabGtkConstants {
  SadTabGtkConstants()
      : sad_tab_bitmap(
          ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_SAD_TAB)),
        title_font(
            ResourceBundle::GetSharedInstance().GetFont(
                ResourceBundle::BaseFont).DeriveFont(2, gfx::Font::BOLD)),
        message_font(
            ResourceBundle::GetSharedInstance().GetFont(
                ResourceBundle::BaseFont).DeriveFont(1)),
        title(l10n_util::GetString(IDS_SAD_TAB_TITLE)),
        message(l10n_util::GetString(IDS_SAD_TAB_MESSAGE)) {}

  const SkBitmap* sad_tab_bitmap;
  gfx::Font title_font;
  gfx::Font message_font;
  std::wstring title;
  std::wstring message;
};

base::LazyInstance<SadTabGtkConstants>
    g_sad_tab_constants(base::LINKER_INITIALIZED);

}  // namespace

SadTabGtk::SadTabGtk()
  : width_(0),
    height_(0),
    title_y_(0),
    message_y_(0),
    widget_(gtk_drawing_area_new()) {
  gtk_widget_set_double_buffered(widget_.get(), FALSE);
  g_signal_connect(widget_.get(), "expose-event",
                   G_CALLBACK(OnExposeThunk), this);
  g_signal_connect(widget_.get(), "configure-event",
                   G_CALLBACK(OnConfigureThunk), this);
}

SadTabGtk::~SadTabGtk() {
  widget_.Destroy();
}

// static
gboolean SadTabGtk::OnExposeThunk(GtkWidget* widget,
                                  GdkEventExpose* event,
                                  const SadTabGtk* sad_tab) {
  return sad_tab->OnExpose(widget, event);
}

gboolean SadTabGtk::OnExpose(GtkWidget* widget, GdkEventExpose* event) const {
  gfx::CanvasPaint canvas(event);
  SkPaint paint;
  paint.setShader(skia::CreateGradientShader(0, height_,
                                             kBackgroundColor,
                                             kBackgroundEndColor))->safeUnref();
  paint.setStyle(SkPaint::kFill_Style);
  canvas.drawRectCoords(0, 0,
                        SkIntToScalar(width_),
                        SkIntToScalar(height_),
                        paint);

  const SadTabGtkConstants& sad_tab_constants =
      g_sad_tab_constants.Get();

  // Paint the sad tab icon.
  canvas.DrawBitmapInt(*sad_tab_constants.sad_tab_bitmap,
                       icon_bounds_.x(),
                       icon_bounds_.y());

  // Paint the "Aw, snap!"
  canvas.DrawStringInt(sad_tab_constants.title,
                       sad_tab_constants.title_font,
                       kTitleTextColor,
                       0,
                       title_y_,
                       width_,
                       sad_tab_constants.title_font.height(),
                       gfx::Canvas::TEXT_ALIGN_CENTER);

  // Paint the explanatory message.
  canvas.DrawStringInt(
      sad_tab_constants.message,
      sad_tab_constants.message_font,
      kMessageTextColor,
      0,
      message_y_,
      width_,
      99999,  // Let the height be large, and we'll clip if needed.
      gfx::Canvas::TEXT_ALIGN_CENTER |
      gfx::Canvas::MULTI_LINE |
      gfx::Canvas::TEXT_VALIGN_TOP);

  return TRUE;
}

// static
gboolean SadTabGtk::OnConfigureThunk(GtkWidget* widget,
                                     GdkEventConfigure* event,
                                     SadTabGtk* sad_tab) {
  return sad_tab->OnConfigure(widget, event);
}

gboolean SadTabGtk::OnConfigure(GtkWidget* widget, GdkEventConfigure* event) {
  const SadTabGtkConstants& sad_tab_constants =
      g_sad_tab_constants.Get();
  width_ = event->width;
  height_= event->height;
  int icon_width = sad_tab_constants.sad_tab_bitmap->width();
  int icon_height = sad_tab_constants.sad_tab_bitmap->height();
  int icon_x = (event->width - icon_width) / 2;
  int icon_y = ((event->height - icon_height) / 2) + kSadTabOffset;
  icon_bounds_.SetRect(icon_x, icon_y, icon_width, icon_height);

  title_y_ =
      icon_bounds_.bottom() + kIconTitleSpacing;
  int title_height = sad_tab_constants.title_font.height();
  message_y_ =
      title_y_ + title_height + kTitleMessageSpacing;
  return TRUE;
}
