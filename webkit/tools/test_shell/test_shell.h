/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_H_
#define WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_H_

#pragma once

#include <string>
#include <list>

#include "base/basictypes.h"
#include "base/gfx/native_widget_types.h"
#if defined(OS_MACOSX)
#include "base/lazy_instance.h"
#endif
#include "base/ref_counted.h"
#include "webkit/tools/test_shell/event_sending_controller.h"
#include "webkit/tools/test_shell/layout_test_controller.h"
#include "webkit/tools/test_shell/resource.h"
#include "webkit/tools/test_shell/text_input_controller.h"
#include "webkit/tools/test_shell/test_webview_delegate.h"
#include "webkit/tools/test_shell/webview_host.h"
#include "webkit/tools/test_shell/webwidget_host.h"

typedef std::list<gfx::NativeWindow> WindowList;

struct WebPreferences;
class TestNavigationEntry;
class TestNavigationController;

class TestShell {
public:
    struct TestParams {
      // Load the test defaults.
      TestParams() : dump_tree(true), dump_pixels(false) {
      }

      // The kind of output we want from this test.
      bool dump_tree;
      bool dump_pixels;

      // Filename we dump pixels to (when pixel testing is enabled).
      std::wstring pixel_file_name;
      // URL of the test.
      std::string test_url;
    };

    TestShell();
    virtual ~TestShell();

    // Initialization and clean up of logging.
    static void InitLogging(bool suppress_error_dialogs,
                            bool running_layout_tests,
                            bool enable_gp_fault_error_box);
    static void CleanupLogging();

    // Initialization and clean up of a static member variable.
    static void InitializeTestShell(bool layout_test_mode);
    static void ShutdownTestShell();

    static bool layout_test_mode() { return layout_test_mode_; }

    // Called from the destructor to let each platform do any necessary
    // cleanup.
    void PlatformCleanUp();

    WebView* webView() {
      return m_webViewHost.get() ? m_webViewHost->webview() : NULL;
    }
    WebViewHost* webViewHost() { return m_webViewHost.get(); }
    WebWidget* popup() { return m_popupHost ? m_popupHost->webwidget() : NULL; }
    WebWidgetHost* popupHost() { return m_popupHost; }

    // Called by the LayoutTestController to signal test completion.
    void TestFinished();

    // Called to block the calling thread until TestFinished is called.
    void WaitTestFinished();

    void Show(WebView* webview, WindowOpenDisposition disposition);

    // We use this to avoid relying on Windows focus during layout test mode.
    void SetFocus(WebWidgetHost* host, bool enable);

    LayoutTestController* layout_test_controller() {
      return layout_test_controller_.get();
    }
    TestWebViewDelegate* delegate() { return delegate_.get(); }
    TestNavigationController* navigation_controller() {
      return navigation_controller_.get();
    }

    // Resets the LayoutTestController and EventSendingController.  Should be
    // called before loading a page, since some end-editing event notifications
    // may arrive after the previous page has finished dumping its text and
    // therefore end up in the next test's results if the messages are still
    // enabled.
    void ResetTestController() {
      layout_test_controller_->Reset();
      event_sending_controller_->Reset();

      // Reset state in the test webview delegate.
      delegate()->SetSmartInsertDeleteEnabled(true);
#if defined(OS_WIN)
      delegate()->SetSelectTrailingWhitespaceEnabled(true);
#else
      delegate()->SetSelectTrailingWhitespaceEnabled(false);
#endif
    }

    // Passes options from LayoutTestController through to the delegate (or
    // any other caller).
    bool ShouldDumpEditingCallbacks() {
      return layout_test_mode_ &&
             layout_test_controller_->ShouldDumpEditingCallbacks();
    }
    bool ShouldDumpFrameLoadCallbacks() {
      return layout_test_mode_ && (test_is_preparing_ || test_is_pending_) &&
             layout_test_controller_->ShouldDumpFrameLoadCallbacks();
    }
    bool ShouldDumpResourceLoadCallbacks() {
      return layout_test_mode_ && (test_is_preparing_ || test_is_pending_) &&
             layout_test_controller_->ShouldDumpResourceLoadCallbacks();
    }
    bool ShouldDumpTitleChanges() {
      return layout_test_mode_ &&
             layout_test_controller_->ShouldDumpTitleChanges();
    }
    bool AcceptsEditing() {
      return layout_test_controller_->AcceptsEditing();
    }

    void LoadURL(const wchar_t* url);
    void LoadURLForFrame(const wchar_t* url, const wchar_t* frame_name);
    void GoBackOrForward(int offset);
    void Reload();
    bool Navigate(const TestNavigationEntry& entry, bool reload);

    bool PromptForSaveFile(const wchar_t* prompt_title, std::wstring* result);
    std::wstring GetDocumentText();
    void DumpDocumentText();
    void DumpRenderTree();

    gfx::NativeWindow mainWnd() const { return m_mainWnd; }
    gfx::NativeView webViewWnd() const { return m_webViewHost->view_handle(); }
    gfx::NativeEditView editWnd() const { return m_editWnd; }
    gfx::NativeView popupWnd() const { return m_popupHost->view_handle(); }

    static WindowList* windowList() { return window_list_; }

    // If shell is non-null, then *shell is assigned upon successful return
    static bool CreateNewWindow(const std::wstring& startingURL,
                                TestShell** shell = NULL);

    static void DestroyWindow(gfx::NativeWindow windowHandle);

    // Remove the given window from window_list_, return true if it was in the
    // list and was removed and false otherwise.
    static bool RemoveWindowFromList(gfx::NativeWindow window);

    // Implements CreateWebView for TestWebViewDelegate, which in turn
    // is called as a WebViewDelegate.
    WebView* CreateWebView(WebView* webview);
    WebWidget* CreatePopupWidget(WebView* webview);
    void ClosePopup();

#if defined(OS_WIN)
    static ATOM RegisterWindowClass();
#endif

    // Called by the WebView delegate WindowObjectCleared() method, this
    // binds the layout_test_controller_ and other C++ controller classes to
    // window JavaScript objects so they can be accessed by layout tests.
    virtual void BindJSObjectsToWindow(WebFrame* frame);

    // Runs a layout test.  Loads a single file (specified in params.test_url)
    // into the first available window, then dumps the requested text
    // representation to stdout. Returns false if the test cannot be run
    // because no windows are open.
    static bool RunFileTest(const TestParams& params);

    // Writes the back-forward list data for every open window into result.
    static void DumpBackForwardList(std::wstring* result);

    // Dumps the output from given test as text and/or image depending on
    // the flags set.
    static void Dump(TestShell* shell);

    // Writes the image captured from the given web frame to the given file.
    // The returned string is the ASCII-ized MD5 sum of the image.
    static std::string DumpImage(WebFrame* web_frame,
                                 const std::wstring& file_name);

    static void ResetWebPreferences();

    static void SetAllowScriptsToCloseWindows();

    WebPreferences* GetWebPreferences() { return web_prefs_; }

    // Some layout tests hardcode a file:///tmp/LayoutTests URL.  We get around
    // this by substituting "tmp" with the path to the LayoutTests parent dir.
    static std::string RewriteLocalUrl(const std::string& url);

    // Set the timeout for running a test.
    static void SetFileTestTimeout(int timeout_ms) {
      file_test_timeout_ms_ = timeout_ms;
    }

    // Get the timeout for running a test.
    static int GetLayoutTestTimeout() { return file_test_timeout_ms_; }

    // Get the timeout for running a test in seconds
    static int GetLayoutTestTimeoutInSeconds() {
      return file_test_timeout_ms_ / 1000;
    }

#if defined(OS_WIN)
    // Access to the finished event.  Used by the static WatchDog
    // thread.
    HANDLE finished_event() { return finished_event_; }
#endif

    // Have the shell print the StatsTable to stdout on teardown.
    void DumpStatsTableOnExit() { dump_stats_table_on_exit_ = true; }

    void CallJSGC();

    void set_is_modal(bool value) { is_modal_ = value; }
    bool is_modal() const { return is_modal_; }

    const TestParams* test_params() { return test_params_; }
    void set_test_params(const TestParams* test_params) {
      test_params_ = test_params;
    }

#if defined(OS_MACOSX)
    // handle cleaning up a shell given the associated window
    static void DestroyAssociatedShell(gfx::NativeWindow handle);
#endif

    // Show the "attach to me" dialog, for debugging test shell startup.
    static void ShowStartupDebuggingDialog();

protected:
    bool Initialize(const std::wstring& startingURL);
    void SizeToSVG();
    void SizeToDefault();
    void SizeTo(int width, int height);
    void ResizeSubViews();

    // Set the focus in interactive mode (pass through to relevant system call).
    void InteractiveSetFocus(WebWidgetHost* host, bool enable);

#if defined(OS_WIN)
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK EditWndProc(HWND, UINT, WPARAM, LPARAM);
#endif

#if defined(OS_WIN) || defined(OS_MACOSX)
    static void PlatformShutdown();
#endif

protected:
    gfx::NativeWindow       m_mainWnd;
    gfx::NativeEditView     m_editWnd;
    scoped_ptr<WebViewHost> m_webViewHost;
    WebWidgetHost*          m_popupHost;
#if defined(OS_WIN)
    WNDPROC                 default_edit_wnd_proc_;
#endif

    // Primitive focus controller for layout test mode.
    WebWidgetHost*          m_focusedWidgetHost;

private:
    // A set of all our windows.
    static WindowList* window_list_;
#if defined(OS_MACOSX)
    typedef std::map<gfx::NativeWindow, TestShell *> WindowMap;
    static base::LazyInstance<WindowMap> window_map_;
#endif

#if defined(OS_WIN)
    static HINSTANCE instance_handle_;
#endif

    // True when the app is being run using the --layout-tests switch.
    static bool layout_test_mode_;

    // Default timeout in ms for file page loads when in layout test mode.
    static int file_test_timeout_ms_;

    scoped_ptr<LayoutTestController> layout_test_controller_;

    scoped_ptr<EventSendingController> event_sending_controller_;

    scoped_ptr<TextInputController> text_input_controller_;

    scoped_ptr<TestNavigationController> navigation_controller_;

    scoped_refptr<TestWebViewDelegate> delegate_;

    const TestParams* test_params_;

    // True while a test is preparing to run
    bool test_is_preparing_;

    // True while a test is running
    bool test_is_pending_;

    // True if driven from a nested message loop.
    bool is_modal_;

    // The preferences for the test shell.
    static WebPreferences* web_prefs_;

#if defined(OS_WIN)
    // Used by the watchdog to know when it's finished.
    HANDLE finished_event_;
#endif

    // Dump the stats table counters on exit.
    bool dump_stats_table_on_exit_;

#if defined(OS_LINUX)
    // The height of the non-rendering area of the main window, in pixels.
    int top_chrome_height_;
#endif
};

#endif // WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_H_
