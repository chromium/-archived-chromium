// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_DECISION_H__
#define CHROME_VIEWS_DECISION_H__

#include <string>

#include "chrome/views/controller.h"
#include "chrome/views/native_button.h"
#include "chrome/views/view.h"

namespace views {

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

  // Overridden from View for custom layout.
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

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

}  // namespace views

#endif // CHROME_VIEWS_DECISION_H__
