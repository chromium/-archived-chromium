// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <atlbase.h>
#include <commdlg.h>
#include <objbase.h>
#include <shlwapi.h>
#include <wininet.h>

#include "webkit/tools/test_shell/test_shell.h"

#include "base/gfx/bitmap_platform_device.h"
#include "base/gfx/png_encoder.h"
#include "base/md5.h"
#include "base/memory_debug.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/trace_event.h"
#include "base/win_util.h"
#include "net/url_request/url_request_file_job.h"
#include "webkit/glue/webdatasource.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/webview.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_navigation_controller.h"

#define MAX_LOADSTRING 100

#define BUTTON_WIDTH 72
#define URLBAR_HEIGHT  24

// Global Variables:
static TCHAR g_windowTitle[MAX_LOADSTRING];     // The title bar text
static TCHAR g_windowClass[MAX_LOADSTRING];     // The main window class name

// Forward declarations of functions included in this code module:
static INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

// The W3C SVG layout tests use a different size than the other layout tests
const int kSVGTestWindowWidth = 480;
const int kSVGTestWindowHeight = 360;

// Hide the window offscreen when it is non-interactive.
// This would correspond with a minimized window position if x = y = -32000.
// However we shift the x to 0 to pass test cross-frame-access-put.html
// which expects screenX/screenLeft to be 0 (http://b/issue?id=1227945).
// TODO(ericroman): x should be defined as 0 rather than -4. There is
// probably a frameborder not being accounted for in the setting/getting.
const int kTestWindowXLocation = -4;
const int kTestWindowYLocation = -32000;

// Initialize static member variable
HINSTANCE TestShell::instance_handle_;

/////////////////////////////////////////////////////////////////////////////
// static methods on TestShell

void TestShell::InitializeTestShell(bool interactive) {
  // Start COM stuff.
  HRESULT res = OleInitialize(NULL);
  DCHECK(SUCCEEDED(res));

  window_list_ = new WindowList;
  instance_handle_ = ::GetModuleHandle(NULL);
  interactive_ = interactive;

  web_prefs_ = new WebPreferences;

  ResetWebPreferences();
}

void TestShell::ShutdownTestShell() {
  delete window_list_;
  SimpleResourceLoaderBridge::Shutdown();
  delete TestShell::web_prefs_;
  OleUninitialize();
}

bool TestShell::CreateNewWindow(const std::wstring& startingURL,
                                TestShell** result) {
  TestShell* shell = new TestShell();
  bool rv = shell->Initialize(startingURL);
  if (rv) {
    if (result)
      *result = shell;
    TestShell::windowList()->push_back(shell->m_mainWnd);
  }
  return rv;
}

void TestShell::DestroyWindow(gfx::WindowHandle windowHandle) {
  // Do we want to tear down some of the machinery behind the scenes too?
  ::DestroyWindow(windowHandle);
}

ATOM TestShell::RegisterWindowClass() {
  LoadString(instance_handle_, IDS_APP_TITLE, g_windowTitle, MAX_LOADSTRING);
  LoadString(instance_handle_, IDC_TESTSHELL, g_windowClass, MAX_LOADSTRING);

  WNDCLASSEX wcex = {
    sizeof(WNDCLASSEX),
    CS_HREDRAW | CS_VREDRAW,
    TestShell::WndProc,
    0,
    0,
    instance_handle_,
    LoadIcon(instance_handle_, MAKEINTRESOURCE(IDI_TESTSHELL)),
    LoadCursor(NULL, IDC_ARROW),
    0,
    MAKEINTRESOURCE(IDC_TESTSHELL),
    g_windowClass,
    LoadIcon(instance_handle_, MAKEINTRESOURCE(IDI_SMALL)),
  };
  return RegisterClassEx(&wcex);
}

// static
std::string TestShell::DumpImage(WebFrame* web_frame,
    const std::wstring& file_name) {
  gfx::BitmapPlatformDevice device(web_frame->CaptureImage(true));
  const SkBitmap& src_bmp = device.accessBitmap(false);

  // Encode image.
  std::vector<unsigned char> png;
  SkAutoLockPixels src_bmp_lock(src_bmp); 
  PNGEncoder::Encode(
      reinterpret_cast<const unsigned char*>(src_bmp.getPixels()),
      PNGEncoder::FORMAT_BGRA, src_bmp.width(), src_bmp.height(),
      static_cast<int>(src_bmp.rowBytes()), true, &png);

  // Write to disk.
  FILE* file = NULL;
  if (_wfopen_s(&file, file_name.c_str(), L"wb") == 0) {
    fwrite(&png[0], 1, png.size(), file);
    fclose(file);
  }

  // Compute MD5 sum.
  MD5Context ctx;
  MD5Init(&ctx);
  MD5Update(&ctx, src_bmp.getPixels(), src_bmp.getSize());

  MD5Digest digest;
  MD5Final(&digest, &ctx);
  return MD5DigestToBase16(digest);
}

void TestShell::DumpBackForwardList(std::wstring* result) {
  result->clear();
  for (WindowList::iterator iter = TestShell::windowList()->begin();
     iter != TestShell::windowList()->end(); iter++) {
    HWND hwnd = *iter;
    TestShell* shell =
        static_cast<TestShell*>(win_util::GetWindowUserData(hwnd));
    webkit_glue::DumpBackForwardList(shell->webView(), NULL, result);
  }
}

bool TestShell::RunFileTest(const char *filename, const TestParams& params) {
  // Load the test file into the first available window.
  if (TestShell::windowList()->empty()) {
    LOG(ERROR) << "No windows open.";
    return false;
  }

  HWND hwnd = *(TestShell::windowList()->begin());
  TestShell* shell =
      static_cast<TestShell*>(win_util::GetWindowUserData(hwnd));
  shell->ResetTestController();

  // ResetTestController may have closed the window we were holding on to. 
  // Grab the first window again.
  hwnd = *(TestShell::windowList()->begin());
  shell = static_cast<TestShell*>(win_util::GetWindowUserData(hwnd));
  DCHECK(shell);

  // Clear focus between tests.
  shell->m_focusedWidgetHost = NULL;

  // Make sure the previous load is stopped.
  shell->webView()->StopLoading();
  shell->navigation_controller()->Reset();

  // Clean up state between test runs.
  webkit_glue::ResetBeforeTestRun(shell->webView());
  ResetWebPreferences();
  shell->webView()->SetPreferences(*web_prefs_);

  SetWindowPos(shell->m_mainWnd, NULL,
               kTestWindowXLocation, kTestWindowYLocation, 0, 0,
               SWP_NOSIZE | SWP_NOZORDER);
  shell->ResizeSubViews();

  if (strstr(filename, "loading/") || strstr(filename, "loading\\"))
    shell->layout_test_controller()->SetShouldDumpFrameLoadCallbacks(true);

  shell->test_is_preparing_ = true;

  std::wstring wstr = UTF8ToWide(filename);
  shell->LoadURL(wstr.c_str());

  shell->test_is_preparing_ = false;
  shell->WaitTestFinished();

  // Echo the url in the output so we know we're not getting out of sync.
  printf("#URL:%s\n", filename);

  // Dump the requested representation.
  WebFrame* webFrame = shell->webView()->GetMainFrame();
  if (webFrame) {
    bool should_dump_as_text =
        shell->layout_test_controller_->ShouldDumpAsText();
    bool dumped_anything = false;
    if (params.dump_tree) {
      dumped_anything = true;
      // Text output: the test page can request different types of output
      // which we handle here.
      if (!should_dump_as_text) {
        // Plain text pages should be dumped as text
        std::wstring mime_type = webFrame->GetDataSource()->GetResponseMimeType();
        should_dump_as_text = (mime_type == L"text/plain");
      }
      if (should_dump_as_text) {
        bool recursive = shell->layout_test_controller_->
            ShouldDumpChildFramesAsText();
        std::string data_utf8 = WideToUTF8(
            webkit_glue::DumpFramesAsText(webFrame, recursive));
        fwrite(data_utf8.c_str(), 1, data_utf8.size(), stdout);
      } else {
        printf("%s", WideToUTF8(
            webkit_glue::DumpRenderer(webFrame)).c_str());

        bool recursive = shell->layout_test_controller_->
            ShouldDumpChildFrameScrollPositions();
        printf("%s", WideToUTF8(
            webkit_glue::DumpFrameScrollPosition(webFrame, recursive)).c_str());
      }

      if (shell->layout_test_controller_->ShouldDumpBackForwardList()) {
        std::wstring bfDump;
        DumpBackForwardList(&bfDump);
        printf("%s", WideToUTF8(bfDump).c_str());
      }
    }
    
    if (params.dump_pixels && !should_dump_as_text) {
      // Image output: we write the image data to the file given on the
      // command line (for the dump pixels argument), and the MD5 sum to
      // stdout.
      dumped_anything = true;
      std::string md5sum = DumpImage(webFrame, params.pixel_file_name);
      printf("#MD5:%s\n", md5sum.c_str());
    }
    if (dumped_anything)
      printf("#EOF\n");
    fflush(stdout);
  }

  return true;
}

std::string TestShell::RewriteLocalUrl(const std::string& url) {
  // Convert file:///tmp/LayoutTests urls to the actual location on disk.
  const char kPrefix[] = "file:///tmp/LayoutTests/";
  const int kPrefixLen = arraysize(kPrefix) - 1;

  std::string new_url(url);
  if (url.compare(0, kPrefixLen, kPrefix, kPrefixLen) == 0) {
    std::wstring replace_url;
    PathService::Get(base::DIR_EXE, &replace_url);
    file_util::UpOneDirectory(&replace_url);
    file_util::UpOneDirectory(&replace_url);
    file_util::AppendToPath(&replace_url, L"webkit");
    file_util::AppendToPath(&replace_url, L"data");
    file_util::AppendToPath(&replace_url, L"layout_tests");
    file_util::AppendToPath(&replace_url, L"LayoutTests");
    replace_url.push_back(file_util::kPathSeparator);
    new_url = std::string("file:///") +
              WideToUTF8(replace_url).append(url.substr(kPrefixLen));
  }
  return new_url;
}



/////////////////////////////////////////////////////////////////////////////
// TestShell implementation

void TestShell::PlatformCleanUp() {
  // When the window is destroyed, tell the Edit field to forget about us, 
  // otherwise we will crash. 
  win_util::SetWindowProc(m_editWnd, default_edit_wnd_proc_);
  win_util::SetWindowUserData(m_editWnd, NULL);
}

bool TestShell::Initialize(const std::wstring& startingURL) {
  // Perform application initialization:
  m_mainWnd = CreateWindow(g_windowClass, g_windowTitle,
                           WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                           CW_USEDEFAULT, 0, CW_USEDEFAULT, 0,
                           NULL, NULL, instance_handle_, NULL);
  win_util::SetWindowUserData(m_mainWnd, this);

  HWND hwnd;
  int x = 0;
  
  hwnd = CreateWindow(L"BUTTON", L"Back",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON ,
                      x, 0, BUTTON_WIDTH, URLBAR_HEIGHT,
                      m_mainWnd, (HMENU) IDC_NAV_BACK, instance_handle_, 0);
  x += BUTTON_WIDTH;

  hwnd = CreateWindow(L"BUTTON", L"Forward",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON ,
                      x, 0, BUTTON_WIDTH, URLBAR_HEIGHT,
                      m_mainWnd, (HMENU) IDC_NAV_FORWARD, instance_handle_, 0);
  x += BUTTON_WIDTH;

  hwnd = CreateWindow(L"BUTTON", L"Reload",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON ,
                      x, 0, BUTTON_WIDTH, URLBAR_HEIGHT,
                      m_mainWnd, (HMENU) IDC_NAV_RELOAD, instance_handle_, 0);
  x += BUTTON_WIDTH;

  hwnd = CreateWindow(L"BUTTON", L"Stop",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON ,
                      x, 0, BUTTON_WIDTH, URLBAR_HEIGHT,
                      m_mainWnd, (HMENU) IDC_NAV_STOP, instance_handle_, 0);
  x += BUTTON_WIDTH;

  // this control is positioned by ResizeSubViews
  m_editWnd = CreateWindow(L"EDIT", 0,
                           WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT |
                           ES_AUTOVSCROLL | ES_AUTOHSCROLL, 
                           x, 0, 0, 0, m_mainWnd, 0, instance_handle_, 0);

  default_edit_wnd_proc_ =
      win_util::SetWindowProc(m_editWnd, TestShell::EditWndProc);
  win_util::SetWindowUserData(m_editWnd, this);

  // create webview
  m_webViewHost.reset(
      WebViewHost::Create(m_mainWnd, delegate_.get(), *TestShell::web_prefs_));
  webView()->SetUseEditorDelegate(true);
  delegate_->RegisterDragDrop();
  
  // Load our initial content.
  if (!startingURL.empty())
    LoadURL(startingURL.c_str());

  ShowWindow(webViewWnd(), SW_SHOW);

  bool bIsSVGTest = startingURL.find(L"W3C-SVG-1.1") != std::wstring::npos;

  if (bIsSVGTest) {
    SizeTo(kSVGTestWindowWidth, kSVGTestWindowHeight);
  } else {
    SizeToDefault();
  }

  return true;
}

void TestShell::TestFinished() {
  if (!test_is_pending_)
    return;  // reached when running under test_shell_tests

  UINT_PTR timer_id = reinterpret_cast<UINT_PTR>(this);
  KillTimer(mainWnd(), timer_id);

  test_is_pending_ = false;
  MessageLoop::current()->Quit();
}

// Thread main to run for the thread which just tests for timeout.
unsigned int __stdcall WatchDogThread(void *arg) {
  // If we're debugging a layout test, don't timeout.
  if (::IsDebuggerPresent())
    return 0;

  TestShell* shell = static_cast<TestShell*>(arg);
  DWORD timeout = static_cast<DWORD>(shell->GetFileTestTimeout() * 2.5);
  DWORD rv = WaitForSingleObject(shell->finished_event(), timeout);
  if (rv == WAIT_TIMEOUT) {
    // Print a warning to be caught by the layout-test script.
    // Note: the layout test driver may or may not recognize
    // this as a timeout.
    puts("#TEST_TIMED_OUT\n");
    puts("#EOF\n");
    fflush(stdout);
    TerminateProcess(GetCurrentProcess(), 0);
  }
  // Finished normally.
  return 0;
}

void TestShell::WaitTestFinished() {
  DCHECK(!test_is_pending_) << "cannot be used recursively";

  test_is_pending_ = true;

  // Create a watchdog thread which just sets a timer and
  // kills the process if it times out.  This catches really
  // bad hangs where the shell isn't coming back to the 
  // message loop.  If the watchdog is what catches a 
  // timeout, it can't do anything except terminate the test
  // shell, which is unfortunate.
  finished_event_ = CreateEvent(NULL, TRUE, FALSE, NULL);
  DCHECK(finished_event_ != NULL);

  HANDLE thread_handle = reinterpret_cast<HANDLE>(_beginthreadex(
                                 NULL,
                                 0,
                                 &WatchDogThread,
                                 this,
                                 0,
                                 0));
  DCHECK(thread_handle != NULL);

  // TestFinished() will post a quit message to break this loop when the page
  // finishes loading.
  while (test_is_pending_)
    MessageLoop::current()->Run();

  // Tell the watchdog that we are finished.
  SetEvent(finished_event_);

  // Wait to join the watchdog thread.  (up to 1s, then quit)
  WaitForSingleObject(thread_handle, 1000);  
}

void TestShell::SetFocus(WebWidgetHost* host, bool enable) {
  if (interactive_) {
    if (enable) {
      ::SetFocus(host->window_handle());
    } else {
      if (GetFocus() == host->window_handle())
        ::SetFocus(NULL);
    }
  } else {
    if (enable) {
      if (m_focusedWidgetHost != host) {
        if (m_focusedWidgetHost)
            m_focusedWidgetHost->webwidget()->SetFocus(false);
         host->webwidget()->SetFocus(enable);
         m_focusedWidgetHost = host;
      }
    } else {
      if (m_focusedWidgetHost == host) {
        host->webwidget()->SetFocus(enable);
        m_focusedWidgetHost = NULL;
      }
    }
  }
}

WebWidget* TestShell::CreatePopupWidget(WebView* webview) {
  DCHECK(!m_popupHost);
  m_popupHost = WebWidgetHost::Create(NULL, delegate_.get());
  ShowWindow(popupWnd(), SW_SHOW);

  return m_popupHost->webwidget();
}

void TestShell::ClosePopup() {
  PostMessage(popupWnd(), WM_CLOSE, 0, 0);
  m_popupHost = NULL;
}

void TestShell::SizeTo(int width, int height) {
  RECT rc, rw;
  GetClientRect(m_mainWnd, &rc);
  GetWindowRect(m_mainWnd, &rw);

  int client_width = rc.right - rc.left;
  int window_width = rw.right - rw.left;
  window_width = (window_width - client_width) + width;

  int client_height = rc.bottom - rc.top;
  int window_height = rw.bottom - rw.top;
  window_height = (window_height - client_height) + height;

  // add space for the url bar:
  window_height += URLBAR_HEIGHT;

  SetWindowPos(m_mainWnd, NULL, 0, 0, window_width, window_height,
               SWP_NOMOVE | SWP_NOZORDER);
}

void TestShell::ResizeSubViews() {
  RECT rc;
  GetClientRect(m_mainWnd, &rc);

  int x = BUTTON_WIDTH * 4;
  MoveWindow(m_editWnd, x, 0, rc.right - x, URLBAR_HEIGHT, TRUE);

  MoveWindow(webViewWnd(), 0, URLBAR_HEIGHT, rc.right,
             rc.bottom - URLBAR_HEIGHT, TRUE);
}

void TestShell::LoadURLForFrame(const wchar_t* url,
                                const wchar_t* frame_name) {
  if (!url)
      return;

  TRACE_EVENT_BEGIN("url.load", this, WideToUTF8(url));
  bool bIsSVGTest = wcsstr(url, L"W3C-SVG-1.1") > 0;

  if (bIsSVGTest) {
    SizeTo(kSVGTestWindowWidth, kSVGTestWindowHeight);
  } else {
    SizeToDefault();
  }

  std::wstring urlString(url);
  if (!urlString.empty() && (PathFileExists(url) || PathIsUNC(url))) {
      TCHAR fileURL[INTERNET_MAX_URL_LENGTH];
      DWORD fileURLLength = sizeof(fileURL)/sizeof(fileURL[0]);
      if (SUCCEEDED(UrlCreateFromPath(url, fileURL, &fileURLLength, 0)))
          urlString.assign(fileURL);
  }

  std::wstring frame_string;
  if (frame_name)
      frame_string = frame_name;

  navigation_controller_->LoadEntry(new TestNavigationEntry(
      -1, GURL(urlString), std::wstring(), frame_string));
}

LRESULT CALLBACK TestShell::WndProc(HWND hwnd, UINT message, WPARAM wParam,
                                    LPARAM lParam) {
  TestShell* shell = static_cast<TestShell*>(win_util::GetWindowUserData(hwnd));

  switch (message) {
  case WM_COMMAND:
    {
      int wmId    = LOWORD(wParam);
      int wmEvent = HIWORD(wParam);

      switch (wmId) {
      case IDM_ABOUT:
        DialogBox(shell->instance_handle_, MAKEINTRESOURCE(IDD_ABOUTBOX), hwnd,
                  About);
        break;
      case IDM_EXIT:
        DestroyWindow(hwnd);
        break;
      case IDC_NAV_BACK:
        shell->GoBackOrForward(-1);
        break;
      case IDC_NAV_FORWARD:
        shell->GoBackOrForward(1);
        break;
      case IDC_NAV_RELOAD:
      case IDC_NAV_STOP:
        {
          if (wmId == IDC_NAV_RELOAD) {
            shell->Reload();
          } else {
            shell->webView()->StopLoading();
          }
        }
        break;
      case IDM_DUMP_BODY_TEXT:
        shell->DumpDocumentText();
        break;
      case IDM_DUMP_RENDER_TREE:
        shell->DumpRenderTree();
        break;
      case IDM_SHOW_WEB_INSPECTOR:
        shell->webView()->InspectElement(0, 0);
        break;
      }
    }
    break;

  case WM_DESTROY:
    {
      // Dump all in use memory just before shutdown if in use memory
      // debugging has been enabled.
      base::MemoryDebug::DumpAllMemoryInUse();

      WindowList::iterator entry =
          std::find(TestShell::windowList()->begin(),
                    TestShell::windowList()->end(), hwnd);
      if (entry != TestShell::windowList()->end())
        TestShell::windowList()->erase(entry);

      if (TestShell::windowList()->empty() || shell->is_modal()) {
        MessageLoop::current()->PostTask(FROM_HERE,
                                         new MessageLoop::QuitTask());
      }
      delete shell;
    }
    return 0;

  case WM_SIZE:
    if (shell->webView())
      shell->ResizeSubViews();
    return 0;
  }

  return DefWindowProc(hwnd, message, wParam, lParam);
}


#define MAX_URL_LENGTH  1024

LRESULT CALLBACK TestShell::EditWndProc(HWND hwnd, UINT message,
                                        WPARAM wParam, LPARAM lParam) {
  TestShell* shell =
      static_cast<TestShell*>(win_util::GetWindowUserData(hwnd));

  switch (message) {
    case WM_CHAR:
      if (wParam == VK_RETURN) {
        wchar_t strPtr[MAX_URL_LENGTH];
        *((LPWORD)strPtr) = MAX_URL_LENGTH; 
        LRESULT strLen = SendMessage(hwnd, EM_GETLINE, 0, (LPARAM)strPtr);
        if (strLen > 0)
          shell->LoadURL(strPtr);

        return 0;
      }
  }

  return (LRESULT) CallWindowProc(shell->default_edit_wnd_proc_, hwnd,
                                  message, wParam, lParam);
}


// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
  UNREFERENCED_PARAMETER(lParam);
  switch (message) {
  case WM_INITDIALOG:
    return (INT_PTR)TRUE;

  case WM_COMMAND:
    if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
      EndDialog(hDlg, LOWORD(wParam));
      return (INT_PTR)TRUE;
    }
    break;
  }
  return (INT_PTR)FALSE;
}

bool TestShell::PromptForSaveFile(const wchar_t* prompt_title,
                                  std::wstring* result) {
  wchar_t path_buf[MAX_PATH] = L"data.txt";

  OPENFILENAME info = {0};
  info.lStructSize = sizeof(info);
  info.hwndOwner = m_mainWnd;
  info.hInstance = instance_handle_;
  info.lpstrFilter = L"*.txt";
  info.lpstrFile = path_buf;
  info.nMaxFile = arraysize(path_buf);
  info.lpstrTitle = prompt_title;
  if (!GetSaveFileName(&info))
    return false;

  result->assign(info.lpstrFile);
  return true;
}

// TODO(tc): Use methods in base or net/base for this.
static void WriteTextToFile(const std::wstring& data,
                            const std::wstring& file_path) {
  FILE* fp;
  errno_t err = _wfopen_s(&fp, file_path.c_str(), L"wt");
  if (err)
    return;
  std::string data_utf8 = WideToUTF8(data);
  fwrite(data_utf8.c_str(), 1, data_utf8.size(), fp);
  fclose(fp);
}

void TestShell::DumpDocumentText() {
  std::wstring file_path;
  if (!PromptForSaveFile(L"Dump document text", &file_path))
    return;

  WriteTextToFile(webkit_glue::DumpDocumentText(webView()->GetMainFrame()),
                  file_path);
}

void TestShell::DumpRenderTree() {
  std::wstring file_path;
  if (!PromptForSaveFile(L"Dump render tree", &file_path))
    return;

  WriteTextToFile(webkit_glue::DumpRenderer(webView()->GetMainFrame()),
                  file_path);
}

/////////////////////////////////////////////////////////////////////////////
// WebKit glue functions

namespace webkit_glue {

IMLangFontLink2* GetLangFontLink() {
  return webkit_glue::GetLangFontLinkHelper();
}

std::wstring GetLocalizedString(int message_id) {
  const ATLSTRINGRESOURCEIMAGE* image =
      AtlGetStringResourceImage(_AtlBaseModule.GetModuleInstance(),
                                message_id);
  if (!image) {
    NOTREACHED();
    return L"No string for this identifier!";
  }
  return std::wstring(image->achString, image->nLength);
}

HCURSOR LoadCursor(int cursor_id) {
  return NULL;
}

bool EnsureFontLoaded(HFONT font) {
  return true;
}

MONITORINFOEX GetMonitorInfoForWindow(HWND window) {
  return webkit_glue::GetMonitorInfoForWindowHelper(window);
}

bool DownloadUrl(const std::string& url, HWND caller_window) {
  return false;
}

}  // namespace webkit_glue
