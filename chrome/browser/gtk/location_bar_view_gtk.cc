// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/location_bar_view_gtk.h"

#include <string>

#include "app/resource_bundle.h"
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
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/glue/window_open_disposition.h"

namespace {

// We are positioned with a little bit of extra space that we don't use now.
const int kTopMargin = 1;
const int kBottomMargin = 1;
// We don't want to edit control's text to be right against the edge.
const int kEditLeftRightPadding = 4;
// We draw a border on the top and bottom (but not on left or right).
const int kBorderThickness = 1;

// Padding around the security icon.
const int kSecurityIconPaddingLeft = 4;
const int kSecurityIconPaddingRight = 2;

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
    ToolbarModel* toolbar_model, AutocompletePopupPositioner* popup_positioner)
    : security_icon_(NULL),
      profile_(NULL),
      command_updater_(command_updater),
      toolbar_model_(toolbar_model),
      popup_positioner_(popup_positioner),
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
                                                    command_updater_,
                                                    popup_positioner_));
  location_entry_->Init();

  alignment_.Own(gtk_alignment_new(0.0, 0.0, 1.0, 1.0));
  UpdateAlignmentPadding();
  // We will paint for the alignment, to paint the background and border.
  gtk_widget_set_app_paintable(alignment_.get(), TRUE);
  // Have GTK double buffer around the expose signal.
  gtk_widget_set_double_buffered(alignment_.get(), TRUE);
  // Redraw the whole location bar when it changes size (e.g., when toggling
  // the home button on/off.
  gtk_widget_set_redraw_on_allocate(alignment_.get(), TRUE);
  g_signal_connect(alignment_.get(), "expose-event",
                   G_CALLBACK(&HandleExposeThunk), this);

  gtk_container_add(GTK_CONTAINER(alignment_.get()),
                    location_entry_->widget());
}

void LocationBarViewGtk::SetProfile(Profile* profile) {
  profile_ = profile;
}

void LocationBarViewGtk::Update(const TabContents* contents) {
  SetSecurityIcon(toolbar_model_->GetIcon());
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
  // This is identical to the Windows code, except that we don't proxy the call
  // back through the Toolbar, and just access the model here.
  // The edit should make sure we're only notified when something changes.
  DCHECK(toolbar_model_->input_in_progress() != in_progress);

  toolbar_model_->set_input_in_progress(in_progress);
  Update(NULL);
}

SkBitmap LocationBarViewGtk::GetFavIcon() const {
  NOTIMPLEMENTED();
  return SkBitmap();
}

std::wstring LocationBarViewGtk::GetTitle() const {
  NOTIMPLEMENTED();
  return std::wstring();
}

void LocationBarViewGtk::ShowFirstRunBubble(bool use_OEM_bubble) {
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
  location_entry_->SetFocus();
  location_entry_->SetForcedQuery();
}

void LocationBarViewGtk::UpdatePageActions() {
  // http://code.google.com/p/chromium/issues/detail?id=11973
}

void LocationBarViewGtk::SaveStateToContents(TabContents* contents) {
  // http://crbug.com/9225
}

void LocationBarViewGtk::Revert() {
  location_entry_->RevertAll();
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

  // Some of our calculations are a bit sloppy.  Since we draw on our parent
  // window, set a clip to make sure that we don't draw outside.
  gdk_gc_set_clip_rectangle(gc, &inner_rect);

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

  // If we have an SSL icon, draw it vertically centered on the right side.
  if (security_icon_) {
    int icon_width = gdk_pixbuf_get_width(security_icon_);
    int icon_height = gdk_pixbuf_get_height(security_icon_);
    int icon_x = inner_rect.x + inner_rect.width -
        kBorderThickness - icon_width - kSecurityIconPaddingRight;
    int icon_y = inner_rect.y +
        ((inner_rect.height - icon_height) / 2);
    gdk_draw_pixbuf(drawable, gc, security_icon_,
                    0, 0, icon_x, icon_y, -1, -1,
                    GDK_RGB_DITHER_NONE, 0, 0);
  }

  g_object_unref(gc);

  return FALSE;  // Continue propagating the expose.
}

void LocationBarViewGtk::UpdateAlignmentPadding() {
  // When we have an icon, increase our right padding to make space for it.
  int right_padding = security_icon_ ? gdk_pixbuf_get_width(security_icon_) +
      kSecurityIconPaddingLeft + kSecurityIconPaddingRight :
      kEditLeftRightPadding;

  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment_.get()),
                            kTopMargin + kBorderThickness,
                            kBottomMargin + kBorderThickness,
                            kEditLeftRightPadding, right_padding);
}

void LocationBarViewGtk::SetSecurityIcon(ToolbarModel::Icon icon) {
  static ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  static GdkPixbuf* kLockIcon = rb.GetPixbufNamed(IDR_LOCK);
  static GdkPixbuf* kWarningIcon = rb.GetPixbufNamed(IDR_WARNING);

  switch (icon) {
    case ToolbarModel::LOCK_ICON:
      security_icon_ = kLockIcon;
      break;
    case ToolbarModel::WARNING_ICON:
      security_icon_ = kWarningIcon;
      break;
    case ToolbarModel::NO_ICON:
      security_icon_ = NULL;
      break;
    default:
      NOTREACHED();
      security_icon_ = NULL;
      break;
  }

  // Make sure there is room for the icon.
  UpdateAlignmentPadding();
}
