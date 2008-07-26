// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_BROWSER_VIEWS_EXTERNAL_FOCUS_TRACKER_H__
#define CHROME_BROWSER_VIEWS_EXTERNAL_FOCUS_TRACKER_H__

#include "chrome/views/focus_manager.h"

namespace ChromeViews {

class View;
class ViewStorage;

// ExternalFocusTracker tracks the last focused view which belongs to the
// provided focus manager and is not either the provided parent view or one of
// its descendants. This is generally used if the parent view want to return
// focus to some other view once it is dismissed. The parent view and the focus
// manager must exist for the duration of the tracking. If the focus manager
// must be deleted before this object is deleted, make sure to call
// SetFocusManager(NULL) first.
//
// Typical use: When a view is added to the view hierarchy, it instantiates an
// ExternalFocusTracker and passes in itself and its focus manager. Then,
// when that view wants to return focus to the last focused view which is not
// itself and not a descandant of itself, (usually when it is being closed)
// it calls FocusLastFocusedExternalView.
class ExternalFocusTracker : public FocusChangeListener {
 public:
  ExternalFocusTracker(View* parent_view, FocusManager* focus_manager);

  virtual ~ExternalFocusTracker();
  // FocusChangeListener implementation.
  virtual void FocusWillChange(View* focused_before, View* focused_now);

  // Focuses last focused view which is not a child of parent view and is not
  // parent view itself. Returns true if focus for a view was requested, false
  // otherwise.
  void FocusLastFocusedExternalView();

  // Sets the focus manager whose focus we are tracking. |focus_manager| can
  // be NULL, but no focus changes will be tracked. This is useful if the focus
  // manager went away, but you might later want to start tracking with a new
  // manager later, or call FocusLastFocusedExternalView to focus the previous
  // view.
  void SetFocusManager(ChromeViews::FocusManager* focus_manager);

 private:
  // Store the provided view. This view will be focused when
  // FocusLastFocusedExternalView is called.
  void StoreLastFocusedView(View* view);

  // Store the currently focused view for our view manager and register as a
  // listener for future focus changes.
  void StartTracking();

  // Focus manager which we are a listener for.
  FocusManager* focus_manager_;

  // ID of the last focused view, which we store in view_storage_.
  int last_focused_view_storage_id_;

  // Used to store the last focused view which is not a child of
  // ExternalFocusTracker.
  ViewStorage* view_storage_;

  // The parent view of views which we should not track focus changes to. We
  // also do not track changes to parent_view_ itself.
  View* parent_view_;

  DISALLOW_EVIL_CONSTRUCTORS(ExternalFocusTracker);
};

} // namespace
#endif // CHROME_BROWSER_VIEWS_EXTERNAL_FOCUS_TRACKER_H__
