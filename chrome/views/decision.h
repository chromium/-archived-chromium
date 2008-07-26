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

#ifndef CHROME_VIEWS_DECISION_H__
#define CHROME_VIEWS_DECISION_H__

#include <string>

#include "chrome/views/controller.h"
#include "chrome/views/native_button.h"
#include "chrome/views/view.h"

namespace ChromeViews {

class Label;
class Option;

// A view that presents a user with a decision.  This view contains a title and
// a general description.  Users of this class should append at least one option
// for the user to select.
class Decision : public View {
 public:
  // The |title| appears in large font at the top of the view.  The |details|
  // appear in a multi-line text area below the title.  The |controller| is
  // notified when the user selects an option.
  Decision(const std::wstring& title,
           const std::wstring& details,
           Controller* controller);

  // Append an option to the view.  The |description| explains this option to
  // the user.  The |action| text is the text the user will click on to select
  // this option.  If the user selects this option, the controller will be asked
  // to ExecuteCommand |command_id|.
  void AppendOption(int command_id,
                    const std::wstring& description,
                    const std::wstring& action);

  // Need to override this to call layout.
  virtual void DidChangeBounds(const CRect& old_bounds,
                               const CRect& new_bounds);

  // Overridden from View for custom layout.
  virtual void Layout();
  virtual void GetPreferredSize(CSize *out);

 protected:
  // Override to call Layout().
  virtual void ViewHierarchyChanged(bool is_add, View *parent, View *child);

 private:
  // Our controller.
  Controller* controller_;

  // The views.
  Label* title_label_;
  Label* details_label_;

  // The option views that have been added.
  std::vector<Option*> options_;
};

} // namespace ChromeViews

#endif // CHROME_VIEWS_DECISION_H__
