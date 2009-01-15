// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/tab_contents/about_internets_status_view.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"

AboutInternetsStatusView::AboutInternetsStatusView()
    : StatusView(TAB_CONTENTS_ABOUT_INTERNETS_STATUS_VIEW) {}

AboutInternetsStatusView::~AboutInternetsStatusView() {
  if (process_handle_.IsValid())
    TerminateProcess(process_handle_.Get(), 0);
}

const std::wstring AboutInternetsStatusView::GetDefaultTitle() const {
  return L"Don't Clog the Tubes!";
}

const std::wstring& AboutInternetsStatusView::GetTitle() const {
  return title_;
}

void AboutInternetsStatusView::OnCreate(const CRect& rect) {
  HWND contents_hwnd = GetContainerHWND();
  STARTUPINFO startup_info;
  memset(&startup_info, 0, sizeof(startup_info));
  startup_info.cb = sizeof(startup_info);
  PROCESS_INFORMATION process_info = {0};

  std::wstring path;
  PathService::Get(base::DIR_SYSTEM, &path);
  file_util::AppendToPath(&path, L"sspipes.scr");
  std::wstring parameters;
  parameters.append(path.c_str());
  // Append the handle of the HWND that we want to render the pipes into.
  parameters.append(L" /p ");
  parameters.append(
      Int64ToWString(reinterpret_cast<int64>(contents_hwnd)).c_str());
  BOOL result =
      CreateProcess(NULL,
                    const_cast<LPWSTR>(parameters.c_str()),
                    NULL,           // LPSECURITY_ATTRIBUTES lpProcessAttributes
                    NULL,           // LPSECURITY_ATTRIBUTES lpThreadAttributes
                    FALSE,          // BOOL bInheritHandles
                    CREATE_DEFAULT_ERROR_MODE,  // DWORD dwCreationFlags
                    NULL,           // LPVOID lpEnvironment
                    NULL,           // LPCTSTR lpCurrentDirectory
                    &startup_info,   // LPstartup_info lpstartup_info
                    &process_info);  // LPPROCESS_INFORMATION
                                      // lpProcessInformation

  if (result) {
    title_ = GetDefaultTitle();
    CloseHandle(process_info.hThread);
    process_handle_.Set(process_info.hProcess);
  } else {
    title_ = L"The Tubes are Clogged!";
  }
}

void AboutInternetsStatusView::OnSize(const CRect& rect) {
  // We're required to implement this because it is abstract, but we don't
  // actually have anything to do right here.
}

