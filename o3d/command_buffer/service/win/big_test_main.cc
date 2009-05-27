/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <windows.h>
#include <Shellapi.h>
#include "command_buffer/service/cross/big_test_helpers.h"
#include "command_buffer/service/win/d3d9/gapi_d3d9.h"
#include "core/cross/types.h"

namespace o3d {
namespace command_buffer {

String *g_program_path = NULL;
GAPIInterface *g_gapi = NULL;

class Thread {
 public:
  Thread(ThreadFunc func, void *data)
      : handle_(NULL),
        func_(func),
        data_(data) {
  }

  ~Thread() {}

  HANDLE handle() const { return handle_; }
  void set_handle(HANDLE handle) { handle_ = handle; }

  void * data() const { return data_; }
  ThreadFunc func() const { return func_; }

 private:
  HANDLE handle_;
  ThreadFunc func_;
  void * data_;
};

DWORD WINAPI ThreadMain(LPVOID lpParam) {
  Thread *thread = static_cast<Thread *>(lpParam);
  ThreadFunc func = thread->func();
  func(thread->data());
  return 0;
}

Thread *CreateThread(ThreadFunc func, void* param) {
  Thread *thread = new Thread(func, param);
  HANDLE handle = ::CreateThread(NULL, 0, ThreadMain, thread, 0, NULL);
  return thread;
}

void JoinThread(Thread *thread) {
  ::WaitForSingleObject(thread->handle(), INFINITE);
  ::CloseHandle(thread->handle());
  delete thread;
}

bool ProcessSystemMessages() {
  MSG msg;
  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    if (msg.message == WM_QUIT) {
      return false;
    }
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return true;
}

}  // namespace command_buffer
}  // namespace o3d

using o3d::String;
using o3d::command_buffer::g_program_path;
using o3d::command_buffer::g_gapi;
using o3d::command_buffer::GAPID3D9;

LRESULT CALLBACK WindowProc(HWND hWnd,
                            UINT msg,
                            WPARAM wParam,
                            LPARAM lParam) {
  switch (msg) {
    case WM_CLOSE:
      PostQuitMessage(0);
      break;
    case WM_DESTROY:
      PostQuitMessage(0);
      break;
    default:
      return DefWindowProc(hWnd, msg, wParam, lParam);
  }
  return 0;
}

int main(int argc, char *argv[]) {
  WNDCLASSEX wc = {
    sizeof(WNDCLASSEX), CS_CLASSDC, WindowProc, 0L, 0L, GetModuleHandle(NULL),
    NULL, NULL, NULL, NULL, L"O3D big test", NULL
  };
  RegisterClassEx(&wc);

  // Create the application's window.
  HWND hWnd = CreateWindow(L"O3D big test", L"O3D Big Test",
                           WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 300,
                           300, GetDesktopWindow(), NULL, wc.hInstance, NULL);
  UpdateWindow(hWnd);

  GAPID3D9 d3d9_gapi;
  d3d9_gapi.set_hwnd(hWnd);
  g_gapi = &d3d9_gapi;

  wchar_t program_filename[512];
  GetModuleFileName(NULL, program_filename, sizeof(program_filename));
  program_filename[511] = 0;

  String program_path = WideToUTF8(std::wstring(program_filename));

  // Remove all characters starting with last '\'.
  size_t backslash_pos = program_path.rfind('\\');
  if (backslash_pos != String::npos) {
    program_path.erase(backslash_pos);
  }
  g_program_path = &program_path;

  // Convert the command line arguments to an argc, argv format.
  LPWSTR *arg_list = NULL;
  int arg_count;
  arg_list = CommandLineToArgvW(GetCommandLineW(), &arg_count);

  int ret = big_test_main(arg_count, arg_list);

  g_gapi = NULL;
  g_program_path = NULL;
  return ret;
}
