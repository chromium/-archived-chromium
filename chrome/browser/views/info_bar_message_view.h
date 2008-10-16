// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  explicit InfoBarMessageView(views::Label* message);

  virtual ~InfoBarMessageView();

  void SetMessageText(const std::wstring& message);

  std::wstring GetMessageText();

 private:
  // Creates message label.
  void Init();

  std::wstring message_string_;

  views::Label* message_label_;

  DISALLOW_EVIL_CONSTRUCTORS(InfoBarMessageView);
};

#endif // CHROME_BROWSER_VIEWS_INFO_BAR_MESSAGE_VIEW_H__
