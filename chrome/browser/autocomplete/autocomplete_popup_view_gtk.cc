// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_popup_view_gtk.h"

#include <gtk/gtk.h>

#include <algorithm>

#include "app/gfx/font.h"
#include "app/resource_bundle.h"
#include "base/basictypes.h"
#include "base/gfx/gtk_util.h"
#include "base/gfx/rect.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view_gtk.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/common/notification_service.h"
#include "grit/theme_resources.h"

namespace {

const GdkColor kBorderColor = GDK_COLOR_RGB(0xc7, 0xca, 0xce);
const GdkColor kBackgroundColor = GDK_COLOR_RGB(0xff, 0xff, 0xff);
const GdkColor kSelectedBackgroundColor = GDK_COLOR_RGB(0xdf, 0xe6, 0xf6);
const GdkColor kHoveredBackgroundColor = GDK_COLOR_RGB(0xef, 0xf2, 0xfa);

const GdkColor kContentTextColor = GDK_COLOR_RGB(0x00, 0x00, 0x00);
const GdkColor kURLTextColor = GDK_COLOR_RGB(0x00, 0x88, 0x00);
const GdkColor kDescriptionTextColor = GDK_COLOR_RGB(0x80, 0x80, 0x80);
const GdkColor kDescriptionSelectedTextColor = GDK_COLOR_RGB(0x78, 0x82, 0xb1);

// We have a 1 pixel border around the entire results popup.
const int kBorderThickness = 1;
// The vertical height of each result.
const int kHeightPerResult = 24;
// Width of the icons.
const int kIconWidth = 16;
// We want to vertically center the image in the result space.
const int kIconTopPadding = 4;
// Space between the left edge (including the border) and the text.
const int kIconLeftPadding = 6 + kBorderThickness;
// Space between the image and the text.  Would be 6 to line up with the
// entry, but nudge it a bit more to match with the text in the entry.
const int kIconRightPadding = 10;
// Space between the left edge (including the border) and the text.
const int kIconAreaWidth =
    kIconLeftPadding + kIconWidth + kIconRightPadding;
// Space between the right edge (including the border) and the text.
const int kRightPadding = 3;
// When we have both a content and description string, we don't want the
// content to push the description off.  Limit the content to a percentage of
// the total width.
const float kContentWidthPercentage = 0.7;

// TODO(deanm): We should put this on gfx::Font so it can be shared.
// Returns a new pango font, free with pango_font_description_free().
PangoFontDescription* PangoFontFromGfxFont(const gfx::Font& chrome_font) {
  gfx::Font font = chrome_font;  // Copy so we can call non-const methods.
  PangoFontDescription* pfd = pango_font_description_new();
  pango_font_description_set_family(pfd, WideToUTF8(font.FontName()).c_str());
  pango_font_description_set_size(pfd, font.FontSize() * PANGO_SCALE);

  switch (font.style()) {
    case gfx::Font::NORMAL:
      // Nothing to do, should already be PANGO_STYLE_NORMAL.
      break;
    case gfx::Font::BOLD:
      pango_font_description_set_weight(pfd, PANGO_WEIGHT_BOLD);
      break;
    case gfx::Font::ITALIC:
      pango_font_description_set_style(pfd, PANGO_STYLE_ITALIC);
      break;
    case gfx::Font::UNDERLINED:
      // TODO(deanm): How to do underlined?  Where do we use it?  Probably have
      // to paint it ourselves, see pango_font_metrics_get_underline_position.
      break;
  }

  return pfd;
}

// Return a Rect covering the whole area of |window|.
gfx::Rect GetWindowRect(GdkWindow* window) {
  gint width, height;
  gdk_drawable_get_size(GDK_DRAWABLE(window), &width, &height);
  return gfx::Rect(0, 0, width, height);
}

// Return a Rect for the space for a result line.  This excludes the border,
// but includes the padding.  This is the area that is colored for a selection.
gfx::Rect GetRectForLine(size_t line, int width) {
  return gfx::Rect(kBorderThickness,
                   (line * kHeightPerResult) + kBorderThickness,
                   width - (kBorderThickness * 2),
                   kHeightPerResult);
}

// Helper for drawing an entire pixbuf without dithering.
void DrawFullPixbuf(GdkDrawable* drawable, GdkGC* gc, GdkPixbuf* pixbuf,
                    gint dest_x, gint dest_y) {
  gdk_draw_pixbuf(drawable, gc, pixbuf,
                  0, 0,                        // Source.
                  dest_x, dest_y,              // Dest.
                  -1, -1,                      // Width/height (auto).
                  GDK_RGB_DITHER_NONE, 0, 0);  // Don't dither.
}

// TODO(deanm): Find some better home for this, and make it more efficient.
size_t GetUTF8Offset(const std::wstring& wide_text, size_t wide_text_offset) {
  return WideToUTF8(wide_text.substr(0, wide_text_offset)).size();
}

void SetupLayoutForMatch(PangoLayout* layout,
      const std::wstring& text,
      AutocompleteMatch::ACMatchClassifications classifications,
      const GdkColor* base_color,
      const std::string& prefix_text) {
  std::string text_utf8 = prefix_text + WideToUTF8(text);
  pango_layout_set_text(layout, text_utf8.data(), text_utf8.size());

  PangoAttrList* attrs = pango_attr_list_new();

  // TODO(deanm): This is a hack, just to handle coloring prefix_text.
  // Hopefully I can clean up the match situation a bit and this will
  // come out cleaner.  For now, apply the base color to the whole text
  // so that our prefix will have the base color applied.
  PangoAttribute* base_fg_attr = pango_attr_foreground_new(
      base_color->red, base_color->green, base_color->blue);
  pango_attr_list_insert(attrs, base_fg_attr);  // Ownership taken.

  // Walk through the classifications, they are linear, in order, and should
  // cover the entire text.  We create a bunch of overlapping attributes,
  // extending from the offset to the end of the string.  The ones created
  // later will override the previous ones, meaning we will still setup each
  // portion correctly, we just don't need to compute the end offset.
  for (ACMatchClassifications::const_iterator i = classifications.begin();
       i != classifications.end(); ++i) {
    size_t offset = GetUTF8Offset(text, i->offset) + prefix_text.size();

    // TODO(deanm): All the colors should probably blend based on whether this
    // result is selected or not.  This would include the green URLs.  Right
    // now the caller is left to blend only the base color.  Do we need to
    // handle things like DIM urls?  Turns out DIM means something different
    // than you'd think, all of the description text is not DIM, it is a
    // special case that is not very common, but we should figure out and
    // support it.
    const GdkColor* color = base_color;
    if (i->style & ACMatchClassification::URL)
        color = &kURLTextColor;

    PangoAttribute* fg_attr = pango_attr_foreground_new(
        color->red, color->green, color->blue);
    fg_attr->start_index = offset;
    pango_attr_list_insert(attrs, fg_attr);  // Ownership taken.

    // Matched portions are bold, otherwise use the normal weight.
    PangoWeight weight = (i->style & ACMatchClassification::MATCH) ?
        PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL;
    PangoAttribute* weight_attr = pango_attr_weight_new(weight);
    weight_attr->start_index = offset;
    pango_attr_list_insert(attrs, weight_attr);  // Ownership taken.
  }

  pango_layout_set_attributes(layout, attrs);  // Ref taken.
  pango_attr_list_unref(attrs);
}

GdkPixbuf* IconForMatch(const AutocompleteMatch& match, bool selected) {
  // TODO(deanm): These would be better as pixmaps someday.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  static GdkPixbuf* o2_globe = rb.GetPixbufNamed(IDR_O2_GLOBE);
  static GdkPixbuf* o2_globe_s = rb.GetPixbufNamed(IDR_O2_GLOBE_SELECTED_DARK);
  static GdkPixbuf* o2_history = rb.GetPixbufNamed(IDR_O2_HISTORY);
  static GdkPixbuf* o2_history_s =
      rb.GetPixbufNamed(IDR_O2_HISTORY_SELECTED_DARK);
  static GdkPixbuf* o2_more = rb.GetPixbufNamed(IDR_O2_MORE);
  static GdkPixbuf* o2_more_s = rb.GetPixbufNamed(IDR_O2_MORE_SELECTED_DARK);
  static GdkPixbuf* o2_search = rb.GetPixbufNamed(IDR_O2_SEARCH);
  static GdkPixbuf* o2_search_s =
      rb.GetPixbufNamed(IDR_O2_SEARCH_SELECTED_DARK);
  static GdkPixbuf* o2_star = rb.GetPixbufNamed(IDR_O2_STAR);
  static GdkPixbuf* o2_star_s = rb.GetPixbufNamed(IDR_O2_STAR_SELECTED_DARK);

  if (match.starred)
    return selected ? o2_star_s : o2_star;

  switch (match.type) {
    case AutocompleteMatch::URL_WHAT_YOU_TYPED:
    case AutocompleteMatch::NAVSUGGEST:
      return selected ? o2_globe_s : o2_globe;
    case AutocompleteMatch::HISTORY_URL:
    case AutocompleteMatch::HISTORY_TITLE:
    case AutocompleteMatch::HISTORY_BODY:
    case AutocompleteMatch::HISTORY_KEYWORD:
      return selected ? o2_history_s : o2_history;
    case AutocompleteMatch::SEARCH_WHAT_YOU_TYPED:
    case AutocompleteMatch::SEARCH_HISTORY:
    case AutocompleteMatch::SEARCH_SUGGEST:
    case AutocompleteMatch::SEARCH_OTHER_ENGINE:
      return selected ? o2_search_s : o2_search;
    case AutocompleteMatch::OPEN_HISTORY_PAGE:
      return selected ? o2_more_s : o2_more;
    default:
      NOTREACHED();
      break;
  }

  return NULL;
}

}  // namespace

AutocompletePopupViewGtk::AutocompletePopupViewGtk(
    AutocompleteEditViewGtk* edit_view,
    AutocompleteEditModel* edit_model,
    Profile* profile,
    AutocompletePopupPositioner* popup_positioner)
    : model_(new AutocompletePopupModel(this, edit_model, profile)),
      edit_view_(edit_view),
      popup_positioner_(popup_positioner),
      window_(gtk_window_new(GTK_WINDOW_POPUP)),
      layout_(NULL),
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

  // Cache the layout so we don't have to create it for every expose.  If we
  // were a real widget we should handle changing directions, but we're not
  // doing RTL or anything yet, so it shouldn't be important now.
  layout_ = gtk_widget_create_pango_layout(window_, NULL);
  // We always ellipsize when drawing our text runs.
  pango_layout_set_ellipsize(layout_, PANGO_ELLIPSIZE_END);
  // TODO(deanm): We might want to eventually follow what Windows does and
  // plumb a gfx::Font through.  This is because popup windows have a
  // different font size, although we could just derive that font here.
  // For now, force the font size to 10pt.
  gfx::Font font = gfx::Font::CreateFont(gfx::Font().FontName(), 10);
  PangoFontDescription* pfd = PangoFontFromGfxFont(font);
  pango_layout_set_font_description(layout_, pfd);
  pango_font_description_free(pfd);

  gtk_widget_add_events(window_, GDK_BUTTON_MOTION_MASK |
                                 GDK_POINTER_MOTION_MASK |
                                 GDK_BUTTON_PRESS_MASK |
                                 GDK_BUTTON_RELEASE_MASK);
  g_signal_connect(window_, "motion-notify-event",
                   G_CALLBACK(&HandleMotionThunk), this);
  g_signal_connect(window_, "button-press-event",
                   G_CALLBACK(&HandleButtonPressThunk), this);
  g_signal_connect(window_, "button-release-event",
                   G_CALLBACK(&HandleButtonReleaseThunk), this);
  g_signal_connect(window_, "expose-event",
                   G_CALLBACK(&HandleExposeThunk), this);
}

AutocompletePopupViewGtk::~AutocompletePopupViewGtk() {
  // Explicitly destroy our model here, before we destroy our GTK widgets.
  // This is because the model destructor can call back into us, and we need
  // to make sure everything is still valid when it does.
  model_.reset();
  g_object_unref(layout_);
  gtk_widget_destroy(window_);
}

void AutocompletePopupViewGtk::InvalidateLine(size_t line) {
  // TODO(deanm): Is it possible to use some constant for the width, instead
  // of having to query the width of the window?
  GdkRectangle line_rect = GetRectForLine(
      line, GetWindowRect(window_->window).width()).ToGdkRectangle();
  gdk_window_invalidate_rect(window_->window, &line_rect, FALSE);
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

AutocompletePopupModel* AutocompletePopupViewGtk::GetModel() {
  return model_.get();
}

void AutocompletePopupViewGtk::Show(size_t num_results) {
  gfx::Rect rect = popup_positioner_->GetPopupBounds();
  rect.set_height((num_results * kHeightPerResult) + (kBorderThickness * 2));

  gtk_window_move(GTK_WINDOW(window_), rect.x(), rect.y());
  gtk_widget_set_size_request(window_, rect.width(), rect.height());
  gtk_widget_show(window_);
  opened_ = true;
}

void AutocompletePopupViewGtk::Hide() {
  gtk_widget_hide(window_);
  opened_ = false;
}

size_t AutocompletePopupViewGtk::LineFromY(int y) {
  size_t line = std::max(y - kBorderThickness, 0) / kHeightPerResult;
  return std::min(line, model_->result().size() - 1);
}

void AutocompletePopupViewGtk::AcceptLine(size_t line,
                                          WindowOpenDisposition disposition) {
  const AutocompleteMatch& match = model_->result().match_at(line);
  // OpenURL() may close the popup, which will clear the result set and, by
  // extension, |match| and its contents.  So copy the relevant strings out to
  // make sure they stay alive until the call completes.
  const GURL url(match.destination_url);
  std::wstring keyword;
  const bool is_keyword_hint = model_->GetKeywordForMatch(match, &keyword);
  edit_view_->OpenURL(url, disposition, match.transition, GURL(), line,
                      is_keyword_hint ? std::wstring() : keyword);
}

gboolean AutocompletePopupViewGtk::HandleMotion(GtkWidget* widget,
                                                GdkEventMotion* event) {
  // TODO(deanm): Windows has a bunch of complicated logic here.
  size_t line = LineFromY(event->y);
  // There is both a hovered and selected line, hovered just means your mouse
  // is over it, but selected is what's showing in the location edit.
  model_->SetHoveredLine(line);
  // Select the line if the user has the left mouse button down.
  if (event->state & GDK_BUTTON1_MASK)
    model_->SetSelectedLine(line, false);
  return TRUE;
}

gboolean AutocompletePopupViewGtk::HandleButtonPress(GtkWidget* widget,
                                                     GdkEventButton* event) {
  // Very similar to HandleMotion.
  size_t line = LineFromY(event->y);
  model_->SetHoveredLine(line);
  if (event->button == 1)
    model_->SetSelectedLine(line, false);
  return TRUE;
}

gboolean AutocompletePopupViewGtk::HandleButtonRelease(GtkWidget* widget,
                                                       GdkEventButton* event) {
  size_t line = LineFromY(event->y);
  switch (event->button) {
    case 1:  // Left click.
      AcceptLine(line, CURRENT_TAB);
      break;
    case 2:  // Middle click.
      AcceptLine(line, NEW_BACKGROUND_TAB);
      break;
    default:
      // Don't open the result.
      break;
  }
  return TRUE;
}

gboolean AutocompletePopupViewGtk::HandleExpose(GtkWidget* widget,
                                                GdkEventExpose* event) {
  const AutocompleteResult& result = model_->result();

  gfx::Rect window_rect = GetWindowRect(event->window);
  gfx::Rect damage_rect = gfx::Rect(event->area);
  // Handle when our window is super narrow.  A bunch of the calculations
  // below would go negative, and really we're not going to fit anything
  // useful in such a small window anyway.  Just don't paint anything.
  // This means we won't draw the border, but, yeah, whatever.
  // TODO(deanm): Make the code more robust and remove this check.
  if (window_rect.width() < (kIconAreaWidth * 3))
    return TRUE;

  GdkDrawable* drawable = GDK_DRAWABLE(event->window);
  GdkGC* gc = gdk_gc_new(drawable);

  // kBorderColor is unallocated, so use the GdkRGB routine.
  gdk_gc_set_rgb_fg_color(gc, &kBorderColor);

  // This assert is kinda ugly, but it would be more currently unneeded work
  // to support painting a border that isn't 1 pixel thick.  There is no point
  // in writing that code now, and explode if that day ever comes.
  COMPILE_ASSERT(kBorderThickness == 1, border_1px_implied);
  // Draw the 1px border around the entire window.
  gdk_draw_rectangle(drawable, gc, FALSE,
                     0, 0,
                     window_rect.width() - 1, window_rect.height() - 1);

  pango_layout_set_height(layout_, kHeightPerResult * PANGO_SCALE);

  // TODO(deanm): Intersect the line and damage rects, and only repaint and
  // layout the lines that are actually damaged.  For now paint everything.
  for (size_t i = 0; i < result.size(); ++i) {
    gfx::Rect line_rect = GetRectForLine(i, window_rect.width());
    // Only repaint and layout damaged lines.
    if (!line_rect.Intersects(damage_rect))
      continue;

    const AutocompleteMatch& match = result.match_at(i);
    bool is_selected = (model_->selected_line() == i);
    bool is_hovered = (model_->hovered_line() == i);
    if (is_selected || is_hovered) {
      gdk_gc_set_rgb_fg_color(gc, is_selected ? &kSelectedBackgroundColor :
                                                 &kHoveredBackgroundColor);
      // This entry is selected or hovered, fill a rect with the color.
      gdk_draw_rectangle(drawable, gc, TRUE,
                         line_rect.x(), line_rect.y(),
                         line_rect.width(), line_rect.height());
    }

    // Draw the icon for this result time.
    DrawFullPixbuf(drawable, gc, IconForMatch(match, is_selected),
                   kIconLeftPadding, line_rect.y() + kIconTopPadding);

    // Draw the results text vertically centered in the results space.
    // First draw the contents / url, but don't let it take up the whole width
    // if there is also a description to be shown.
    bool has_description = !match.description.empty();
    int text_width = window_rect.width() - (kIconAreaWidth + kRightPadding);
    int allocated_content_width = has_description ?
        text_width * kContentWidthPercentage : text_width;
    pango_layout_set_width(layout_, allocated_content_width * PANGO_SCALE);

    SetupLayoutForMatch(layout_, match.contents, match.contents_class,
                        &kContentTextColor, std::string());

    int actual_content_width, actual_content_height;
    pango_layout_get_size(layout_,
        &actual_content_width, &actual_content_height);
    actual_content_width /= PANGO_SCALE;
    actual_content_height /= PANGO_SCALE;

    DCHECK_LT(actual_content_height, kHeightPerResult);  // Font is too tall.
    // Center the text within the line.
    int content_y = std::max(line_rect.y(),
        line_rect.y() + ((kHeightPerResult - actual_content_height) / 2));

    gdk_draw_layout(drawable, gc,
                    kIconAreaWidth, content_y,
                    layout_);

    if (has_description) {
      pango_layout_set_width(layout_,
          (text_width - actual_content_width) * PANGO_SCALE);
      SetupLayoutForMatch(layout_, match.description, match.description_class,
                          is_selected ? &kDescriptionSelectedTextColor :
                              &kDescriptionTextColor,
                          std::string(" - "));

      gdk_draw_layout(drawable, gc,
                      kIconAreaWidth + actual_content_width, content_y,
                      layout_);
    }
  }

  g_object_unref(gc);

  return TRUE;
}
