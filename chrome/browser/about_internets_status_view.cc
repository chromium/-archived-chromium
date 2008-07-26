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

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/fixed_string.h"
#include "chrome/browser/about_internets_status_view.h"
#include "chrome/browser/tab_contents_delegate.h"

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
  FixedString<wchar_t, MAX_PATH> parameters;
  parameters.Append(path.c_str());
  // Append the handle of the HWND that we want to render the pipes into.
  parameters.Append(L" /p ");
  parameters.Append(
      Int64ToWString(reinterpret_cast<int64>(contents_hwnd)).c_str());
  BOOL result =
      CreateProcess(NULL,
                    parameters.get(),
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
