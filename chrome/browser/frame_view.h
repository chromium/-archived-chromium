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

#ifndef CHROME_BROWSER_FRAME_VIEW_H__
#define CHROME_BROWSER_FRAME_VIEW_H__

#include <set>

#include "chrome/views/view.h"

class ChromeFrame;
class OSExchangeData;

// FrameView is the View that contains all the views of the ChromeFrame
// (XPFrame or VistaFrame). FrameView forwards all drag and drop messages to
// the TabStrip.
class FrameView : public ChromeViews::View {
 public:
  explicit FrameView(ChromeFrame* frame);
  virtual ~FrameView() {}

  // Adds view to the set of views that drops are allowed to occur on. You only
  // need invoke this for views whose y-coordinate extends above the tab strip
  // and you want to allow drops on.
  void AddViewToDropList(ChromeViews::View* view);

 protected:
  // As long as ShouldForwardToTabStrip returns true, drag and drop methods
  // are forwarded to the tab strip.
  virtual bool CanDrop(const OSExchangeData& data);
  virtual void OnDragEntered(const ChromeViews::DropTargetEvent& event);
  virtual int OnDragUpdated(const ChromeViews::DropTargetEvent& event);
  virtual void OnDragExited();
  virtual int OnPerformDrop(const ChromeViews::DropTargetEvent& event);

  // Returns true if the event should be forwarded to the TabStrip. This returns
  // true if y coordinate is less than the bottom of the tab strip, and is not
  // over another child view.
  virtual bool ShouldForwardToTabStrip(
      const ChromeViews::DropTargetEvent& event);

  // Overriden to remove views from dropable_views_.
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);

 private:
  // Creates and returns a new DropTargetEvent in the coordinates of the
  // TabStrip.
  ChromeViews::DropTargetEvent* MapEventToTabStrip(
      const ChromeViews::DropTargetEvent& event);

  // The ChromeFrame we're the child of.
  ChromeFrame* frame_;

  // Initially set in CanDrop by invoking the same method on the TabStrip.
  bool can_drop_;

  // If true, drag and drop events are being forwarded to the tab strip.
  // This is used to determine when to send OnDragExited and OnDragExited
  // to the tab strip.
  bool forwarding_to_tab_strip_;

  // Set of additional views drops are allowed on. We do NOT own these.
  std::set<ChromeViews::View*> dropable_views_;

  DISALLOW_EVIL_CONSTRUCTORS(FrameView);
};

#endif  // CHROME_BROWSER_FRAME_VIEW_H__
