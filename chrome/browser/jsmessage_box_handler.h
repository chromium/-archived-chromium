// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_JSMESSAGE_BOX_HANDLER_H_
#define CHROME_BROWSER_JSMESSAGE_BOX_HANDLER_H_

#include <string>

#include "chrome/common/ipc_message.h"

class GURL;
class TabContents;

// Creates and runs a Javascript Message Box dialog.
// The dialog type is specified within |dialog_flags|, the
// default static display text is in |message_text| and if the dialog box is
// a user input prompt() box, the default text for the text field is in
// |default_prompt_text|. The result of the operation is returned using
// |reply_msg|.
void RunJavascriptMessageBox(TabContents* tab_contents,
                             const GURL& frame_url,
                             int dialog_flags,
                             const std::wstring& message_text,
                             const std::wstring& default_prompt_text,
                             bool display_suppress_checkbox,
                             IPC::Message* reply_msg);

// This will display a modal dialog box with a header and footer asking the
// the user if they wish to navigate away from a page, with additional text
// |message_text| between the header and footer. The users response is
// returned to the renderer using |reply_msg|.
void RunBeforeUnloadDialog(TabContents* tab_contents,
                           const std::wstring& message_text,
                           IPC::Message* reply_msg);

#endif // CHROME_BROWSER_JSMESSAGE_BOX_HANDLER_H_
