// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/location_bar_view_mac.h"

#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/alternate_nav_url_fetcher.h"
#import "chrome/browser/app_controller_mac.h"
#import "chrome/browser/autocomplete/autocomplete_edit_view_mac.h"
#include "chrome/browser/command_updater.h"
#include "skia/include/SkBitmap.h"

// TODO(shess): This code is mostly copied from the gtk
// implementation.  Make sure it's all appropriate and flesh it out.

LocationBarViewMac::LocationBarViewMac(NSTextField* field,
                                       CommandUpdater* command_updater,
                                       ToolbarModel* toolbar_model)
    : field_(field),
      command_updater_(command_updater),
      toolbar_model_(toolbar_model),
      disposition_(CURRENT_TAB),
      transition_(PageTransition::TYPED) {
}

LocationBarViewMac::~LocationBarViewMac() {
  // TODO(shess): Placeholder for omnibox changes.
}

void LocationBarViewMac::Init() {
  // TODO(shess): deanm indicates that it's likely we will eventually
  // get the profile somewhere between point of construction and
  // Init(), so mirroring how the gtk code sets this up.
  Profile* profile = [[NSApp delegate] defaultProfile];
  edit_view_.reset(new AutocompleteEditViewMac(this,
                                               toolbar_model_,
                                               profile,
                                               command_updater_));
  // TODO(shess): Include in constructor.
  edit_view_->SetField(field_);
}

std::wstring LocationBarViewMac::GetInputString() const {
  return location_input_;
}

WindowOpenDisposition LocationBarViewMac::GetWindowOpenDisposition() const {
  return disposition_;
}

// TODO(shess): Verify that this TODO is TODONE.
// TODO(rohitrao): Fix this to return different types once autocomplete and
// the onmibar are implemented.  For now, any URL that comes from the
// LocationBar has to have been entered by the user, and thus is of type
// PageTransition::TYPED.
PageTransition::Type LocationBarViewMac::GetPageTransition() const {
  return transition_;
}

void LocationBarViewMac::FocusLocation() {
  edit_view_->FocusLocation();
}

void LocationBarViewMac::SaveStateToContents(TabContents* contents) {
  // TODO(shess): Why SaveStateToContents vs SaveStateToTab?
  edit_view_->SaveStateToTab(contents);
}

void LocationBarViewMac::OnAutocompleteAccept(const GURL& url,
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

void LocationBarViewMac::OnChanged() {
  NOTIMPLEMENTED();
}

void LocationBarViewMac::OnInputInProgress(bool in_progress) {
  NOTIMPLEMENTED();
}

SkBitmap LocationBarViewMac::GetFavIcon() const {
  NOTIMPLEMENTED();
  return SkBitmap();
}

std::wstring LocationBarViewMac::GetTitle() const {
  NOTIMPLEMENTED();
  return std::wstring();
}
