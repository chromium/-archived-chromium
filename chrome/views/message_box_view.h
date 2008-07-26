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

#ifndef CHROME_VIEWS_MESSAGE_BOX_VIEW_VIEW_H__
#define CHROME_VIEWS_MESSAGE_BOX_VIEW_VIEW_H__

#include <string>

#include "base/task.h"
#include "chrome/views/checkbox.h"
#include "chrome/views/image_view.h"
#include "chrome/views/label.h"
#include "chrome/views/text_field.h"
#include "chrome/views/view.h"

// This class displays a message box within a constrained window
// with options for a message, a prompt, and OK and Cancel buttons.
class MessageBoxView : public ChromeViews::View {
 public:
  // flags
  static const int kFlagHasOKButton = 0x1;
  static const int kFlagHasCancelButton = 0x2;
  static const int kFlagHasPromptField = 0x4;
  static const int kFlagHasMessage = 0x8;

  static const int kIsConfirmMessageBox = kFlagHasMessage |
                                          kFlagHasOKButton |
                                          kFlagHasCancelButton;
  static const int kIsJavascriptAlert = kFlagHasOKButton | kFlagHasMessage;
  static const int kIsJavascriptConfirm = kIsJavascriptAlert |
                                          kFlagHasCancelButton;
  static const int kIsJavascriptPrompt = kIsJavascriptConfirm |
                                         kFlagHasPromptField;

  MessageBoxView(int dialog_flags,
                 const std::wstring& message,
                 const std::wstring& default_prompt,
                 int message_width);

  MessageBoxView(int dialog_flags,
                 const std::wstring& message,
                 const std::wstring& default_prompt);

  // Returns user entered data in the prompt field.
  std::wstring GetInputText();

  // Returns true if a checkbox is selected, false otherwise. (And false if
  // the message box has no checkbox.)
  bool IsCheckBoxSelected();

  // Adds |icon| to the upper left of the message box or replaces the current
  // icon. To start out, the message box has no icon.
  void SetIcon(const SkBitmap& icon);

  // Adds a checkbox with the specified label to the message box if this is the
  // first call. Otherwise, it changes the label of the current checkbox. To
  // start, the message box has no checkbox until this function is called.
  void SetCheckBoxLabel(const std::wstring& label);

 protected:
  // Layout and Painting functions.
  virtual void ViewHierarchyChanged(bool is_add,
                                    ChromeViews::View* parent,
                                    ChromeViews::View* child);

 private:
  // Called after ViewHierarchyChanged's call stack unwinds and returns to the
  // message loop to focus the first focusable element in the dialog box.
  void FocusFirstFocusableControl();

  // Sets up the layout manager and initializes the prompt field. This should
  // only be called once, from the constructor.
  void Init(int dialog_flags, const std::wstring& default_prompt);

  // Sets up the layout manager based on currently initialized views. Should be
  // called when a view is initialized or changed.
  void ResetLayoutManager();

  // Message for the message box.
  ChromeViews::Label* message_label_;

  // Input text field for the message box.
  ChromeViews::TextField* prompt_field_;

  // Icon displayed in the upper left corner of the message box.
  ChromeViews::ImageView* icon_;

  // Checkbox for the message box.
  ChromeViews::CheckBox* check_box_;

  // Maximum width of the message label.
  int message_width_;

  ScopedRunnableMethodFactory<MessageBoxView> focus_grabber_factory_;

  DISALLOW_EVIL_CONSTRUCTORS(MessageBoxView);
};

#endif // CHROME_VIEWS_MESSAGE_BOX_VIEW_VIEW_H__
