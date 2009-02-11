// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_info.h"

#include "base/logging.h"
#include "chrome/common/l10n_util.h"

#include "generated_resources.h"

std::wstring ChildProcessInfo::GetTypeNameInEnglish(
    ChildProcessInfo::ProcessType type) {
  switch (type) {
   case BROWSER_PROCESS:
     return L"Browser";
   case RENDER_PROCESS:
     return L"Tab";
   case PLUGIN_PROCESS:
     return L"Plug-in";
   case WORKER_PROCESS:
     return L"Web Worker";
   case UNKNOWN_PROCESS:
   default:
     DCHECK(false) << "Unknown child process type!";
     return L"Unknown";
  }
}

std::wstring ChildProcessInfo::GetLocalizedTitle() const {
  std::wstring title = name_;
  if (type_ == ChildProcessInfo::PLUGIN_PROCESS && title.empty())
    title = l10n_util::GetString(IDS_TASK_MANAGER_UNKNOWN_PLUGIN_NAME);

  int message_id;
  if (type_ == ChildProcessInfo::PLUGIN_PROCESS) {
    message_id = IDS_TASK_MANAGER_PLUGIN_PREFIX;
  } else if (type_ == ChildProcessInfo::WORKER_PROCESS) {
    message_id = IDS_TASK_MANAGER_WORKER_PREFIX;
  } else {
    DCHECK(false) << "Need localized name for child process type.";
    return title;
  }

  // Explicitly mark name as LTR if there is no strong RTL character,
  // to avoid the wrong concatenation result similar to "!Yahoo! Mail: the
  // best web-based Email: NIGULP", in which "NIGULP" stands for the Hebrew
  // or Arabic word for "plugin".
  l10n_util::AdjustStringForLocaleDirection(title, &title);
  return l10n_util::GetStringF(message_id, title);
}
