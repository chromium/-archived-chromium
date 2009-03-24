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
#include "chrome/common/page_transition_types.h"
#include "skia/include/SkBitmap.h"
#include "webkit/glue/window_open_disposition.h"

namespace {

// We are positioned with a little bit of extra space that we don't use now.
const int kTopPadding = 1;
const int kBottomPadding = 2;

// TODO(deanm): Eventually this should be painted with the background png
// image, but for now we get pretty close by just drawing a solid border.
const GdkColor kBorderColor = GDK_COLOR_RGB(0xbe, 0xc8, 0xd4);

}  // namespace

LocationBarViewGtk::LocationBarViewGtk(CommandUpdater* command_updater,
                                       ToolbarModel* toolbar_model)
    : inner_vbox_(NULL),
      profile_(NULL),
      command_updater_(command_updater),
      toolbar_model_(toolbar_model),
      disposition_(CURRENT_TAB),
      transition_(PageTransition::TYPED) {
}

LocationBarViewGtk::~LocationBarViewGtk() {
  // All of our widgets should have be children of / owned by the outer bin.
  outer_bin_.Destroy();
}

void LocationBarViewGtk::Init() {
  location_entry_.reset(new AutocompleteEditViewGtk(this,
                                                    toolbar_model_,
                                                    profile_,
                                                    command_updater_));
  location_entry_->Init();

  inner_vbox_ = gtk_vbox_new(false, 0);

  // TODO(deanm): We use a bunch of widgets to get things to layout with a
  // border, etc.  This should eventually be custom paint using the correct
  // background image, etc.
  gtk_box_pack_start(GTK_BOX(inner_vbox_), location_entry_->widget(),
                     TRUE, TRUE, 0);

  // Use an alignment to position our bordered location entry exactly.
  outer_bin_.Own(gtk_alignment_new(0, 0, 1, 1));
  gtk_alignment_set_padding(GTK_ALIGNMENT(outer_bin_.get()),
                            kTopPadding, kBottomPadding, 0, 0);
  gtk_container_add(
      GTK_CONTAINER(outer_bin_.get()),
      gfx::CreateGtkBorderBin(inner_vbox_, &kBorderColor, 1, 1, 0, 0));
}

void LocationBarViewGtk::SetProfile(Profile* profile) {
  profile_ = profile;
}

void LocationBarViewGtk::Update(const TabContents* contents) {
  location_entry_->Update(contents);
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
  NOTIMPLEMENTED();
}

void LocationBarViewGtk::SaveStateToContents(TabContents* contents) {
  NOTIMPLEMENTED();
}
