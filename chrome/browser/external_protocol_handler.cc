// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/external_protocol_handler.h"

#include <windows.h>
#include <shellapi.h>
#include <set>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/registry.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/views/external_protocol_dialog.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/pref_names.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"

// static
void ExternalProtocolHandler::PrepopulateDictionary(DictionaryValue* win_pref) {
  static bool is_warm = false;
  if (is_warm)
    return;
  is_warm = true;

  static const wchar_t* const denied_schemes[] = {
    L"afp",
    L"data",
    L"disk",
    L"disks",
    // ShellExecuting file:///C:/WINDOWS/system32/notepad.exe will simply
    // execute the file specified!  Hopefully we won't see any "file" schemes
    // because we think of file:// URLs as handled URLs, but better to be safe
    // than to let an attacker format the user's hard drive.
    L"file",
    L"hcp",
    L"javascript",
    L"ms-help",
    L"nntp",
    L"shell",
    L"vbscript",
    // view-source is a special case in chrome. When it comes through an
    // iframe or a redirect, it looks like an external protocol, but we don't
    // want to shellexecute it.
    L"view-source",
    L"vnd.ms.radio",
  };

  static const wchar_t* const allowed_schemes[] = {
    L"mailto",
    L"news",
    L"snews",
  };

  bool should_block;
  for (int i = 0; i < arraysize(denied_schemes); ++i) {
    if (!win_pref->GetBoolean(denied_schemes[i], &should_block)) {
      win_pref->SetBoolean(denied_schemes[i], true);
    }
  }

  for (int i = 0; i < arraysize(allowed_schemes); ++i) {
    if (!win_pref->GetBoolean(allowed_schemes[i], &should_block)) {
      win_pref->SetBoolean(allowed_schemes[i], false);
    }
  }
}

// static
ExternalProtocolHandler::BlockState ExternalProtocolHandler::GetBlockState(
    const std::wstring& scheme) {
  if (scheme.length() == 1) {
    // We have a URL that looks something like:
    //   C:/WINDOWS/system32/notepad.exe
    // ShellExecuting this URL will cause the specified program to be executed.
    return BLOCK;
  }

  // Check the stored prefs.
  // TODO(pkasting): http://b/119651 This kind of thing should go in the
  // preferences on the profile, not in the local state.
  PrefService* pref = g_browser_process->local_state();
  if (pref) {  // May be NULL during testing.
    DictionaryValue* win_pref =
        pref->GetMutableDictionary(prefs::kExcludedSchemes);
    CHECK(win_pref);

    // Warm up the dictionary if needed.
    PrepopulateDictionary(win_pref);

    bool should_block;
    if (win_pref->GetBoolean(scheme, &should_block))
      return should_block ? BLOCK : DONT_BLOCK;
  }

  return UNKNOWN;
}

// static
void ExternalProtocolHandler::LaunchUrl(const GURL& url,
                                        int render_process_host_id,
                                        int tab_contents_id) {
  // Escape the input scheme to be sure that the command does not
  // have parameters unexpected by the external program.
  std::string escaped_url_string = EscapeExternalHandlerValue(url.spec());
  GURL escaped_url(escaped_url_string);
  BlockState block_state = GetBlockState(ASCIIToWide(escaped_url.scheme()));
  if (block_state == BLOCK)
    return;

  if (block_state == UNKNOWN) {
    std::wstring command = ExternalProtocolDialog::GetApplicationForProtocol(
                               escaped_url);
    if (command.empty()) {
      // ShellExecute won't do anything. Don't bother warning the user.
      return;
    }

    // Ask the user if they want to allow the protocol. This will call
    // LaunchUrlWithoutSecurityCheck if the user decides to accept the protocol.
    ExternalProtocolDialog::RunExternalProtocolDialog(escaped_url,
                                                      command,
                                                      render_process_host_id,
                                                      tab_contents_id);
    return;
  }

  // Put this work on the file thread since ShellExecute may block for a
  // significant amount of time.
  MessageLoop* loop = g_browser_process->file_thread()->message_loop();
  if (loop == NULL) {
    return;
  }

  // Otherwise the protocol is white-listed, so go ahead and launch.
  loop->PostTask(FROM_HERE,
      NewRunnableFunction(
          &ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck,
          escaped_url));
}

// static
void ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(const GURL& url) {
  // Quote the input scheme to be sure that the command does not have
  // parameters unexpected by the external program. This url should already
  // have been escaped.
  std::string escaped_url = url.spec();
  escaped_url.insert(0, "\"");
  escaped_url += "\"";

  // According to Mozilla in uriloader/exthandler/win/nsOSHelperAppService.cpp:
  // "Some versions of windows (Win2k before SP3, Win XP before SP1) crash in
  // ShellExecute on long URLs (bug 161357 on bugzilla.mozilla.org). IE 5 and 6
  // support URLS of 2083 chars in length, 2K is safe."
  const int kMaxUrlLength = 2048;
  if (escaped_url.length() > kMaxUrlLength) {
    NOTREACHED();
    return;
  }

  RegKey key;
  std::wstring registry_path = ASCIIToWide(url.scheme()) +
                               L"\\shell\\open\\command";
  key.Open(HKEY_CLASSES_ROOT, registry_path.c_str());
  if (key.Valid()) {
    DWORD size = 0;
    key.ReadValue(NULL, NULL, &size);
    if (size <= 2) {
      // ShellExecute crashes the process when the command is empty.
      // We check for "2" because it always returns the trailing NULL.
      // TODO(nsylvain): we should also add a dialog to warn on errors. See
      // bug 1136923.
      return;
    }
  }

  if (reinterpret_cast<ULONG_PTR>(ShellExecuteA(NULL, "open",
                                                escaped_url.c_str(), NULL, NULL,
                                                SW_SHOWNORMAL)) <= 32) {
    // We fail to execute the call. We could display a message to the user.
    // TODO(nsylvain): we should also add a dialog to warn on errors. See
    // bug 1136923.
    return;
  }
}

// static
void ExternalProtocolHandler::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kExcludedSchemes);
}
