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

#ifndef CHROME_BROWSER_VIEWS_INFO_BAR_MESSAGE_VIEW_H__
#define CHROME_BROWSER_VIEWS_INFO_BAR_MESSAGE_VIEW_H__

#include "chrome/browser/views/info_bar_item_view.h"
#include "chrome/views/label.h"

// A generic message for the info bar. Displays a label and a close button.
// Can be inherited to override the default behavior of the close button, which
// closes and deletes the info bar by default.
class InfoBarMessageView :  public InfoBarItemView {

 public:
  explicit InfoBarMessageView(const std::wstring& message);

  explicit InfoBarMessageView(ChromeViews::Label* message);

  virtual ~InfoBarMessageView();

  void SetMessageText(const std::wstring& message);

  std::wstring GetMessageText();

 private:
  // Creates message label.
  void Init();

  std::wstring message_string_;

  ChromeViews::Label* message_label_;

  DISALLOW_EVIL_CONSTRUCTORS(InfoBarMessageView);
};

#endif // CHROME_BROWSER_VIEWS_INFO_BAR_MESSAGE_VIEW_H__