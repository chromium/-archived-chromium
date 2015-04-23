// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_shell.h"

#include <windows.h>
#include <atlbase.h>
#include <commdlg.h>
#include <objbase.h>
#include <shlwapi.h>
#include <wininet.h>  // For INTERNET_MAX_URL_LENGTH

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/memory_debug.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/resource_util.h"
#include "base/stack_container.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/trace_event.h"
#include "base/win_util.h"
#include "breakpad/src/client/windows/handler/exception_handler.h"
#include "grit/webkit_resources.h"
#include "net/base/net_module.h"
#include "net/url_request/url_request_file_job.h"
#include "skia/ext/bitmap_platform_device.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/webview.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/tools/test_shell/resource.h"
#include "webkit/tools/test_shell/test_navigation_controller.h"
#include "webkit/tools/test_shell/test_shell_switches.h"

#define MAX_LOADSTRING 100

#define BUTTON_WIDTH 72
#define URLBAR_HEIGHT  24

// Global Variables:
static TCHAR g_windowTitle[MAX_LOADSTRING];     // The title bar text
static TCHAR g_windowClass[MAX_LOADSTRING];     // The main window class name

// This is only set for layout tests.  It is used to determine the name of a
// minidump file.
static const size_t kPathBufSize = 2048;
static wchar_t g_currentTestName[kPathBufSize];

// Forward declarations of functions included in this code module:
static INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

// Hide the window offscreen when in layout test mode.
// This would correspond with a minimized window position if x = y = -32000.
// However we shift the x to 0 to pass test cross-frame-access-put.html
// which expects screenX/screenLeft to be 0 (http://b/issue?id=1227945).
// TODO(ericroman): x should be defined as 0 rather than -4. There is
// probably a frameborder not being accounted for in the setting/getting.
const int kTestWindowXLocation = -4;
const int kTestWindowYLocation = -32000;

namespace {

// This method is used to keep track of the current test name so when we write
// a minidump file, we have the test name in the minidump filename.
void SetCurrentTestName(const char* path) {
  const char* lastSlash = strrchr(path, '/');
  if (lastSlash) {
    ++lastSlash;
  } else {
    lastSlash = path;
  }

  base::wcslcpy(g_currentTestName,
                UTF8ToWide(lastSlash).c_str(),
                arraysize(g_currentTestName));
}

bool MinidumpCallback(const wchar_t *dumpPath,
                      const wchar_t *minidumpID,
                      void *context,
                      EXCEPTION_POINTERS *exinfo,
                      MDRawAssertionInfo *assertion,
                      bool succeeded) {
  // Warning: Don't use the heap in this function.  It may be corrupted.
  if (!g_currentTestName[0])
    return false;

  // Try to rename the minidump file to include the crashed test's name.
  // StackString uses the stack but overflows onto the heap.  But we don't
  // care too much about being completely correct here, since most crashes
  // will be happening on developers' machines where they have debuggers.
  StackWString<kPathBufSize * 2> origPath;
  origPath->append(dumpPath);
  origPath->push_back(FilePath::kSeparators[0]);
  origPath->append(minidumpID);
  origPath->append(L".dmp");

  StackWString<kPathBufSize * 2> newPath;
  newPath->append(dumpPath);
  newPath->push_back(FilePath::kSeparators[0]);
  newPath->append(g_currentTestName);
  newPath->append(L"-");
  newPath->append(minidumpID);
  newPath->append(L".dmp");

  // May use the heap, but oh well.  If this fails, we'll just have the
  // original dump file lying around.
  _wrename(origPath->c_str(), newPath->c_str());

  return false;
}

// Helper method for getting the path to the test shell resources directory.
FilePath GetResourcesFilePath() {
  FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.AppendASCII("webkit");
  path = path.AppendASCII("tools");
  path = path.AppendASCII("test_shell");
  return path.AppendASCII("resources");
}

static StringPiece GetRawDataResource(HMODULE module, int resource_id) {
  void* data_ptr;
  size_t data_size;
  return base::GetDataResourceFromModule(module, resource_id, &data_ptr,
                                         &data_size) ?
      StringPiece(static_cast<char*>(data_ptr), data_size) : StringPiece();
}

// This is called indirectly by the network layer to access resources.
StringPiece NetResourceProvider(int key) {
  return GetRawDataResource(::GetModuleHandle(NULL), key);
}

}  // namespace

// Initialize static member variable
HINSTANCE TestShell::instance_handle_;

/////////////////////////////////////////////////////////////////////////////
// static methods on TestShell

void TestShell::InitializeTestShell(bool layout_test_mode) {
  // Start COM stuff.
  HRESULT res = OleInitialize(NULL);
  DCHECK(SUCCEEDED(res));

  window_list_ = new WindowList;
  instance_handle_ = ::GetModuleHandle(NULL);
  layout_test_mode_ = layout_test_mode;

  web_prefs_ = new WebPreferences;

  ResetWebPreferences();

  // Register the Ahem font used by layout tests.
  DWORD num_fonts = 1;
  void* font_ptr;
  size_t font_size;
  if (base::GetDataResourceFromModule(::GetModuleHandle(NULL), IDR_AHEM_FONT,
                                      &font_ptr, &font_size)) {
    HANDLE rc = AddFontMemResourceEx(font_ptr, font_size, 0, &num_fonts);
    DCHECK(rc != 0);
  }

  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  if (parsed_command_line.HasSwitch(test_shell::kCrashDumps)) {
    std::wstring dir(
        parsed_command_line.GetSwitchValue(test_shell::kCrashDumps));
    new google_breakpad::ExceptionHandler(dir, 0, &MinidumpCallback, 0, true);
  }
}

void TestShell::DestroyWindow(gfx::NativeWindow windowHandle) {
  // Do we want to tear down some of the machinery behind the scenes too?
  RemoveWindowFromList(windowHandle);
  ::DestroyWindow(windowHandle);
}

void TestShell::PlatformShutdown() {
  OleUninitialize();
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

void TestShell::DumpAllBackForwardLists(std::wstring* result) {
  result->clear();
  for (WindowList::iterator iter = TestShell::windowList()->begin();
     iter != TestShell::windowList()->end(); iter++) {
    HWND hwnd = *iter;
    TestShell* shell =
        static_cast<TestShell*>(win_util::GetWindowUserData(hwnd));
    shell->DumpBackForwardList(result);
  }
}

bool TestShell::RunFileTest(const TestParams& params) {
  SetCurrentTestName(params.test_url.c_str());

  // Load the test file into the first available window.
  if (TestShell::windowList()->empty()) {
    LOG(ERROR) << "No windows open.";
    return false;
  }

  HWND hwnd = *(TestShell::windowList()->begin());
  TestShell* shell =
      static_cast<TestShell*>(win_util::GetWindowUserData(hwnd));

  // Clear focus between tests.
  shell->m_focusedWidgetHost = NULL;

  // Make sure the previous load is stopped.
  shell->webView()->StopLoading();
  shell->navigation_controller()->Reset();

  // StopLoading may update state maintained in the test controller (for
  // example, whether the WorkQueue is frozen) as such, we need to reset it
  // after we invoke StopLoading.
  shell->ResetTestController();

  // ResetTestController may have closed the window we were holding on to.
  // Grab the first window again.
  hwnd = *(TestShell::windowList()->begin());
  shell = static_cast<TestShell*>(win_util::GetWindowUserData(hwnd));
  DCHECK(shell);

  // Clean up state between test runs.
  webkit_glue::ResetBeforeTestRun(shell->webView());
  ResetWebPreferences();
  shell->webView()->SetPreferences(*web_prefs_);

  SetWindowPos(shell->m_mainWnd, NULL,
               kTestWindowXLocation, kTestWindowYLocation, 0, 0,
               SWP_NOSIZE | SWP_NOZORDER);
  shell->ResizeSubViews();

  if (strstr(params.test_url.c_str(), "loading/") ||
      strstr(params.test_url.c_str(), "loading\\"))
    shell->layout_test_controller()->SetShouldDumpFrameLoadCallbacks(true);

  shell->test_is_preparing_ = true;
  shell->set_test_params(&params);
  std::wstring wstr = UTF8ToWide(params.test_url.c_str());
  shell->LoadURL(wstr.c_str());

  shell->test_is_preparing_ = false;
  shell->WaitTestFinished();
  shell->set_test_params(NULL);


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
    replace_url.push_back(FilePath::kSeparators[0]);
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
    SizeToSVG();
  } else {
    SizeToDefault();
  }

  return true;
}

void TestShell::TestFinished() {
  if (!test_is_pending_)
    return;  // reached when running under test_shell_tests

  test_is_pending_ = false;
  HWND hwnd = *(TestShell::windowList()->begin());
  TestShell* shell =
      static_cast<TestShell*>(win_util::GetWindowUserData(hwnd));
  TestShell::Dump(shell);

  UINT_PTR timer_id = reinterpret_cast<UINT_PTR>(this);
  KillTimer(mainWnd(), timer_id);

  MessageLoop::current()->Quit();
}

// Thread main to run for the thread which just tests for timeout.
unsigned int __stdcall WatchDogThread(void *arg) {
  // If we're debugging a layout test, don't timeout.
  if (::IsDebuggerPresent())
    return 0;

  TestShell* shell = static_cast<TestShell*>(arg);
  DWORD timeout = static_cast<DWORD>(shell->GetLayoutTestTimeout() * 2.5);
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

void TestShell::InteractiveSetFocus(WebWidgetHost* host, bool enable) {
  if (!enable && ::GetFocus() == host->view_handle())
    ::SetFocus(NULL);
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
    SizeToSVG();
  } else {
    // only resize back to the default when running tests
    if (layout_test_mode())
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

      RemoveWindowFromList(hwnd);

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
        wchar_t strPtr[MAX_URL_LENGTH + 1];  // Leave room for adding a NULL;
        *((LPWORD)strPtr) = MAX_URL_LENGTH;
        LRESULT strLen = SendMessage(hwnd, EM_GETLINE, 0, (LPARAM)strPtr);
        if (strLen > 0) {
          strPtr[strLen] = 0;  // EM_GETLINE doesn't NULL terminate.
          shell->LoadURL(strPtr);
        }

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

// static
void TestShell::ShowStartupDebuggingDialog() {
  MessageBox(NULL, L"attach to me?", L"test_shell", MB_OK);
}

// static
StringPiece TestShell::NetResourceProvider(int key) {
  return GetRawDataResource(::GetModuleHandle(NULL), key);
}


/////////////////////////////////////////////////////////////////////////////
// WebKit glue functions

namespace webkit_glue {

string16 GetLocalizedString(int message_id) {
  const ATLSTRINGRESOURCEIMAGE* image =
      AtlGetStringResourceImage(_AtlBaseModule.GetModuleInstance(),
                                message_id);
  if (!image) {
    NOTREACHED();
    return L"No string for this identifier!";
  }
  return string16(image->achString, image->nLength);
}

// TODO(tc): Convert this to using resources from test_shell.rc.
StringPiece GetDataResource(int resource_id) {
  switch (resource_id) {
  case IDR_BROKENIMAGE: {
    // Use webkit's broken image icon (16x16)
    static std::string broken_image_data;
    if (broken_image_data.empty()) {
      FilePath path = GetResourcesFilePath();
      path = path.AppendASCII("missingImage.gif");
      bool success = file_util::ReadFileToString(path.ToWStringHack(),
                                                 &broken_image_data);
      if (!success) {
        LOG(FATAL) << "Failed reading: " << path.value();
      }
    }
    return broken_image_data;
  }
  case IDR_FEED_PREVIEW:
    // It is necessary to return a feed preview template that contains
    // a {{URL}} substring where the feed URL should go; see the code
    // that computes feed previews in feed_preview.cc:MakeFeedPreview.
    // This fixes issue #932714.
    return "Feed preview for {{URL}}";
  case IDR_TEXTAREA_RESIZER: {
    // Use webkit's text area resizer image.
    static std::string resize_corner_data;
    if (resize_corner_data.empty()) {
      FilePath path = GetResourcesFilePath();
      path = path.AppendASCII("textAreaResizeCorner.png");
      bool success = file_util::ReadFileToString(path.ToWStringHack(),
                                                 &resize_corner_data);
      if (!success) {
        LOG(FATAL) << "Failed reading: " << path.value();
      }
    }
    return resize_corner_data;
  }

  case IDR_SEARCH_CANCEL:
  case IDR_SEARCH_CANCEL_PRESSED:
  case IDR_SEARCH_MAGNIFIER:
  case IDR_SEARCH_MAGNIFIER_RESULTS:
  case IDR_MEDIA_PAUSE_BUTTON:
  case IDR_MEDIA_PLAY_BUTTON:
  case IDR_MEDIA_SOUND_FULL_BUTTON:
  case IDR_MEDIA_SOUND_NONE_BUTTON:
    return NetResourceProvider(resource_id);

  default:
    break;
  }

  return StringPiece();
}

HCURSOR LoadCursor(int cursor_id) {
  return NULL;
}

bool GetPlugins(bool refresh, std::vector<WebPluginInfo>* plugins) {
  return NPAPI::PluginList::Singleton()->GetPlugins(refresh, plugins);
}

bool EnsureFontLoaded(HFONT font) {
  return true;
}

bool DownloadUrl(const std::string& url, HWND caller_window) {
  return false;
}

}  // namespace webkit_glue
