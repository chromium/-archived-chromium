// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_LOCATION_BAR_VIEW_MAC_H_
#define CHROME_BROWSER_COCOA_LOCATION_BAR_VIEW_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_ptr.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view_mac.h"
#include "chrome/browser/location_bar.h"

class CommandUpdater;
class ToolbarModel;

// A C++ bridge class that represents the location bar UI element to
// the portable code.  Wires up an AutocompleteEditViewMac instance to
// the location bar text field, which handles most of the work.

class LocationBarViewMac : public AutocompleteEditController,
                           public LocationBar,
                           public LocationBarTesting {
 public:
  LocationBarViewMac(NSTextField* field,
                     CommandUpdater* command_updater,
                     ToolbarModel* toolbar_model,
                     Profile* profile);
  virtual ~LocationBarViewMac();

  // Overridden from LocationBar:
  virtual void ShowFirstRunBubble(bool use_OEM_bubble) { NOTIMPLEMENTED(); }
  virtual std::wstring GetInputString() const;
  virtual WindowOpenDisposition GetWindowOpenDisposition() const;
  virtual PageTransition::Type GetPageTransition() const;
  virtual void AcceptInput();
  virtual void AcceptInputWithDisposition(WindowOpenDisposition disposition);
  virtual void FocusLocation();
  virtual void FocusSearch();
  virtual void UpdateFeedIcon() { /* http://crbug.com/8832 */ }
  virtual void UpdatePageActions() { /* http://crbug.com/12281 */ }
  virtual void SaveStateToContents(TabContents* contents);
  virtual void Revert();
  virtual AutocompleteEditView* location_entry() {
    return edit_view_.get();
  }
  virtual LocationBarTesting* GetLocationBarForTesting() { return this; }

  // Overriden from LocationBarTesting:
  virtual int PageActionVisibleCount();

  // Updates the location bar.  Resets the bar's permanent text and
  // security style, and if |should_restore_state| is true, restores
  // saved state from the tab (for tab switching).
  void Update(const TabContents* tab, bool should_restore_state);

  virtual void OnAutocompleteAccept(const GURL& url,
      WindowOpenDisposition disposition,
      PageTransition::Type transition,
      const GURL& alternate_nav_url);
  virtual void OnChanged();
  virtual void OnInputInProgress(bool in_progress);
  virtual SkBitmap GetFavIcon() const;
  virtual std::wstring GetTitle() const;

 private:
  scoped_ptr<AutocompleteEditViewMac> edit_view_;

  CommandUpdater* command_updater_;  // Weak, owned by Browser.

  // When we get an OnAutocompleteAccept notification from the autocomplete
  // edit, we save the input string so we can give it back to the browser on
  // the LocationBar interface via GetInputString().
  std::wstring location_input_;

  // The user's desired disposition for how their input should be opened.
  WindowOpenDisposition disposition_;

  // The transition type to use for the navigation.
  PageTransition::Type transition_;

  DISALLOW_COPY_AND_ASSIGN(LocationBarViewMac);
};

#endif  // CHROME_BROWSER_COCOA_LOCATION_BAR_VIEW_MAC_H_
