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

#include "base/command_line.h"
#include "base/debug_on_start.h"
#include "base/file_util.h"
#include "base/gfx/bitmap_platform_device_win.h"
#include "base/gfx/png_encoder.h"
#include "base/gfx/size.h"
#include "base/icu_util.h"
#include "base/md5.h"
#include "base/memory_debug.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/stats_table.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "googleurl/src/url_util.h"
#include "net/base/mime_util.h"
#include "net/url_request/url_request_file_job.h"
#include "net/url_request/url_request_filter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/webdatasource.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webkit_resources.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/weburlrequest.h"
#include "webkit/glue/webview.h"
#include "webkit/glue/webwidget.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_navigation_controller.h"

#include "webkit_strings.h"

#include "SkBitmap.h"

using std::min;
using std::max;

#define MAX_LOADSTRING 100

#define BUTTON_WIDTH 72
#define URLBAR_HEIGHT  24

// Global Variables:
static TCHAR g_windowTitle[MAX_LOADSTRING];     // The title bar text
static TCHAR g_windowClass[MAX_LOADSTRING];     // The main window class name

// Forward declarations of functions included in this code module:
static INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

// Default timeout for page load when running non-interactive file
// tests, in ms.
const int kDefaultFileTestTimeoutMillisecs = 10 * 1000;

// Content area size for newly created windows.
const int kTestWindowWidth = 800;
const int kTestWindowHeight = 600;

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
WindowList* TestShell::window_list_;
HINSTANCE TestShell::instance_handle_;
WebPreferences* TestShell::web_prefs_ = NULL;
bool TestShell::interactive_ = true;
int TestShell::file_test_timeout_ms_ = kDefaultFileTestTimeoutMillisecs;

// URLRequestTestShellFileJob is used to serve the inspector
class URLRequestTestShellFileJob : public URLRequestFileJob {
 public:
  virtual ~URLRequestTestShellFileJob() { }

  static URLRequestJob* InspectorFactory(URLRequest* request, 
                                         const std::string& scheme) {
    std::wstring path;
    PathService::Get(base::DIR_EXE, &path);
    file_util::AppendToPath(&path, L"Resources");
    file_util::AppendToPath(&path, L"Inspector");
    file_util::AppendToPath(&path, UTF8ToWide(request->url().path()));
    return new URLRequestTestShellFileJob(request, path);
  }

 private:
  URLRequestTestShellFileJob(URLRequest* request, const std::wstring& path)
      : URLRequestFileJob(request) { 
    this->file_path_ = path;  // set URLRequestFileJob::file_path_
  }

  DISALLOW_EVIL_CONSTRUCTORS(URLRequestTestShellFileJob);
};

TestShell::TestShell() 
    : m_mainWnd(NULL),
      m_editWnd(NULL),
      m_webViewHost(NULL),
      m_popupHost(NULL),
      m_focusedWidgetHost(NULL),
      default_edit_wnd_proc_(0),
      delegate_(new TestWebViewDelegate(this)),
      test_is_preparing_(false),
      test_is_pending_(false),
      is_modal_(false),
      dump_stats_table_on_exit_(false) {
    layout_test_controller_.reset(new LayoutTestController(this));
    event_sending_controller_.reset(new EventSendingController(this));
    text_input_controller_.reset(new TextInputController(this));
    navigation_controller_.reset(new TestNavigationController(this));

    URLRequestFilter* filter = URLRequestFilter::GetInstance();
    filter->AddHostnameHandler("test-shell-resource", "inspector", 
                               &URLRequestTestShellFileJob::InspectorFactory);
    url_util::AddStandardScheme("test-shell-resource");
}

TestShell::~TestShell() {

    // Call GC twice to clean up garbage.
    CallJSGC();
    CallJSGC();

    // When the window is destroyed, tell the Edit field to forget about us, 
    // otherwise we will crash. 
    win_util::SetWindowProc(m_editWnd, default_edit_wnd_proc_);
    win_util::SetWindowUserData(m_editWnd, NULL);

    StatsTable *table = StatsTable::current();
    if (dump_stats_table_on_exit_) {
      // Dump the stats table.
      printf("<stats>\n");
      if (table != NULL) {
          int counter_max = table->GetMaxCounters();
          for (int index=0; index < counter_max; index++) {
              std::wstring name(table->GetRowName(index));
              if (name.length() > 0) {
                  int value = table->GetRowValue(index);
                  printf("%s:\t%d\n", WideToUTF8(name).c_str(), value);
              }
          }
      }
      printf("</stats>\n");
    }
}

// All fatal log messages (e.g. DCHECK failures) imply unit test failures
static void UnitTestAssertHandler(const std::string& str) {
    FAIL() << str;
}

// static
void TestShell::InitLogging(bool suppress_error_dialogs) {
    if (!IsDebuggerPresent() && suppress_error_dialogs) {
        UINT new_flags = SEM_FAILCRITICALERRORS |
                         SEM_NOGPFAULTERRORBOX |
                         SEM_NOOPENFILEERRORBOX;
        // Preserve existing error mode, as discussed at http://t/dmea
        UINT existing_flags = SetErrorMode(new_flags);
        SetErrorMode(existing_flags | new_flags);

        logging::SetLogAssertHandler(UnitTestAssertHandler);
    }

    // We might have multiple test_shell processes going at once
    std::wstring log_filename;
    PathService::Get(base::DIR_EXE, &log_filename);
    file_util::AppendToPath(&log_filename, L"test_shell.log");
    logging::InitLogging(log_filename.c_str(),
                         logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG,
                         logging::LOCK_LOG_FILE,
                         logging::DELETE_OLD_LOG_FILE);

    // we want process and thread IDs because we may have multiple processes
    logging::SetLogItems(true, true, false, true);
}

// static
void TestShell::CleanupLogging() {
    logging::CloseLogFile();
}

// static
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

// static
void TestShell::ResetWebPreferences() {
    DCHECK(web_prefs_);

    // Match the settings used by Mac DumpRenderTree.
    if (web_prefs_) {
        *web_prefs_ = WebPreferences();
        web_prefs_->standard_font_family = L"Times";
        web_prefs_->fixed_font_family = L"Courier";
        web_prefs_->serif_font_family = L"Times";
        web_prefs_->sans_serif_font_family = L"Helvetica";
        web_prefs_->cursive_font_family = L"Apple Chancery";
        web_prefs_->fantasy_font_family = L"Papyrus";
        web_prefs_->default_encoding = L"ISO-8859-1";
        web_prefs_->default_font_size = 16;
        web_prefs_->default_fixed_font_size = 13;
        web_prefs_->minimum_font_size = 1;
        web_prefs_->minimum_logical_font_size = 9;
        web_prefs_->javascript_can_open_windows_automatically = true;
        web_prefs_->dom_paste_enabled = true;
        web_prefs_->developer_extras_enabled = interactive_;
        web_prefs_->shrinks_standalone_images_to_fit = false;
        web_prefs_->uses_universal_detector = false;
        web_prefs_->text_areas_are_resizable = false;
        web_prefs_->user_agent = webkit_glue::GetDefaultUserAgent();
        web_prefs_->dashboard_compatibility_mode = false;
        web_prefs_->java_enabled = true;
    }
}
	
// static
void TestShell::ShutdownTestShell() {
    delete window_list_;
    SimpleResourceLoaderBridge::Shutdown();
    delete TestShell::web_prefs_;
    OleUninitialize();
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
unsigned int __stdcall WatchDogThread(void *arg)
{
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

void TestShell::Show(WebView* webview, WindowOpenDisposition disposition) {
  delegate_->Show(webview, disposition);
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

void TestShell::BindJSObjectsToWindow(WebFrame* frame) {
    // Only bind the test classes if we're running tests.
    if (!interactive_) {
        layout_test_controller_->BindToJavascript(frame, 
                                                  L"layoutTestController");
        event_sending_controller_->BindToJavascript(frame,
                                                    L"eventSender");
        text_input_controller_->BindToJavascript(frame,
                                                 L"textInputController");
    }
}


void TestShell::CallJSGC() {
    WebFrame* frame = webView()->GetMainFrame();
    frame->CallJSGC();
}


/*static*/
bool TestShell::CreateNewWindow(const std::wstring& startingURL,
                                TestShell** result)
{
    TestShell* shell = new TestShell();
    bool rv = shell->Initialize(startingURL);
    if (rv) {
        if (result)
            *result = shell;
        TestShell::windowList()->push_back(shell->m_mainWnd);
    }
    return rv;
}

WebView* TestShell::CreateWebView(WebView* webview) {
    // If we're running layout tests, only open a new window if the test has
    // called layoutTestController.setCanOpenWindows()
    if (!interactive_ && !layout_test_controller_->CanOpenWindows())
        return NULL;

    TestShell* new_win;
    if (!CreateNewWindow(std::wstring(), &new_win))
        return NULL;

    return new_win->webView();
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

void TestShell::SizeToDefault() {
   SizeTo(kTestWindowWidth, kTestWindowHeight);
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

/* static */ std::string TestShell::DumpImage(
        WebFrame* web_frame,
        const std::wstring& file_name) {
    gfx::BitmapPlatformDeviceWin device(web_frame->CaptureImage(true));
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

/* static */ void TestShell::DumpBackForwardList(std::wstring* result) {
    result->clear();
    for (WindowList::iterator iter = TestShell::windowList()->begin();
         iter != TestShell::windowList()->end(); iter++) {
        HWND hwnd = *iter;
        TestShell* shell =
            static_cast<TestShell*>(win_util::GetWindowUserData(hwnd));
        webkit_glue::DumpBackForwardList(shell->webView(), NULL, result);
    }
}

/* static */ bool TestShell::RunFileTest(const char *filename,
                                         const TestParams& params) {
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
                    webkit_glue::DumpFrameScrollPosition(webFrame, recursive)).
                    c_str());
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


/* static */
ATOM TestShell::RegisterWindowClass()
{
    LoadString(instance_handle_, IDS_APP_TITLE, g_windowTitle, MAX_LOADSTRING);
    LoadString(instance_handle_, IDC_TESTSHELL, g_windowClass, MAX_LOADSTRING);

    WNDCLASSEX wcex = {
      /* cbSize = */ sizeof(WNDCLASSEX),
      /* style = */ CS_HREDRAW | CS_VREDRAW,
      /* lpfnWndProc = */ TestShell::WndProc,
      /* cbClsExtra = */ 0,
      /* cbWndExtra = */ 0,
      /* hInstance = */ instance_handle_,
      /* hIcon = */ LoadIcon(instance_handle_, MAKEINTRESOURCE(IDI_TESTSHELL)),
      /* hCursor = */ LoadCursor(NULL, IDC_ARROW),
      /* hbrBackground = */ 0,
      /* lpszMenuName = */ MAKEINTRESOURCE(IDC_TESTSHELL),
      /* lpszClassName = */ g_windowClass,
      /* hIconSm = */ LoadIcon(instance_handle_, MAKEINTRESOURCE(IDI_SMALL)),
    };
    return RegisterClassEx(&wcex);
}

LRESULT CALLBACK TestShell::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
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
                MessageLoop::current()->PostTask(
                    FROM_HERE, new MessageLoop::QuitTask());
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
                                               WPARAM wParam, LPARAM lParam)
{
    TestShell* shell =
        static_cast<TestShell*>(win_util::GetWindowUserData(hwnd));

    switch (message) {
        case WM_CHAR:
            if (wParam == 13) { // Enter Key
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
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
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

void TestShell::LoadURL(const wchar_t* url)
{
    LoadURLForFrame(url, NULL);
}

void TestShell::LoadURLForFrame(const wchar_t* url,
                                const wchar_t* frame_name) {
    if (!url)
        return;

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

bool TestShell::Navigate(const TestNavigationEntry& entry, bool reload) {
    const TestNavigationEntry& test_entry =
        *static_cast<const TestNavigationEntry*>(&entry);

    WebRequestCachePolicy cache_policy;
    if (reload) {
      cache_policy = WebRequestReloadIgnoringCacheData;
    } else if (entry.GetPageID() != -1) {
      cache_policy = WebRequestReturnCacheDataElseLoad;
    } else {
      cache_policy = WebRequestUseProtocolCachePolicy;
    }

    scoped_ptr<WebRequest> request(WebRequest::Create(entry.GetURL()));
    request->SetCachePolicy(cache_policy);
    // If we are reloading, then WebKit will use the state of the current page.
    // Otherwise, we give it the state to navigate to.
    if (!reload)
      request->SetHistoryState(entry.GetContentState());
      
    request->SetExtraData(
        new TestShellExtraRequestData(entry.GetPageID()));

    // Get the right target frame for the entry.
    WebFrame* frame = webView()->GetMainFrame();
    if (!test_entry.GetTargetFrame().empty())
        frame = webView()->GetFrameWithName(test_entry.GetTargetFrame());
    // TODO(mpcomplete): should we clear the target frame, or should
    // back/forward navigations maintain the target frame?

    frame->LoadRequest(request.get());
    // Restore focus to the main frame prior to loading new request.
    // This makes sure that we don't have a focused iframe. Otherwise, that
    // iframe would keep focus when the SetFocus called immediately after
    // LoadRequest, thus making some tests fail (see http://b/issue?id=845337
    // for more details).
    webView()->SetFocusedFrame(frame);
    SetFocus(webViewHost(), true);

    return true;
}

void TestShell::GoBackOrForward(int offset) {
    navigation_controller_->GoToOffset(offset);
}

bool TestShell::PromptForSaveFile(const wchar_t* prompt_title,
                                  std::wstring* result)
{
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

static void WriteTextToFile(const std::wstring& data,
                            const std::wstring& file_path)
{
    FILE* fp;
    errno_t err = _wfopen_s(&fp, file_path.c_str(), L"wt");
    if (err)
        return;
    std::string data_utf8 = WideToUTF8(data);
    fwrite(data_utf8.c_str(), 1, data_utf8.size(), fp);
    fclose(fp);
}

std::wstring TestShell::GetDocumentText()
{
  return webkit_glue::DumpDocumentText(webView()->GetMainFrame());
}

void TestShell::DumpDocumentText()
{
    std::wstring file_path;
    if (!PromptForSaveFile(L"Dump document text", &file_path))
        return;

    WriteTextToFile(webkit_glue::DumpDocumentText(webView()->GetMainFrame()),
                    file_path);
}

void TestShell::DumpRenderTree()
{
    std::wstring file_path;
    if (!PromptForSaveFile(L"Dump render tree", &file_path))
        return;

    WriteTextToFile(webkit_glue::DumpRenderer(webView()->GetMainFrame()),
                    file_path);
}

void TestShell::Reload() {
    navigation_controller_->Reload();
}

/* static */
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

//-----------------------------------------------------------------------------

namespace webkit_glue {

bool HistoryContains(const char16* url, int url_len, 
                     const char* document_host, int document_host_len,
                     bool is_dns_prefetch_enabled) {
  return false;
}

void DnsPrefetchUrl(const char16* url, int url_length) {}

void PrecacheUrl(const char16* url, int url_length) {}

void AppendToLog(const char* file, int line, const char* msg) {
  logging::LogMessage(file, line).stream() << msg;
}

bool GetMimeTypeFromExtension(std::wstring &ext, std::string *mime_type) {
  return net::GetMimeTypeFromExtension(ext, mime_type);
}

bool GetMimeTypeFromFile(const std::wstring &file_path,
                         std::string *mime_type) {
  return net::GetMimeTypeFromFile(file_path, mime_type);
}

bool GetPreferredExtensionForMimeType(const std::string& mime_type,
                                      std::wstring* ext) {
  return net::GetPreferredExtensionForMimeType(mime_type, ext);
}

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

std::string GetDataResource(int resource_id) {
  if (resource_id == IDR_BROKENIMAGE) {
    // Use webkit's broken image icon (16x16)
    static std::string broken_image_data;
    if (broken_image_data.empty()) {
      std::wstring path;
      PathService::Get(base::DIR_SOURCE_ROOT, &path);
      file_util::AppendToPath(&path, L"webkit");
      file_util::AppendToPath(&path, L"tools");
      file_util::AppendToPath(&path, L"test_shell");
      file_util::AppendToPath(&path, L"resources");
      file_util::AppendToPath(&path, L"missingImage.gif");
      bool success = file_util::ReadFileToString(path, &broken_image_data);
      if (!success) {
        LOG(FATAL) << "Failed reading: " << path;
      }
    }
    return broken_image_data;
  } else if (resource_id == IDR_FEED_PREVIEW) {
    // It is necessary to return a feed preview template that contains
    // a {{URL}} substring where the feed URL should go; see the code 
    // that computes feed previews in feed_preview.cc:MakeFeedPreview. 
    // This fixes issue #932714.    
    return std::string("Feed preview for {{URL}}");
  } else {
    return std::string();
  }
}

HCURSOR LoadCursor(int cursor_id) {
  return NULL;
}

bool GetApplicationDirectory(std::wstring *path) {
  return PathService::Get(base::DIR_EXE, path);
}

GURL GetInspectorURL() {
  return GURL("test-shell-resource://inspector/inspector.html");
}

std::string GetUIResourceProtocol() {
  return "test-shell-resource";
}

bool GetExeDirectory(std::wstring *path) {
  return PathService::Get(base::DIR_EXE, path);
}

bool SpellCheckWord(const wchar_t* word, int word_len,
                    int* misspelling_start, int* misspelling_len) {
  // Report all words being correctly spelled.
  *misspelling_start = 0;
  *misspelling_len = 0;
  return true;
}

bool GetPlugins(bool refresh, std::vector<WebPluginInfo>* plugins) {
  return NPAPI::PluginList::Singleton()->GetPlugins(refresh, plugins);
}

bool webkit_glue::IsPluginRunningInRendererProcess() {
  return true;
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

bool GetPluginFinderURL(std::string* plugin_finder_url) {
  return false;
}

bool IsDefaultPluginEnabled() {
  return false;
}

std::wstring GetWebKitLocale() {
  return L"en-US";
}

}  // namespace webkit_glue

