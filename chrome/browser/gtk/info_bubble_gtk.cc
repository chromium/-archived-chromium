// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/info_bubble_gtk.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "app/gfx/path.h"
#include "base/basictypes.h"
#include "base/gfx/gtk_util.h"
#include "base/gfx/rect.h"
#include "base/logging.h"

namespace {

// The height of the arrow, and the width will be about twice the height.
const int kArrowSize = 5;
// Number of pixels to the start of the arrow from the edge of the window.
const int kArrowX = 13;
// Number of pixels between the tip of the arrow and the region we're
// pointing to.
const int kArrowToContentPadding = -6;
// We draw flat diagonal corners, each corner is an NxN square.
const int kCornerSize = 3;
// Margins around the content.
const int kTopMargin = kArrowSize + kCornerSize + 6;
const int kBottomMargin = kCornerSize + 6;
const int kLeftMargin = kCornerSize + 6;
const int kRightMargin = kCornerSize + 6;

const GdkColor kBackgroundColor = GDK_COLOR_RGB(0xff, 0xff, 0xff);
const GdkColor kFrameColor = GDK_COLOR_RGB(0x63, 0x63, 0x63);

// A small convenience since GdkPoint is a POD without a constructor.
GdkPoint MakeGdkPoint(gint x, gint y) {
  GdkPoint point = {x, y};
  return point;
}

enum FrameType {
  FRAME_MASK,
  FRAME_STROKE,
};

// Make the points for our polygon frame, either for fill (the mask), or for
// when we stroke the border.  NOTE: This seems a bit overcomplicated, but it
// requires a bunch of careful fudging to get the pixels rasterized exactly
// where we want them, the arrow to have a 1 pixel point, etc.
// TODO(deanm): Windows draws with Skia and uses some PNG images for the
// corners.  This is a lot more work, but they get anti-aliasing.
std::vector<GdkPoint> MakeFramePolygonPoints(int width,
                                             int height,
                                             FrameType type) {
  std::vector<GdkPoint> points;

  // If we have a stroke, we have to offset some of our points by 1 pixel.
  int off = (type == FRAME_MASK) ? 0 : 1;

  // Top left corner.
  points.push_back(MakeGdkPoint(0, kArrowSize + kCornerSize - 1));
  points.push_back(MakeGdkPoint(kCornerSize - 1, kArrowSize));

  // The arrow.
  points.push_back(MakeGdkPoint(kArrowX - kArrowSize, kArrowSize));
  points.push_back(MakeGdkPoint(kArrowX, 0));
  points.push_back(MakeGdkPoint(kArrowX + 1 - off, 0));
  points.push_back(MakeGdkPoint(kArrowX + kArrowSize + 1 - off, kArrowSize));

  // Top right corner.
  points.push_back(MakeGdkPoint(width - kCornerSize + 1 - off, kArrowSize));
  points.push_back(MakeGdkPoint(width - off, kArrowSize + kCornerSize - 1));

  // Bottom right corner.
  points.push_back(MakeGdkPoint(width - off, height - kCornerSize));
  points.push_back(MakeGdkPoint(width - kCornerSize, height - off));

  // Bottom left corner.
  points.push_back(MakeGdkPoint(kCornerSize - off, height - off));
  points.push_back(MakeGdkPoint(0, height - kCornerSize));

  return points;
}

// When our size is initially allocated or changed, we need to recompute
// and apply our shape mask region.
void HandleSizeAllocate(GtkWidget* widget,
                        GtkAllocation* allocation,
                        gpointer unused) {
  DCHECK(allocation->x == 0 && allocation->y == 0);
  std::vector<GdkPoint> points = MakeFramePolygonPoints(
      allocation->width, allocation->height, FRAME_MASK);
  GdkRegion* mask_region = gdk_region_polygon(&points[0],
                                              points.size(),
                                              GDK_EVEN_ODD_RULE);
  gdk_window_shape_combine_region(widget->window, mask_region, 0, 0);
  gdk_region_destroy(mask_region);
}

gboolean HandleExpose(GtkWidget* widget,
                      GdkEventExpose* event,
                      gpointer unused) {
  GdkDrawable* drawable = GDK_DRAWABLE(event->window);
  GdkGC* gc = gdk_gc_new(drawable);
  gdk_gc_set_rgb_fg_color(gc, &kFrameColor);

  // Stroke the frame border.
  std::vector<GdkPoint> points = MakeFramePolygonPoints(
      widget->allocation.width, widget->allocation.height, FRAME_STROKE);
  gdk_draw_polygon(drawable, gc, FALSE, &points[0], points.size());

  g_object_unref(gc);
  return FALSE;  // Propagate so our children paint, etc.
}

}  // namespace

// static
InfoBubbleGtk* InfoBubbleGtk::Show(GtkWindow* transient_toplevel,
                                   const gfx::Rect& rect,
                                   GtkWidget* content,
                                   InfoBubbleGtkDelegate* delegate) {
  InfoBubbleGtk* bubble = new InfoBubbleGtk();
  bubble->Init(transient_toplevel, rect, content);
  bubble->set_delegate(delegate);
  return bubble;
}

InfoBubbleGtk::InfoBubbleGtk()
    : delegate_(NULL),
      window_(NULL),
      accel_group_(gtk_accel_group_new()),
      screen_x_(0),
      screen_y_(0) {

}

InfoBubbleGtk::~InfoBubbleGtk() {
  g_object_unref(accel_group_);
}

void InfoBubbleGtk::Init(GtkWindow* transient_toplevel,
                         const gfx::Rect& rect,
                         GtkWidget* content) {
  DCHECK(!window_);
  screen_x_ = rect.x() + (rect.width() / 2) - kArrowX;
  screen_y_ = rect.y() + rect.height() + kArrowToContentPadding;

  window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_transient_for(GTK_WINDOW(window_), transient_toplevel);
  gtk_window_set_decorated(GTK_WINDOW(window_), FALSE);
  gtk_window_set_resizable(GTK_WINDOW(window_), FALSE);
  gtk_widget_set_app_paintable(window_, TRUE);
  // Have GTK double buffer around the expose signal.
  gtk_widget_set_double_buffered(window_, TRUE);
  // Set the background color, so we don't need to paint it manually.
  gtk_widget_modify_bg(window_, GTK_STATE_NORMAL, &kBackgroundColor);
  // Make sure that our window can be focused.
  GTK_WIDGET_SET_FLAGS(window_, GTK_CAN_FOCUS);

  // Attach our accelerator group to the window with an escape accelerator.
  gtk_accel_group_connect(accel_group_, GDK_Escape,
      static_cast<GdkModifierType>(0), static_cast<GtkAccelFlags>(0),
      g_cclosure_new(G_CALLBACK(&HandleEscapeThunk), this, NULL));
  gtk_window_add_accel_group(GTK_WINDOW(window_), accel_group_);

  GtkWidget* alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment),
                            kTopMargin, kBottomMargin,
                            kLeftMargin, kRightMargin);

  gtk_container_add(GTK_CONTAINER(alignment), content);
  gtk_container_add(GTK_CONTAINER(window_), alignment);

  // GtkWidget only exposes the bitmap mask interface.  Use GDK to more
  // efficently mask a GdkRegion.  Make sure the window is realized during
  // HandleSizeAllocate, so the mask can be applied to the GdkWindow.
  gtk_widget_realize(window_);
  gtk_window_move(GTK_WINDOW(window_), screen_x_, screen_y_);

  gtk_widget_add_events(window_, GDK_BUTTON_PRESS_MASK |
                                 GDK_BUTTON_RELEASE_MASK);

  g_signal_connect(window_, "size-allocate",
                   G_CALLBACK(HandleSizeAllocate), NULL);
  g_signal_connect(window_, "expose-event",
                   G_CALLBACK(HandleExpose), NULL);
  g_signal_connect(window_, "configure-event",
                   G_CALLBACK(&HandleConfigureThunk), this);
  g_signal_connect(window_, "button-press-event",
                   G_CALLBACK(&HandleButtonPressThunk), this);
  g_signal_connect(window_, "destroy",
                   G_CALLBACK(&HandleDestroyThunk), this);

  gtk_widget_show_all(window_);
  // Make sure our window has focus, is brought to the top, etc.
  gtk_window_present(GTK_WINDOW(window_));
  // We add a GTK (application level) grab.  This means we will get all
  // keyboard and mouse events for our application, even if they were delivered
  // on another window.  This allows us to close when the user clicks outside
  // of the info bubble.  We don't use an X grab since that would steal
  // keystrokes from your window manager, prevent you from interacting with
  // other applications, etc.
  gtk_grab_add(window_);
}

void InfoBubbleGtk::Close(bool closed_by_escape) {
  // Notify the delegate that we're about to close.  This gives the chance
  // to save state / etc from the hosted widget before it's destroyed.
  if (delegate_)
    delegate_->InfoBubbleClosing(this, closed_by_escape);

  DCHECK(window_);
  gtk_widget_destroy(window_);
  // |this| has been deleted, see HandleDestroy.
}

gboolean InfoBubbleGtk::HandleEscape() {
  Close(true);  // Close by escape.
  return TRUE;
}

gboolean InfoBubbleGtk::HandleConfigure(GdkEventConfigure* event) {
  // If the window is moved someplace besides where we want it, move it back.
  // TODO(deanm): In the end, I will probably remove this code and just let
  // the user move around the bubble like a normal dialog.  I want to try
  // this for now and see if it causes problems when any window managers.
  if (event->x != screen_x_ || event->y != screen_y_)
    gtk_window_move(GTK_WINDOW(window_), screen_x_, screen_y_);
  return FALSE;
}

gboolean InfoBubbleGtk::HandleButtonPress(GdkEventButton* event) {
  // If we got a click in our own window, that's ok.
  if (event->window == window_->window)
    return FALSE;  // Propagate.

  // Otherwise we had a click outside of our window, close ourself.
  Close();
  return TRUE;
}

gboolean InfoBubbleGtk::HandleDestroy() {
  // We are self deleting, we have a destroy signal setup to catch when we
  // destroy the widget manually, or the window was closed via X.  This will
  // delete the InfoBubbleGtk object.
  delete this;
  return FALSE;  // Propagate.
}
