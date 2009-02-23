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

// This class displays the contents of a message box. It is intended for use
// within a constrained window, and has options for a message, prompt, OK
// and Cancel buttons.
class MessageBoxView : public views::View {
 public:
  // flags
  static const int kFlagHasOKButton = 0x1;
  static const int kFlagHasCancelButton = 0x2;
  static const int kFlagHasPromptField = 0x4;
  static const int kFlagHasMessage = 0x8;

  // The following flag is used to indicate whether the message's alignment
  // should be autodetected or inherited from Chrome UI. Callers should pass
  // the correct flag based on the origin of the message. If the message is
  // from a web page (such as the JavaScript alert message), its alignment and
  // directionality are based on the first character with strong directionality
  // in the message. Chrome UI strings are localized string and therefore they
  // should have the same alignment and directionality as those of the Chrome
  // UI. For example, in RTL locales, even though some strings might begin with
  // an English character, they should still be right aligned and be displayed
  // Right-To-Left.
  //
  // TODO(xji): If the message is from a web page, then the message
  // directionality should be determined based on the directionality of the web
  // page. Please refer to http://crbug.com/7166 for more information.
  static const int kAutoDetectAlignment = 0x10;

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

  // Returns the text box.
  views::TextField* text_box() { return prompt_field_; }

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

  // Sets the state of the check-box.
  void SetCheckBoxSelected(bool selected);

 protected:
  // Layout and Painting functions.
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

 private:
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

