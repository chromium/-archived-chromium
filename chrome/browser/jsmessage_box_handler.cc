// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/jsmessage_box_handler.h"

#include "app/gfx/text_elider.h"
#include "app/l10n_util.h"
#include "app/message_box_flags.h"
#include "build/build_config.h"
#include "chrome/browser/app_modal_dialog_queue.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"

namespace {

const size_t kMaxReasonableTextLength = 2048;

// In some platforms, the underlying processing of humongous strings takes too
// long and thus make the UI thread unresponsive.
std::wstring MakeTextSafe(const std::wstring& text) {
  if (text.size() > kMaxReasonableTextLength)
    return text.substr(0, kMaxReasonableTextLength) + L"\x2026";
  return text;
}

std::wstring GetWindowTitle(TabContents* tab_contents, const GURL& frame_url,
                            int dialog_flags) {
  bool is_alert = (dialog_flags == MessageBoxFlags::kIsJavascriptAlert);
  if (!frame_url.has_host())
    return l10n_util::GetString(
        is_alert ? IDS_JAVASCRIPT_ALERT_DEFAULT_TITLE
                 : IDS_JAVASCRIPT_MESSAGEBOX_DEFAULT_TITLE);

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
  std::wstring base_address = gfx::ElideUrl(clean_url, gfx::Font(), 0,
      tab_contents->profile()->GetPrefs()->GetString(prefs::kAcceptLanguages));
  // Force URL to have LTR directionality.
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
    l10n_util::WrapStringWithLTRFormatting(&base_address);
  return l10n_util::GetStringF(
      is_alert ? IDS_JAVASCRIPT_ALERT_TITLE : IDS_JAVASCRIPT_MESSAGEBOX_TITLE,
      base_address);
}

}

void RunJavascriptMessageBox(TabContents* tab_contents,
                             const GURL& frame_url,
                             int dialog_flags,
                             const std::wstring& message_text,
                             const std::wstring& default_prompt_text,
                             bool display_suppress_checkbox,
                             IPC::Message* reply_msg) {
  std::wstring title = GetWindowTitle(tab_contents, frame_url, dialog_flags);
  Singleton<AppModalDialogQueue>()->AddDialog(
      new AppModalDialog(tab_contents, title,
                         dialog_flags, MakeTextSafe(message_text),
                         default_prompt_text, display_suppress_checkbox,
                         false, reply_msg));
}

void RunBeforeUnloadDialog(TabContents* tab_contents,
                           const std::wstring& message_text,
                           IPC::Message* reply_msg) {
  std::wstring full_message =
      message_text + L"\n\n" +
      l10n_util::GetString(IDS_BEFOREUNLOAD_MESSAGEBOX_FOOTER);
  Singleton<AppModalDialogQueue>()->AddDialog(new AppModalDialog(
      tab_contents, l10n_util::GetString(IDS_BEFOREUNLOAD_MESSAGEBOX_TITLE),
      MessageBoxFlags::kIsJavascriptConfirm, MakeTextSafe(message_text),
      std::wstring(), false, true, reply_msg));
}
