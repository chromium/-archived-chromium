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

#ifndef CHROME_BROWSER_TABS_TAB_STRIP_MODEL_ORDER_CONTROLLER_H__
#define CHROME_BROWSER_TABS_TAB_STRIP_MODEL_ORDER_CONTROLLER_H__

#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/pref_member.h"

class TabContents;
class TabStripModel;

///////////////////////////////////////////////////////////////////////////////
// TabStripModelOrderController
//
//  An object that allows different types of ordering and reselection to be
//  heuristics plugged into a TabStripModel.
//
class TabStripModelOrderController : public TabStripModelObserver {
 public:
  explicit TabStripModelOrderController(TabStripModel* tabstrip);
  virtual ~TabStripModelOrderController();

  // Determine where to place a newly opened tab by using the supplied
  // transition and foreground flag to figure out how it was opened.
  virtual int DetermineInsertionIndex(TabContents* new_contents,
                                      PageTransition::Type transition,
                                      bool foreground);

  // Determine where to shift selection after a tab is closed.
  virtual int DetermineNewSelectedIndex(int removed_index) const;

  // Overridden from TabStripModelObserver:
  virtual void TabSelectedAt(TabContents* old_contents,
                             TabContents* new_contents,
                             int index,
                             bool user_gesture);

 protected:
  // Returns a valid index to be selected after the tab at |removing_index| is
  // closed. If |index| is after |removing_index|, |index| is adjusted to
  // reflect the fact that |removing_index| is going away.
  int GetValidIndex(int index, int removing_index) const;

  TabStripModel* tabstrip_;
};

#endif CHROME_BROWSER_TABS_TAB_STRIP_MODEL_ORDER_CONTROLLER_H__
