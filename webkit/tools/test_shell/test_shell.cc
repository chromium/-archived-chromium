// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_shell.h"

#include "base/command_line.h"
#include "base/debug_on_start.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/gfx/png_encoder.h"
#include "base/gfx/size.h"
#include "base/icu_util.h"
#if defined(OS_MACOSX)
#include "base/mac_util.h"
#endif
#include "base/md5.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/stats_table.h"
#include "base/string_util.h"
#include "build/build_config.h"
#include "googleurl/src/url_util.h"
#include "net/base/mime_util.h"
#include "net/url_request/url_request_file_job.h"
#include "net/url_request/url_request_filter.h"
#include "skia/ext/bitmap_platform_device.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/webdatasource.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/weburlrequest.h"
#include "webkit/glue/webview.h"
#include "webkit/glue/webwidget.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_navigation_controller.h"
#include "webkit/tools/test_shell/test_shell_switches.h"
#include "webkit_strings.h"

#include "SkBitmap.h"

namespace {

// Default timeout in ms for file page loads when in layout test mode.
const int kDefaultFileTestTimeoutMillisecs = 10 * 1000;

// Content area size for newly created windows.
const int kTestWindowWidth = 800;
const int kTestWindowHeight = 600;

// The W3C SVG layout tests use a different size than the other layout
// tests.
const int kSVGTestWindowWidth = 480;
const int kSVGTestWindowHeight = 360;

// URLRequestTestShellFileJob is used to serve the inspector
class URLRequestTestShellFileJob : public URLRequestFileJob {
 public:
  virtual ~URLRequestTestShellFileJob() { }

  static URLRequestJob* InspectorFactory(URLRequest* request, 
                                         const std::string& scheme) {
    std::wstring path;
    PathService::Get(base::DIR_EXE, &path);
    file_util::AppendToPath(&path, L"resources");
    file_util::AppendToPath(&path, L"inspector");
    file_util::AppendToPath(&path, UTF8ToWide(request->url().path()));
    return new URLRequestTestShellFileJob(request,
                                          FilePath::FromWStringHack(path));
  }

 private:
  URLRequestTestShellFileJob(URLRequest* request, const FilePath& path)
      : URLRequestFileJob(request, path) {
  }

  DISALLOW_COPY_AND_ASSIGN(URLRequestTestShellFileJob);
};

}  // namespace

// Initialize static member variable
WindowList* TestShell::window_list_;
WebPreferences* TestShell::web_prefs_ = NULL;
bool TestShell::layout_test_mode_ = false;
int TestShell::file_test_timeout_ms_ = kDefaultFileTestTimeoutMillisecs;

TestShell::TestShell() 
    : m_mainWnd(NULL),
      m_editWnd(NULL),
      m_webViewHost(NULL),
      m_popupHost(NULL),
      m_focusedWidgetHost(NULL),
#if defined(OS_WIN)
      default_edit_wnd_proc_(0),
#endif
      test_is_preparing_(false),
      test_is_pending_(false),
      is_modal_(false),
      dump_stats_table_on_exit_(false) {
    delegate_ = new TestWebViewDelegate(this);
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
  // Navigate to an empty page to fire all the destruction logic for the
  // current page.
  LoadURL(L"about:blank");

  // Call GC twice to clean up garbage.
  CallJSGC();
  CallJSGC();

  PlatformCleanUp();

  StatsTable *table = StatsTable::current();
  if (dump_stats_table_on_exit_) {
    // Dump the stats table.
    printf("<stats>\n");
    if (table != NULL) {
      int counter_max = table->GetMaxCounters();
      for (int index=0; index < counter_max; index++) {
        std::string name(table->GetRowName(index));
        if (name.length() > 0) {
          int value = table->GetRowValue(index);
          printf("%s:\t%d\n", name.c_str(), value);
        }
      }
    }
    printf("</stats>\n");
  }
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

void TestShell::ShutdownTestShell() {
    PlatformShutdown();
    SimpleResourceLoaderBridge::Shutdown();
    delete window_list_;
    delete TestShell::web_prefs_;
}

// All fatal log messages (e.g. DCHECK failures) imply unit test failures
static void UnitTestAssertHandler(const std::string& str) {
    FAIL() << str;
}

// static
std::string TestShell::DumpImage(WebFrame* web_frame,
    const std::wstring& file_name) {
  scoped_ptr<skia::BitmapPlatformDevice> device;
  if (!web_frame->CaptureImage(&device, true))
    return std::string();

  const SkBitmap& src_bmp = device->accessBitmap(false);

  // Encode image.
  std::vector<unsigned char> png;
  SkAutoLockPixels src_bmp_lock(src_bmp);
  PNGEncoder::ColorFormat color_format = PNGEncoder::FORMAT_BGRA;
#if defined(OS_WIN) || defined(OS_LINUX)
  bool discard_transparency = true;
#elif defined(OS_MACOSX)
  // the expected PNGs in webkit have an alpha channel. We shouldn't discard
  // the transparency for reference purposes, though the hashes will still
  // match.
  bool discard_transparency = false;
#endif
  PNGEncoder::Encode(
      reinterpret_cast<const unsigned char*>(src_bmp.getPixels()),
      color_format, src_bmp.width(), src_bmp.height(),
      static_cast<int>(src_bmp.rowBytes()), discard_transparency, &png);

  // Write to disk.
  file_util::WriteFile(file_name, reinterpret_cast<const char *>(&png[0]),
                       png.size());

  // Compute MD5 sum.
  MD5Context ctx;
  MD5Init(&ctx);
  MD5Update(&ctx, src_bmp.getPixels(), src_bmp.getSize());

  MD5Digest digest;
  MD5Final(&digest, &ctx);
  return MD5DigestToBase16(digest);
}

// static
void TestShell::InitLogging(bool suppress_error_dialogs,
                            bool layout_test_mode,
                            bool enable_gp_fault_error_box) {
    if (suppress_error_dialogs)
        logging::SetLogAssertHandler(UnitTestAssertHandler);

#if defined(OS_WIN)
    if (!IsDebuggerPresent()) {
        UINT new_flags = SEM_FAILCRITICALERRORS |
                         SEM_NOOPENFILEERRORBOX;
        if (!enable_gp_fault_error_box)
            new_flags |= SEM_NOGPFAULTERRORBOX;
        
        // Preserve existing error mode, as discussed at
        // http://blogs.msdn.com/oldnewthing/archive/2004/07/27/198410.aspx
        UINT existing_flags = SetErrorMode(new_flags);
        SetErrorMode(existing_flags | new_flags);
    }
#endif

    // Only log to a file if we're running layout tests. This prevents debugging
    // output from disrupting whether or not we pass.
    logging::LoggingDestination destination = 
        logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG;
    if (layout_test_mode)
      destination = logging::LOG_ONLY_TO_FILE;

    // We might have multiple test_shell processes going at once
    FilePath log_filename;
    PathService::Get(base::DIR_EXE, &log_filename);
    log_filename = log_filename.Append(FILE_PATH_LITERAL("test_shell.log"));
    logging::InitLogging(log_filename.value().c_str(),
                         destination,
                         logging::LOCK_LOG_FILE,
                         logging::DELETE_OLD_LOG_FILE);

    // we want process and thread IDs because we may have multiple processes
    logging::SetLogItems(true, true, false, true);

    // Turn on logging of notImplemented()s inside WebKit, but only if we're
    // not running layout tests (because otherwise they'd corrupt the test
    // output).
    if (!layout_test_mode)
      webkit_glue::EnableWebCoreNotImplementedLogging();
}

// static
void TestShell::CleanupLogging() {
    logging::CloseLogFile();
}

// static
void TestShell::SetAllowScriptsToCloseWindows() {
  if (web_prefs_)
    web_prefs_->allow_scripts_to_close_windows = true;
}

// static
void TestShell::ResetWebPreferences() {
    DCHECK(web_prefs_);

    // Match the settings used by Mac DumpRenderTree, with the exception of
    // fonts.
    if (web_prefs_) {
        *web_prefs_ = WebPreferences();

#if defined(OS_MACOSX)
        web_prefs_->serif_font_family = L"Times";
        web_prefs_->cursive_font_family = L"Apple Chancery";
        web_prefs_->fantasy_font_family = L"Papyrus";
#else
        // NOTE: case matters here, this must be 'times new roman', else
        // some layout tests fail.
        web_prefs_->serif_font_family = L"times new roman";

        // These two fonts are picked from the intersection of
        // Win XP font list and Vista font list :
        //   http://www.microsoft.com/typography/fonts/winxp.htm 
        //   http://blogs.msdn.com/michkap/archive/2006/04/04/567881.aspx
        // Some of them are installed only with CJK and complex script
        // support enabled on Windows XP and are out of consideration here. 
        // (although we enabled both on our buildbots.)
        // They (especially Impact for fantasy) are not typical cursive
        // and fantasy fonts, but it should not matter for layout tests
        // as long as they're available.
        web_prefs_->cursive_font_family = L"Comic Sans MS";
        web_prefs_->fantasy_font_family = L"Impact";
#endif
        web_prefs_->standard_font_family = web_prefs_->serif_font_family;
        web_prefs_->fixed_font_family = L"Courier";
        web_prefs_->sans_serif_font_family = L"Helvetica";

        web_prefs_->default_encoding = L"ISO-8859-1";
        web_prefs_->default_font_size = 16;
        web_prefs_->default_fixed_font_size = 13;
        web_prefs_->minimum_font_size = 1;
        web_prefs_->minimum_logical_font_size = 9;
        web_prefs_->javascript_can_open_windows_automatically = true;
        web_prefs_->dom_paste_enabled = true;
        web_prefs_->developer_extras_enabled = !layout_test_mode_;
        web_prefs_->shrinks_standalone_images_to_fit = false;
        web_prefs_->uses_universal_detector = false;
        web_prefs_->text_areas_are_resizable = false;
        web_prefs_->java_enabled = true;
        web_prefs_->allow_scripts_to_close_windows = false;
    }
}

// static
bool TestShell::RemoveWindowFromList(gfx::NativeWindow window) {
  WindowList::iterator entry =
      std::find(TestShell::windowList()->begin(),
                TestShell::windowList()->end(),
                window);
  if (entry != TestShell::windowList()->end()) {
    TestShell::windowList()->erase(entry);
    return true;
  }

  return false;
}

void TestShell::Show(WebView* webview, WindowOpenDisposition disposition) {
  delegate_->Show(webview, disposition);
}

void TestShell::BindJSObjectsToWindow(WebFrame* frame) {
    // Only bind the test classes if we're running tests.
    if (layout_test_mode_) {
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


WebView* TestShell::CreateWebView(WebView* webview) {
    // If we're running layout tests, only open a new window if the test has
    // called layoutTestController.setCanOpenWindows()
    if (layout_test_mode_ && !layout_test_controller_->CanOpenWindows())
        return NULL;

    TestShell* new_win;
    if (!CreateNewWindow(std::wstring(), &new_win))
        return NULL;

    return new_win->webView();
}

void TestShell::SizeToSVG() {
  SizeTo(kSVGTestWindowWidth, kSVGTestWindowHeight);
}

void TestShell::SizeToDefault() {
   SizeTo(kTestWindowWidth, kTestWindowHeight);
}

void TestShell::LoadURL(const wchar_t* url) {
    LoadURLForFrame(url, NULL);
}

bool TestShell::Navigate(const TestNavigationEntry& entry, bool reload) {
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
    if (!entry.GetTargetFrame().empty())
        frame = webView()->GetFrameWithName(entry.GetTargetFrame());
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

void TestShell::DumpDocumentText() {
  std::wstring file_path;
  if (!PromptForSaveFile(L"Dump document text", &file_path))
      return;

  const std::string data =
      WideToUTF8(webkit_glue::DumpDocumentText(webView()->GetMainFrame()));
  file_util::WriteFile(file_path, data.c_str(), data.length());
}

void TestShell::DumpRenderTree() {
  std::wstring file_path;
  if (!PromptForSaveFile(L"Dump render tree", &file_path))
    return;

  const std::string data =
      WideToUTF8(webkit_glue::DumpRenderer(webView()->GetMainFrame()));
  file_util::WriteFile(file_path, data.c_str(), data.length());
}

std::wstring TestShell::GetDocumentText() {
  return webkit_glue::DumpDocumentText(webView()->GetMainFrame());
}

void TestShell::Reload() {
    navigation_controller_->Reload();
}

void TestShell::SetFocus(WebWidgetHost* host, bool enable) {
  if (!layout_test_mode_) {
    InteractiveSetFocus(host, enable);
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

//-----------------------------------------------------------------------------

namespace webkit_glue {

bool IsMediaPlayerAvailable() {
  return CommandLine::ForCurrentProcess()->HasSwitch(test_shell::kEnableVideo);
}

void PrefetchDns(const std::string& hostname) {}

void PrecacheUrl(const char16* url, int url_length) {}

void AppendToLog(const char* file, int line, const char* msg) {
  logging::LogMessage(file, line).stream() << msg;
}

bool GetMimeTypeFromExtension(const std::wstring &ext, std::string *mime_type) {
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

GlueBitmap GetBitmapResource(int resource_id) {
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

ScreenInfo GetScreenInfo(gfx::NativeView window) {
  return GetScreenInfoHelper(window);
}

bool IsPluginRunningInRendererProcess() {
  return true;
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

uint64 VisitedLinkHash(const char* canonical_url, size_t length) {
  return 0;
}

bool IsLinkVisited(uint64 link_hash) {
  return false;
}

}  // namespace webkit_glue
