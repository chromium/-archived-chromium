// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/location_bar_view_gtk.h"

#include <string>

#include "base/basictypes.h"
#include "base/gfx/gtk_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/alternate_nav_url_fetcher.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view_gtk.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/page_transition_types.h"
#include "skia/include/SkBitmap.h"
#include "webkit/glue/window_open_disposition.h"

namespace {

// We are positioned with a little bit of extra space that we don't use now.
const int kTopMargin = 1;
const int kBottomMargin = 2;
// We don't want to edit control's text to be right against the edge.
const int kEditLeftRightPadding = 4;
// We draw a border on the top and bottom (but not on left or right).
const int kBorderThickness = 1;

// TODO(deanm): Eventually this should be painted with the background png
// image, but for now we get pretty close by just drawing a solid border.
const GdkColor kBorderColor = GDK_COLOR_RGB(0xbe, 0xc8, 0xd4);

}  // namespace

// static
const GdkColor LocationBarViewGtk::kBackgroundColorByLevel[3] = {
  GDK_COLOR_RGB(255, 245, 195),  // SecurityLevel SECURE: Yellow.
  GDK_COLOR_RGB(255, 255, 255),  // SecurityLevel NORMAL: White.
  GDK_COLOR_RGB(255, 255, 255),  // SecurityLevel INSECURE: White.
};

LocationBarViewGtk::LocationBarViewGtk(CommandUpdater* command_updater,
                                       ToolbarModel* toolbar_model)
    : profile_(NULL),
      command_updater_(command_updater),
      toolbar_model_(toolbar_model),
      disposition_(CURRENT_TAB),
      transition_(PageTransition::TYPED) {
}

LocationBarViewGtk::~LocationBarViewGtk() {
  // All of our widgets should have be children of / owned by the alignment.
  alignment_.Destroy();
}

void LocationBarViewGtk::Init() {
  location_entry_.reset(new AutocompleteEditViewGtk(this,
                                                    toolbar_model_,
                                                    profile_,
                                                    command_updater_));
  location_entry_->Init();

  alignment_.Own(gtk_alignment_new(0.0, 0.0, 1.0, 1.0));
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment_.get()),
                            kTopMargin + kBorderThickness,
                            kBottomMargin + kBorderThickness,
                            kEditLeftRightPadding, kEditLeftRightPadding);
  // We will paint for the alignment, to paint the background and border.
  gtk_widget_set_app_paintable(alignment_.get(), TRUE);
  // Have GTK double buffer around the expose signal.
  gtk_widget_set_double_buffered(alignment_.get(), TRUE);
  g_signal_connect(alignment_.get(), "expose-event", 
                   G_CALLBACK(&HandleExposeThunk), this);

  gtk_container_add(GTK_CONTAINER(alignment_.get()),
                    location_entry_->widget());
}

void LocationBarViewGtk::SetProfile(Profile* profile) {
  profile_ = profile;
}

void LocationBarViewGtk::Update(const TabContents* contents) {
  location_entry_->Update(contents);
  // The security level (background color) could have changed, etc.
  gtk_widget_queue_draw(alignment_.get());
}

void LocationBarViewGtk::OnAutocompleteAccept(const GURL& url,
      WindowOpenDisposition disposition,
      PageTransition::Type transition,
      const GURL& alternate_nav_url) {
  if (!url.is_valid())
    return;

  location_input_ = UTF8ToWide(url.spec());
  disposition_ = disposition;
  transition_ = transition;

  if (!command_updater_)
    return;

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

void LocationBarViewGtk::OnChanged() {
  // TODO(deanm): Here is where we would do layout when we have things like
  // the keyword display, ssl icons, etc.
}

void LocationBarViewGtk::OnInputInProgress(bool in_progress) {
  NOTIMPLEMENTED();
}

SkBitmap LocationBarViewGtk::GetFavIcon() const {
  NOTIMPLEMENTED();
  return SkBitmap();
}

std::wstring LocationBarViewGtk::GetTitle() const {
  NOTIMPLEMENTED();
  return std::wstring();
}

void LocationBarViewGtk::ShowFirstRunBubble() {
  NOTIMPLEMENTED();
}

std::wstring LocationBarViewGtk::GetInputString() const {
  return location_input_;
}

WindowOpenDisposition LocationBarViewGtk::GetWindowOpenDisposition() const {
  return disposition_;
}

PageTransition::Type LocationBarViewGtk::GetPageTransition() const {
  return transition_;
}

void LocationBarViewGtk::AcceptInput() {
  AcceptInputWithDisposition(CURRENT_TAB);
}

void LocationBarViewGtk::AcceptInputWithDisposition(
    WindowOpenDisposition disposition) {
  location_entry_->model()->AcceptInput(disposition, false);
}

void LocationBarViewGtk::FocusLocation() {
  location_entry_->SetFocus();
  location_entry_->SelectAll(true);
}

void LocationBarViewGtk::FocusSearch() {
  location_entry_->SetUserText(L"?");
  location_entry_->SetFocus();
}

void LocationBarViewGtk::UpdateFeedIcon() {
  // Hide our lack of a feed icon for now, since it's not fully implemented
  // even on Windows yet.
  FeedList* feeds = toolbar_model_->GetFeedList().get();
  if (feeds && feeds->list().size() > 0)
    NOTIMPLEMENTED();
}

void LocationBarViewGtk::SaveStateToContents(TabContents* contents) {
  NOTIMPLEMENTED();
}

gboolean LocationBarViewGtk::HandleExpose(GtkWidget* widget,
                                          GdkEventExpose* event) {
  GdkDrawable* drawable = GDK_DRAWABLE(event->window);
  GdkGC* gc = gdk_gc_new(drawable);

  GdkRectangle* alloc_rect = &alignment_.get()->allocation;

  // The area outside of our margin, which includes the border.
  GdkRectangle inner_rect = {
      alloc_rect->x,
      alloc_rect->y + kTopMargin,
      alloc_rect->width,
      alloc_rect->height - kTopMargin - kBottomMargin};

  // Draw our 1px border.  TODO(deanm): Maybe this would be cleaner as an
  // overdrawn stroked rect with a clip to the allocation?
  gdk_gc_set_rgb_fg_color(gc, &kBorderColor);
  gdk_draw_rectangle(drawable, gc, TRUE,
                     inner_rect.x,
                     inner_rect.y,
                     inner_rect.width,
                     kBorderThickness);
  gdk_draw_rectangle(drawable, gc, TRUE,
                     inner_rect.x,
                     inner_rect.y + inner_rect.height - kBorderThickness,
                     inner_rect.width,
                     kBorderThickness);

  // Draw the background within the border.
  gdk_gc_set_rgb_fg_color(gc,
      &kBackgroundColorByLevel[toolbar_model_->GetSchemeSecurityLevel()]);
  gdk_draw_rectangle(drawable, gc, TRUE,
                     inner_rect.x,
                     inner_rect.y + kBorderThickness,
                     inner_rect.width,
                     inner_rect.height - (kBorderThickness * 2));

  g_object_unref(gc);

  return FALSE;  // Continue propagating the expose.
}
