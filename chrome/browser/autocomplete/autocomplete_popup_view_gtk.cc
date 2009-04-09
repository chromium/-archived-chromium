// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_popup_view_gtk.h"

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/gfx/gtk_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view_gtk.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/notification_service.h"

namespace {

const GdkColor kBorderColor = GDK_COLOR_RGB(0xc7, 0xca, 0xce);
const GdkColor kBackgroundColor = GDK_COLOR_RGB(0xff, 0xff, 0xff);
const GdkColor kSelectedBackgroundColor = GDK_COLOR_RGB(0xdf, 0xe6, 0xf6);

const int kBorderThickness = 1;
const int kHeightPerResult = 20;
// Additional distance below the edit control.
const int kTopMargin = 3;
// Space between edge and the text.  This includes the border, so the text
// will be kLeftRightPadding - kBorderThickness away from the border.
const int kLeftRightPadding = 3;

// TODO(deanm): We should put this on ChromeFont so it can be shared.
// Returns a new pango font, free with pango_font_description_free().
PangoFontDescription* PangoFontFromChromeFont(const ChromeFont& chrome_font) {
  ChromeFont font = chrome_font;  // Copy so we can call non-const methods.
  PangoFontDescription* pfd = pango_font_description_new();
  pango_font_description_set_family(pfd, WideToUTF8(font.FontName()).c_str());
  pango_font_description_set_size(pfd, font.FontSize() * PANGO_SCALE);

  switch (font.style()) {
    case ChromeFont::NORMAL:
      // Nothing to do, should already be PANGO_STYLE_NORMAL.
      break;
    case ChromeFont::BOLD:
      pango_font_description_set_weight(pfd, PANGO_WEIGHT_BOLD);
      break;
    case ChromeFont::ITALIC:
      pango_font_description_set_style(pfd, PANGO_STYLE_ITALIC);
      break;
    case ChromeFont::UNDERLINED:
      // TODO(deanm): How to do underlined?  Where do we use it?  Probably have
      // to paint it ourselves, see pango_font_metrics_get_underline_position.
      break;
  }

  return pfd;
}

// Return a GdkRectangle covering the whole area of |window|.
GdkRectangle GetWindowRect(GdkWindow* window) {
  gint width, height;
  gdk_drawable_get_size(GDK_DRAWABLE(window), &width, &height);
  GdkRectangle rect = {0, 0, width, height};
  return rect;
}

// Return a rectangle for the space for a result.  This excludes the border,
// but includes the padding.  This is the area that is colored for a selection.
GdkRectangle GetRectForLine(size_t line, int width) {
  GdkRectangle rect = {kBorderThickness,
                       (line * kHeightPerResult) + kBorderThickness,
                       width - (kBorderThickness * 2),
                       kHeightPerResult};
  return rect;
}

}  // namespace

AutocompletePopupViewGtk::AutocompletePopupViewGtk(
    AutocompleteEditViewGtk* edit_view,
    AutocompleteEditModel* edit_model,
    Profile* profile)
    : font_(NULL),
      model_(new AutocompletePopupModel(this, edit_model, profile)),
      edit_view_(edit_view),
      window_(gtk_window_new(GTK_WINDOW_POPUP)),
      opened_(false) {
  GTK_WIDGET_UNSET_FLAGS(window_, GTK_CAN_FOCUS);
  // Don't allow the window to be resized.  This also forces the window to
  // shrink down to the size of its child contents.
  gtk_window_set_resizable(GTK_WINDOW(window_), FALSE);
  gtk_widget_set_app_paintable(window_, TRUE);
  // Have GTK double buffer around the expose signal.
  gtk_widget_set_double_buffered(window_, TRUE);
  // Set the background color, so we don't need to paint it manually.
  gtk_widget_modify_bg(window_, GTK_STATE_NORMAL, &kBackgroundColor);

  // TODO(deanm): We might want to eventually follow what Windows does and
  // plumb a ChromeFont through.  This is because popup windows have a
  // different font size, although we could just derive that font here.
  ChromeFont default_font;
  font_ = PangoFontFromChromeFont(default_font);

  g_signal_connect(window_, "expose-event", 
                   G_CALLBACK(&HandleExposeThunk), this);
}

AutocompletePopupViewGtk::~AutocompletePopupViewGtk() {
  // Explicitly destroy our model here, before we destroy our GTK widgets.
  // This is because the model destructor can call back into us, and we need
  // to make sure everything is still valid when it does.
  model_.reset();
  gtk_widget_destroy(window_);
  pango_font_description_free(font_);
}

void AutocompletePopupViewGtk::InvalidateLine(size_t line) {
  GdkRectangle rect = GetWindowRect(window_->window);
  rect = GetRectForLine(line, rect.width);
  gdk_window_invalidate_rect(window_->window, &rect, FALSE);
}

void AutocompletePopupViewGtk::UpdatePopupAppearance() {
  const AutocompleteResult& result = model_->result();
  if (result.empty()) {
    Hide();
    return;
  }

  Show(result.size());
  gtk_widget_queue_draw(window_);
}

void AutocompletePopupViewGtk::OnHoverEnabledOrDisabled(bool disabled) {
  NOTIMPLEMENTED();
}

void AutocompletePopupViewGtk::PaintUpdatesNow() {
  // Paint our queued invalidations now, synchronously.
  gdk_window_process_updates(window_->window, FALSE);
}

void AutocompletePopupViewGtk::Show(size_t num_results) {
  gint x, y, width;
  edit_view_->BottomLeftPosWidth(&x, &y, &width);
  gtk_window_move(GTK_WINDOW(window_), x, y + kTopMargin);
  gtk_widget_set_size_request(window_, width,
      (num_results * kHeightPerResult) + (kBorderThickness * 2));
  gtk_widget_show(window_);
  opened_ = true;
}

void AutocompletePopupViewGtk::Hide() {
  gtk_widget_hide(window_);
  opened_ = false;
}

gboolean AutocompletePopupViewGtk::HandleExpose(GtkWidget* widget,
                                                GdkEventExpose* event) {
  const AutocompleteResult& result = model_->result();

  GdkRectangle window_rect = GetWindowRect(event->window);
  // Handle when our window is super narrow.  A bunch of the calculations
  // below would go negative, and really we're not going to fit anything
  // useful in such a small window anyway.  Just don't paint anything.
  // This means we won't draw the border, but, yeah, whatever.
  if (window_rect.width < (kLeftRightPadding * 3))
    return TRUE;

  GdkDrawable* drawable = GDK_DRAWABLE(event->window);
  GdkGC* gc = gdk_gc_new(drawable);

  GdkGCValues default_gc_values;
  gdk_gc_get_values(gc, &default_gc_values);

  // kBorderColor is unallocated, so use the GdkRGB routine.
  gdk_gc_set_rgb_fg_color(gc, &kBorderColor);

  // This assert is kinda ugly, but it would be more currently unneeded work
  // to support painting a border that isn't 1 pixel thick.  There is no point
  // in writing that code now, and explode if that day ever comes.
  COMPILE_ASSERT(kBorderThickness == 1, border_1px_implied);
  // Draw the 1px border around the entire window.
  gdk_draw_rectangle(drawable, gc, FALSE,
                     0, 0,
                     window_rect.width - 1, window_rect.height - 1);

  // Draw the background for the selected result line.
  size_t selected = model_->selected_line();
  if (selected != AutocompletePopupModel::kNoMatch) {
    gdk_gc_set_rgb_fg_color(gc, &kSelectedBackgroundColor);
    GdkRectangle rect = GetRectForLine(selected, window_rect.width);
    gdk_draw_rectangle(drawable, gc, TRUE,
                       rect.x, rect.y, rect.width, rect.height);
  }

  // Restore the original GC foreground color.
  gdk_gc_set_foreground(gc, &default_gc_values.foreground);

  // TODO(deanm): Cache the layout?  How expensive is it to create?
  PangoLayout* layout = gtk_widget_create_pango_layout(window_, NULL);

  pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
  pango_layout_set_width(layout,
      (window_rect.width - (kLeftRightPadding * 2)) * PANGO_SCALE);
  pango_layout_set_height(layout, kHeightPerResult * PANGO_SCALE);
  pango_layout_set_font_description(layout, font_);

  // TODO(deanm): Intersect the line and damage rects, and only repaint and
  // layout the lines that are actually damaged.  For now paint everything.
  for (size_t i = 0; i < result.size(); ++i) {
    std::string contents_utf8 = WideToUTF8(result.match_at(i).contents);
    pango_layout_set_text(layout, contents_utf8.data(), contents_utf8.size());
    // TODO(deanm): Should probably try harder to vertically align the text.
    gdk_draw_layout(drawable, gc,
                    kLeftRightPadding, i * kHeightPerResult + 1,
                    layout);
    // TODO(deanm): Draw the "description" on the right of the contents.
  }

  g_object_unref(gc);

  return TRUE;
}
