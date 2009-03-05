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

//
// NPAPI interactive UI tests.
//

#include "base/file_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/npapi_test_helper.h"
#include "net/base/net_util.h"

const char kTestCompleteCookie[] = "status";
const char kTestCompleteSuccess[] = "OK";
const int kShortWaitTimeout = 5 * 1000;

// Tests if a plugin executing a self deleting script in the context of
// a synchronous mousemove works correctly
TEST_F(NPAPIVisiblePluginTester, SelfDeletePluginInvokeInSynchronousMouseMove) {
  if (!UITest::in_process_renderer()) {
    scoped_ptr<TabProxy> tab_proxy(GetActiveTab());
    HWND tab_window = NULL;
    tab_proxy->GetHWND(&tab_window);

    EXPECT_TRUE(IsWindow(tab_window));

    show_window_ = true;
    std::wstring test_case = L"execute_script_delete_in_mouse_move.html";
    GURL url = GetTestUrl(L"npapi", test_case);
    NavigateToURL(url);

    POINT cursor_position = {130, 130};
    ClientToScreen(tab_window, &cursor_position);

    double screen_width = ::GetSystemMetrics( SM_CXSCREEN ) - 1;
    double screen_height = ::GetSystemMetrics( SM_CYSCREEN ) - 1;
    double location_x =  cursor_position.x * (65535.0f / screen_width);
    double location_y =  cursor_position.y * (65535.0f / screen_height);

    INPUT input_info = {0};
    input_info.type = INPUT_MOUSE;
    input_info.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    input_info.mi.dx = static_cast<long>(location_x);
    input_info.mi.dy = static_cast<long>(location_y);
    ::SendInput(1, &input_info, sizeof(INPUT));

    WaitForFinish("execute_script_delete_in_mouse_move", "1", url,
                  kTestCompleteCookie, kTestCompleteSuccess,
                  kShortWaitTimeout);
  }
}
