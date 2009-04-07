// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/jsmessage_box_handler.h"

#include "build/build_config.h"
#include "chrome/browser/app_modal_dialog_queue.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/gfx/text_elider.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/message_box_flags.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"

namespace {

std::wstring GetWindowTitle(WebContents* web_contents, const GURL& frame_url) {
  if (!frame_url.has_host())
    return l10n_util::GetString(IDS_JAVASCRIPT_MESSAGEBOX_DEFAULT_TITLE);

  // We really only want the scheme, hostname, and port.
  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearPath();
  replacements.ClearQuery();
  replacements.ClearRef();
  GURL clean_url = frame_url.ReplaceComponents(replacements);

  // TODO(brettw) it should be easier than this to do the correct language
  // handling without getting the accept language from the profile.
  std::wstring base_address = gfx::ElideUrl(clean_url, ChromeFont(), 0,
      web_contents->profile()->GetPrefs()->GetString(prefs::kAcceptLanguages));
  // Force URL to have LTR directionality.
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
    l10n_util::WrapStringWithLTRFormatting(&base_address);
  return l10n_util::GetStringF(IDS_JAVASCRIPT_MESSAGEBOX_TITLE, base_address);
}

}

void RunJavascriptMessageBox(WebContents* web_contents,
                             const GURL& frame_url,
                             int dialog_flags,
                             const std::wstring& message_text,
                             const std::wstring& default_prompt_text,
                             bool display_suppress_checkbox,
                             IPC::Message* reply_msg) {
  std::wstring title = GetWindowTitle(web_contents, frame_url);

#if defined(OS_WIN)
  AppModalDialogQueue::AddDialog(new AppModalDialog(web_contents, title,
      dialog_flags, message_text, default_prompt_text,
      display_suppress_checkbox, false, reply_msg));
#else
  NOTIMPLEMENTED();
#endif
}

void RunBeforeUnloadDialog(WebContents* web_contents,
                           const std::wstring& message_text,
                           IPC::Message* reply_msg) {
  std::wstring full_message =
      message_text + L"\n\n" +
      l10n_util::GetString(IDS_BEFOREUNLOAD_MESSAGEBOX_FOOTER);
#if defined(OS_WIN)
  AppModalDialogQueue::AddDialog(new AppModalDialog(
      web_contents, l10n_util::GetString(IDS_BEFOREUNLOAD_MESSAGEBOX_TITLE),
      MessageBox::kIsJavascriptConfirm, message_text, std::wstring(), false,
      true, reply_msg));
#else
  NOTIMPLEMENTED();
#endif
}
