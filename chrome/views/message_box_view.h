// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
class MessageBoxView : public views::View {
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
                                    views::View* parent,
                                    views::View* child);

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
  views::Label* message_label_;

  // Input text field for the message box.
  views::TextField* prompt_field_;

  // Icon displayed in the upper left corner of the message box.
  views::ImageView* icon_;

  // Checkbox for the message box.
  views::CheckBox* check_box_;

  // Maximum width of the message label.
  int message_width_;

  ScopedRunnableMethodFactory<MessageBoxView> focus_grabber_factory_;

  DISALLOW_EVIL_CONSTRUCTORS(MessageBoxView);
};

#endif // CHROME_VIEWS_MESSAGE_BOX_VIEW_VIEW_H__

