// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_info.h"

#include "base/logging.h"
#include "base/singleton.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/l10n_util.h"

#include "generated_resources.h"

typedef std::list<ChildProcessInfo*> ChildProcessList;


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

ChildProcessInfo::ChildProcessInfo(ProcessType type) {
  // This constructor is only used by objects which derive from this class,
  // which means *this* is a real object that refers to a child process, and not
  // just a simple object that contains information about it.  So add it to our
  // list of running processes.
  type_ = type;
  Singleton<ChildProcessList>::get()->push_back(this);
}


ChildProcessInfo::~ChildProcessInfo() {
  Singleton<ChildProcessList>::get()->remove(this);
}


ChildProcessInfo::Iterator::Iterator() : all_(true) {
  iterator_ = Singleton<ChildProcessList>::get()->begin();
  DCHECK(MessageLoop::current() ==
      ChromeThread::GetMessageLoop(ChromeThread::IO)) <<
          "ChildProcessInfo::Iterator must be used on the IO thread.";
}

ChildProcessInfo::Iterator::Iterator(ProcessType type)
    : all_(false), type_(type) {
  iterator_ = Singleton<ChildProcessList>::get()->begin();
  DCHECK(MessageLoop::current() ==
      ChromeThread::GetMessageLoop(ChromeThread::IO)) <<
          "ChildProcessInfo::Iterator must be used on the IO thread.";
}

ChildProcessInfo* ChildProcessInfo::Iterator::operator++() {
  do {
    ++iterator_;
    if (Done())
      break;

    if (!all_ && (*iterator_)->type() != type_)
      continue;

    return *iterator_;
  } while (true);

  return NULL;
}

bool ChildProcessInfo::Iterator::Done() {
  return iterator_ == Singleton<ChildProcessList>::get()->end();
}
