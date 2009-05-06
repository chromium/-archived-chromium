// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_MESSAGE_BOX_FLAGS_H_
#define APP_MESSAGE_BOX_FLAGS_H_

#include "base/basictypes.h"

// This class contains flags used to communicate the type of message box
// to show.  E.g., the renderer can request the browser to show a
// javascript alert or a javascript confirm message.
class MessageBoxFlags {
 public:
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

  // Dialog button identifiers used to specify which buttons to show the user.
  enum DialogButton {
    DIALOGBUTTON_NONE = 0,    // No dialog buttons, for WindowType == WINDOW.
    DIALOGBUTTON_OK = 1,      // Has an OK button.
    DIALOGBUTTON_CANCEL = 2,  // Has a Cancel button (becomes a Close button if
  };                          // no OK button).

 private:
  MessageBoxFlags() {}
  DISALLOW_COPY_AND_ASSIGN(MessageBoxFlags);
};

#endif  // APP_MESSAGE_BOX_FLAGS_H_
