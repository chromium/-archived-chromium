// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include <sys/stat.h>

#include "webkit/tools/test_shell/test_shell.h"

#include "base/basictypes.h"
#include "base/debug_on_start.h"
#include "base/debug_util.h"
#include "base/file_util.h"
#include "base/gfx/bitmap_platform_device.h"
#include "base/gfx/png_encoder.h"
#include "base/gfx/size.h"
#include "base/icu_util.h"
#include "base/md5.h"
#include "base/memory_debug.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/stats_table.h"
#include "base/string_util.h"
#include "net/base/mime_util.h"
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
#include "webkit/tools/test_shell/mac/test_shell_webview.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_navigation_controller.h"

#import "skia/include/SkBitmap.h"

#define MAX_LOADSTRING 100

#define BUTTON_WIDTH 72
#define URLBAR_HEIGHT  24

// Global Variables:

// Default timeout for page load when running non-interactive file
// tests, in ms.
const int kDefaultFileTestTimeoutMillisecs = 10 * 1000;

// Content area size for newly created windows.
const int kTestWindowWidth = 800;
const int kTestWindowHeight = 600;

// The W3C SVG layout tests use a different size than the other layout tests
const int kSVGTestWindowWidth = 480;
const int kSVGTestWindowHeight = 360;

// Hide the window offscreen when it is non-interactive.  Mac OS X limits
// window positions to +/- 16000
const int kTestWindowXLocation = -14000;
const int kTestWindowYLocation = -14000;

static const wchar_t* kStatsFile = L"testshell";
static int kStatsFileThreads = 20;
static int kStatsFileCounters = 100;

// Define static member variables
WindowList* TestShell::window_list_;
WebPreferences* TestShell::web_prefs_;
bool TestShell::interactive_ = true;
int TestShell::file_test_timeout_ms_ = kDefaultFileTestTimeoutMillisecs;
base::LazyInstance <std::map<gfx::WindowHandle, TestShell *> >
    TestShell::window_map_(base::LINKER_INITIALIZED);


TestShell::TestShell() 
    : m_mainWnd(NULL),
      m_editWnd(NULL),
      m_webViewHost(NULL),
      m_popupHost(NULL),
      m_focusedWidgetHost(NULL),
      layout_test_controller_(new LayoutTestController(this)),
      event_sending_controller_(new EventSendingController(this)),
      text_input_controller_(new TextInputController(this)),
      navigation_controller_(new TestNavigationController(this)),
      delegate_(new TestWebViewDelegate(this)),
      test_is_preparing_(false),
      test_is_pending_(false),
      dump_stats_table_on_exit_(false) {
  // load and initialize the stats table (one per process, so that multiple
  // instances don't interfere with each other)
  wchar_t statsfile[64];
  swprintf(statsfile, 64, L"%ls-%d", kStatsFile, getpid());
  
  StatsTable* table = new StatsTable(statsfile, kStatsFileThreads,
                                     kStatsFileCounters);
  StatsTable::set_current(table);
}

TestShell::~TestShell() {
  window_map_.Get().erase(m_mainWnd);

  if (dump_stats_table_on_exit_) {
    // Dump the stats table.
    printf("<stats>\n");
    StatsTable* table = StatsTable::current();
    if (table != NULL) {
      int counter_max = table->GetMaxCounters();
      for (int index = 0; index < counter_max; index++) {
        std::string name(WideToUTF8(table->GetRowName(index)));
        if (name.length() > 0) {
          int value = table->GetRowValue(index);
          printf("%s:\t%d\n", name.c_str(), value);
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
void TestShell::InitLogging(bool suppress_error_dialogs,
                            bool running_layout_tests) {
  if (suppress_error_dialogs) {
    logging::SetLogAssertHandler(UnitTestAssertHandler);
  }

  // Only log to a file if we're running layout tests. This prevents debugging
  // output from disrupting whether or not we pass.
  logging::LoggingDestination destination = 
      logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG;
  if (running_layout_tests)
    destination = logging::LOG_ONLY_TO_FILE;
  
  // We might have multiple test_shell processes going at once
  char log_filename_template[] = "/tmp/test_shell_XXXXXX";
  char* log_filename = mktemp(log_filename_template);
  logging::InitLogging(log_filename,
                       destination,
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
  window_list_ = new WindowList;
  interactive_ = interactive;
  
  web_prefs_ = new WebPreferences;
  
  ResetWebPreferences();
}

// static
void TestShell::ResetWebPreferences() {
  DCHECK(web_prefs_);
  
  // Match the settings used by Mac DumpRenderTree.
  if (web_prefs_) {
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
    web_prefs_->dashboard_compatibility_mode = false;
    web_prefs_->java_enabled = true;
  }
}
	
// static
void TestShell::ShutdownTestShell() {
  delete window_list_;
  delete TestShell::web_prefs_;
}

NSButton* MakeTestButton(NSRect* rect, NSString* title, NSView* parent) {
  NSButton* button = [[NSButton alloc] initWithFrame:*rect];
  [button setTitle:title];
  [button setBezelStyle:NSSmallSquareBezelStyle];
  [button setAutoresizingMask:(NSViewMaxXMargin | NSViewMinYMargin)];
  [parent addSubview:button];
  rect->origin.x += BUTTON_WIDTH;
  return button;
}

bool TestShell::Initialize(const std::wstring& startingURL) {
  // Perform application initialization:
  // send message to app controller?  need to work this out
  
  // TODO(awalker): this is a straight recreation of windows test_shell.cc's
  // window creation code--we should really pull this from the nib and grab
  // references to the already-created subviews that way.
  NSRect screen_rect = [[NSScreen mainScreen] visibleFrame];
  NSRect window_rect = { {0, screen_rect.size.height - kTestWindowHeight},
                         {kTestWindowWidth, kTestWindowHeight} };
  m_mainWnd = [[NSWindow alloc]
                  initWithContentRect:window_rect
                            styleMask:(NSTitledWindowMask |
                                       NSClosableWindowMask |
                                       NSMiniaturizableWindowMask |
                                       NSResizableWindowMask |
                                       NSTexturedBackgroundWindowMask)
                              backing:NSBackingStoreBuffered
                                defer:NO];
  [m_mainWnd setTitle:@"TestShell"];
  
  // create webview
  m_webViewHost.reset(
      WebViewHost::Create(m_mainWnd, delegate_.get(), *TestShell::web_prefs_));
  webView()->SetUseEditorDelegate(true);
  delegate_->RegisterDragDrop();
  TestShellWebView* web_view = 
      static_cast<TestShellWebView*>(m_webViewHost->window_handle());
  [web_view setShell:this];
  
  // create buttons
  NSRect button_rect = [[m_mainWnd contentView] bounds];
  button_rect.origin.y = window_rect.size.height - 22;
  button_rect.size.height = 22;
  button_rect.origin.x += 16;
  button_rect.size.width = BUTTON_WIDTH;
  
  NSView* content = [m_mainWnd contentView];
  
  NSButton* button = MakeTestButton(&button_rect, @"Back", content);
  [button setTarget:web_view];
  [button setAction:@selector(goBack:)];
  
  button = MakeTestButton(&button_rect, @"Forward", content);
  [button setTarget:web_view];
  [button setAction:@selector(goForward:)];
  
  // reload button
  button = MakeTestButton(&button_rect, @"Reload", content);
  [button setTarget:web_view];
  [button setAction:@selector(reload:)];
  
  // stop button
  button = MakeTestButton(&button_rect, @"Stop", content);
  [button setTarget:web_view];
  [button setAction:@selector(stopLoading:)];
  
  // text field for URL
  button_rect.origin.x += 16;
  button_rect.size.width = [[m_mainWnd contentView] bounds].size.width -
  button_rect.origin.x - 32;
  m_editWnd = [[NSTextField alloc] initWithFrame:button_rect];
  [[m_mainWnd contentView] addSubview:m_editWnd];
  [m_editWnd setAutoresizingMask:(NSViewWidthSizable | NSViewMinYMargin)];
  [m_editWnd setTarget:web_view];
  [m_editWnd setAction:@selector(takeURLStringValueFrom:)];

  // show the window
  [m_mainWnd makeKeyAndOrderFront: nil];
  
  // Load our initial content.
  if (!startingURL.empty())
    LoadURL(startingURL.c_str());

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
  
  test_is_pending_ = false;
  MessageLoop::current()->Quit();
}

// A class to be the target/selector of the "watchdog" thread that ensures
// pages timeout if they take too long and tells the test harness via stdout.
@interface WatchDogTarget : NSObject {
 @private
  NSTimeInterval timeout_;
}
// |timeout| is in seconds
- (id)initWithTimeout:(NSTimeInterval)timeout;
// serves as the "run" method of a NSThread.
- (void)run:(id)sender;
@end

@implementation WatchDogTarget

- (id)initWithTimeout:(NSTimeInterval)timeout {
  if ((self = [super init])) {
    timeout_ = timeout;
  }
  return self;
}

- (void)run:(id)ignore {
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
  
  // check for debugger, just bail if so. We don't want the timeouts hitting
  // when we're trying to track down an issue.
  if (DebugUtil::BeingDebugged())
    return;
    
  NSThread* currentThread = [NSThread currentThread];
  
  // Wait to be cancelled. If we are that means the test finished. If it hasn't,
  // then we need to tell the layout script we timed out and start again.
  NSDate* limitDate = [NSDate dateWithTimeIntervalSinceNow:timeout_];
  while ([(NSDate*)[NSDate date] compare:limitDate] == NSOrderedAscending &&
         ![currentThread isCancelled]) {
    // sleep for a small increment then check again
    NSDate* incrementDate = [NSDate dateWithTimeIntervalSinceNow:1.0];
    [NSThread sleepUntilDate:incrementDate];
  }
  if (![currentThread isCancelled]) {
    // Print a warning to be caught by the layout-test script.
    // Note: the layout test driver may or may not recognize
    // this as a timeout.
    puts("#TEST_TIMED_OUT\n");
    puts("#EOF\n");
    fflush(stdout);
    abort();
  }

  [pool release];
}

@end

void TestShell::WaitTestFinished() {
  DCHECK(!test_is_pending_) << "cannot be used recursively";
  
  test_is_pending_ = true;
  
  // Create a watchdog thread which just sets a timer and
  // kills the process if it times out.  This catches really
  // bad hangs where the shell isn't coming back to the 
  // message loop.  If the watchdog is what catches a 
  // timeout, it can't do anything except terminate the test
  // shell, which is unfortunate.
  // Windows multiplies by 2.5, but that causes us to run for far, far too
  // long. We can adjust it down later if we need to.
  NSTimeInterval timeout_seconds = GetFileTestTimeout() / 1000;
  WatchDogTarget* watchdog = [[[WatchDogTarget alloc] 
                                initWithTimeout:timeout_seconds] autorelease];
  NSThread* thread = [[NSThread alloc] initWithTarget:watchdog
                                             selector:@selector(run:) 
                                               object:nil];
  [thread start];
  
  // TestFinished() will post a quit message to break this loop when the page
  // finishes loading.
  while (test_is_pending_)
    MessageLoop::current()->Run();

  // Tell the watchdog that we're finished. No point waiting to re-join, it'll
  // die on its own.
  [thread cancel];
  [thread release];
}

void TestShell::Show(WebView* webview, WindowOpenDisposition disposition) {
  delegate_->Show(webview, disposition);
}

void TestShell::SetFocus(WebWidgetHost* host, bool enable) {
  if (interactive_) {
    if (enable) {
      // ::SetFocus(host->window_handle());
    } else {
      // if (GetFocus() == host->window_handle())
      //    ::SetFocus(NULL);
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

// static*
bool TestShell::CreateNewWindow(const std::wstring& startingURL,
                                TestShell** result) {
  TestShell* shell = new TestShell();
  bool rv = shell->Initialize(startingURL);
  if (rv) {
    if (result)
      *result = shell;
    TestShell::windowList()->push_back(shell->m_mainWnd);
    window_map_.Get()[shell->m_mainWnd] = shell;
  }
  return rv;
}

// static
void TestShell::DestroyWindow(gfx::WindowHandle windowHandle) {
  // Do we want to tear down some of the machinery behind the scenes too?
  [windowHandle performClose:nil];
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
  // ShowWindow(popupWnd(), SW_SHOW);
  
  return m_popupHost->webwidget();
}

void TestShell::ClosePopup() {
  // PostMessage(popupWnd(), WM_CLOSE, 0, 0);
  m_popupHost = NULL;
}

void TestShell::SizeToDefault() {
  SizeTo(kTestWindowWidth, kTestWindowHeight);
}

void TestShell::SizeTo(int width, int height) {
  NSRect r = [m_mainWnd contentRectForFrameRect:[m_mainWnd frame]];
  r.size.width = width;
  r.size.height = height;
  [m_mainWnd setFrame:[m_mainWnd frameRectForContentRect:r] display:YES];
}

void TestShell::ResizeSubViews() {
  // handled by Cocoa for us
}

/* static */ std::string TestShell::DumpImage(
        WebFrame* web_frame,
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
  if (png.size() > 0) {
    FILE* file = fopen(WideToUTF8(file_name).c_str(), "w");
    if (file) {
      fwrite(&png[0], sizeof(unsigned char), png.size(), file);
      fclose(file);
    }
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
#if 0
    HWND hwnd = *iter;
    TestShell* shell =
    static_cast<TestShell*>(win_util::GetWindowUserData(hwnd));
    webkit_glue::DumpBackForwardList(shell->webView(), NULL, result);
#endif
  }
}

/* static */ bool TestShell::RunFileTest(const char* filename,
                                         const TestParams& params) {
  // Load the test file into the first available window.
  if (TestShell::windowList()->empty()) {
    LOG(ERROR) << "No windows open.";
    return false;
  }

  NSWindow* window = *(TestShell::windowList()->begin());
  TestShell* shell = window_map_.Get()[window];
  shell->ResetTestController();

  // ResetTestController may have closed the window we were holding on to. 
  // Grab the first window again.
  window = *(TestShell::windowList()->begin());
  shell = window_map_.Get()[window];
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

  // Hide the window. We can't actually use NSWindow's |-setFrameTopLeftPoint:|
  // because it leaves a chunk of the window visible instead of moving it
  // offscreen.
  [shell->m_mainWnd orderOut:nil];
  shell->ResizeSubViews();

  if (strstr(filename, "loading/"))
    shell->layout_test_controller()->SetShouldDumpFrameLoadCallbacks(true);

  shell->test_is_preparing_ = true;

  shell->LoadURL(UTF8ToWide(filename).c_str());

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
        std::string mime_type =
            WideToUTF8(webFrame->GetDataSource()->GetResponseMimeType());
        should_dump_as_text = (mime_type == "text/plain");
      }
      if (should_dump_as_text) {
        bool recursive = shell->layout_test_controller_->
            ShouldDumpChildFramesAsText();
        printf("%s", WideToUTF8(
            webkit_glue::DumpFramesAsText(webFrame, recursive)).
            c_str());
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

void TestShell::LoadURL(const wchar_t* url)
{
  LoadURLForFrame(url, NULL);
}

void TestShell::LoadURLForFrame(const wchar_t* url,
                                const wchar_t* frame_name) {
  if (!url)
    return;
  
  std::string url8 = WideToUTF8(url);

  bool bIsSVGTest = strstr(url8.c_str(), "W3C-SVG-1.1") != NULL;

  if (bIsSVGTest) {
    SizeTo(kSVGTestWindowWidth, kSVGTestWindowHeight);
  } else {
    // SizeToDefault();
  }

  std::string urlString(url8);
  struct stat stat_buf;
  if (!urlString.empty() && stat(url8.c_str(), &stat_buf) == 0) {
    urlString.insert(0, "file://");
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
  
  request->SetExtraData(new TestShellExtraRequestData(entry.GetPageID()));
  
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
  NSSavePanel* save_panel = [NSSavePanel savePanel];
  
  /* set up new attributes */
  [save_panel setRequiredFileType:@"txt"];
  [save_panel setMessage:
      [NSString stringWithUTF8String:WideToUTF8(prompt_title).c_str()]];
  
  /* display the NSSavePanel */
  if ([save_panel runModalForDirectory:NSHomeDirectory() file:@""] ==
      NSOKButton) {
    result->assign(UTF8ToWide([[save_panel filename] UTF8String]));
    return true;
  }
  return false;
}

static void WriteTextToFile(const std::string& data,
                            const std::string& file_path)
{
  FILE* fp = fopen(file_path.c_str(), "w");
  if (!fp)
    return;
  fwrite(data.c_str(), sizeof(std::string::value_type), data.size(), fp);
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
  
  WriteTextToFile(
      WideToUTF8(webkit_glue::DumpDocumentText(webView()->GetMainFrame())),
      WideToUTF8(file_path));
}

void TestShell::DumpRenderTree()
{
  std::wstring file_path;
  if (!PromptForSaveFile(L"Dump render tree", &file_path))
    return;
  
  WriteTextToFile(
      WideToUTF8(webkit_glue::DumpRenderer(webView()->GetMainFrame())),
      WideToUTF8(file_path));
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
    std::string replace_url8 = WideToUTF8(replace_url);
    new_url = std::string("file:///") +
        replace_url8.append(url.substr(kPrefixLen));
  }
  return new_url;
}

//-----------------------------------------------------------------------------

namespace webkit_glue {

void PrefetchDns(const std::string& hostname) {}

void PrecacheUrl(const char16* url, int url_length) {}

void AppendToLog(const char* file, int line, const char* msg) {
  logging::LogMessage(file, line).stream() << msg;
}

bool GetMimeTypeFromExtension(std::string &ext, std::string* mime_type) {
  return net::GetMimeTypeFromExtension(UTF8ToWide(ext), mime_type);
}

bool GetMimeTypeFromFile(const std::string &file_path,
                         std::string* mime_type) {
  return net::GetMimeTypeFromFile(UTF8ToWide(file_path), mime_type);
}

bool GetPreferredExtensionForMimeType(const std::string& mime_type,
                                      std::string* ext) {
  std::wstring wide_ext;
  bool result = net::GetPreferredExtensionForMimeType(mime_type, &wide_ext);
  if (result)
    *ext = WideToUTF8(wide_ext);
  return result;
}

std::wstring GetLocalizedString(int message_id) {
  NSString* idString = [NSString stringWithFormat:@"%d", message_id];
  NSString* localString = NSLocalizedString(idString, @"");
  
  return UTF8ToWide([localString UTF8String]);
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
      file_util::AppendToPath(&path, L"test_shel");
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

NSCursor* LoadCursor(int cursor_id) {
  // TODO(port): add some more options here
  return [NSCursor arrowCursor];
}

bool GetApplicationDirectory(std::string* path) {
  NSString* bundle_path = [[NSBundle mainBundle] bundlePath];
  if (!bundle_path)
    return false;
  bundle_path = [bundle_path stringByDeletingLastPathComponent];
  *path = [bundle_path UTF8String];
  return true;
}

GURL GetInspectorURL() {
  // TODO(port): is this right?
  NSLog(@"GetInspectorURL");
  return GURL("test-shell-resource://inspector/inspector.html");
}

std::string GetUIResourceProtocol() {
  return "test-shell-resource";
}

bool GetInspectorHTMLPath(std::string* path) {
  NSString* resource_path = [[NSBundle mainBundle] resourcePath];
  if (!resource_path)
    return false;
  *path = [resource_path UTF8String];
  *path += "/Inspector/inspector.htm";
  return true;
}

bool GetExeDirectory(std::string* path) {
  NSString* executable_path = [[NSBundle mainBundle] executablePath];
  if (!executable_path)
    return false;
  *path = [executable_path UTF8String];
  return true;
}

bool SpellCheckWord(const char* word, int word_len,
                    int* misspelling_start, int* misspelling_len) {
  // Report all words being correctly spelled.
  *misspelling_start = 0;
  *misspelling_len = 0;
  return true;
}

bool GetPlugins(bool refresh, std::vector<WebPluginInfo>* plugins) {
  return false; // NPAPI::PluginList::Singleton()->GetPlugins(refresh, plugins);
}

bool IsPluginRunningInRendererProcess() {
  return true;
}

bool DownloadUrl(const std::string& url, NSWindow* caller_window) {
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

void DidLoadPlugin(const std::string& filename) {
}

void DidUnloadPlugin(const std::string& filename) {
}

}  // namespace webkit_glue

// These are here ONLY to satisfy link errors until we reinstate the ObjC
// bindings into WebCore.

@interface DOMRange : NSObject
@end
@implementation DOMRange
@end

@interface DOMDocumentFragment : NSObject
@end
@implementation DOMDocumentFragment
@end

@interface DOMNode : NSObject
@end
@implementation DOMNode
@end

@interface DOMElement : NSObject
@end
@implementation DOMElement
@end

@interface DOMCSSStyleDeclaration : NSObject
@end
@implementation DOMCSSStyleDeclaration
@end
