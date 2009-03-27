// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>
#import <objc/objc-runtime.h>
#include <sys/stat.h>

#include "webkit/tools/test_shell/test_shell.h"

#include "base/basictypes.h"
#include "base/data_pack.h"
#include "base/debug_on_start.h"
#include "base/debug_util.h"
#include "base/file_util.h"
#include "base/gfx/size.h"
#include "base/icu_util.h"
#include "base/logging.h"
#include "base/mac_util.h"
#include "base/memory_debug.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/stats_table.h"
#include "base/string16.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "grit/webkit_resources.h"
#include "net/base/mime_util.h"
#include "skia/ext/bitmap_platform_device.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/webdatasource.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/weburlrequest.h"
#include "webkit/glue/webview.h"
#include "webkit/glue/webwidget.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/tools/test_shell/mac/test_shell_webview.h"
#include "webkit/tools/test_shell/resource.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_navigation_controller.h"

#import "skia/include/SkBitmap.h"

#import "mac/DumpRenderTreePasteboard.h"

#define MAX_LOADSTRING 100

// Sizes for URL bar layout
#define BUTTON_HEIGHT 22
#define BUTTON_WIDTH 72
#define BUTTON_MARGIN 8
#define URLBAR_HEIGHT  32

// Global Variables:

// Content area size for newly created windows.
const int kTestWindowWidth = 800;
const int kTestWindowHeight = 600;

// The W3C SVG layout tests use a different size than the other layout tests
const int kSVGTestWindowWidth = 480;
const int kSVGTestWindowHeight = 360;

// Hide the window offscreen when in layout test mode.  Mac OS X limits
// window positions to +/- 16000.
const int kTestWindowXLocation = -14000;
const int kTestWindowYLocation = -14000;

// Data pack resource. This is a pointer to the mmapped resources file.
static base::DataPack* g_resource_data_pack = NULL;

// Define static member variables
base::LazyInstance <std::map<gfx::NativeWindow, TestShell *> >
    TestShell::window_map_(base::LINKER_INITIALIZED);

// Helper method for getting the path to the test shell resources directory.
FilePath GetResourcesFilePath() {
  FilePath path;
  // We need to know if we're bundled or not to know which path to use.
  if (mac_util::AmIBundled()) {
    PathService::Get(base::DIR_EXE, &path);
    path = path.Append(FilePath::kParentDirectory);
    return path.AppendASCII("Resources");
  } else {
    PathService::Get(base::DIR_SOURCE_ROOT, &path);
    path = path.AppendASCII("webkit");
    path = path.AppendASCII("tools");
    path = path.AppendASCII("test_shell");
    return path.AppendASCII("resources");
  }
}

// Receives notification that the window is closing so that it can start the
// tear-down process. Is responsible for deleting itself when done.
@interface WindowCloseDelegate : NSObject
@end

@implementation WindowCloseDelegate

// Called when the window is about to close. Perform the self-destruction
// sequence by getting rid of the shell and removing it and the window from
// the various global lists. Instead of doing it here, however, we fire off
// a delayed call to |-cleanup:| to allow everything to get off the stack
// before we go deleting objects. By returning YES, we allow the window to be
// removed from the screen.
- (BOOL)windowShouldClose:(id)window {
  // Try to make the window go away, but it may not when running layout
  // tests due to the quirkyness of autorelease pools and having no main loop.
  [window autorelease];

  // clean ourselves up and do the work after clearing the stack of anything
  // that might have the shell on it.
  [self performSelectorOnMainThread:@selector(cleanup:)
                         withObject:window
                      waitUntilDone:NO];

  return YES;
}

// does the work of removing the window from our various bookkeeping lists
// and gets rid of the shell.
- (void)cleanup:(id)window {
  TestShell::RemoveWindowFromList(window);
  TestShell::DestroyAssociatedShell(window);

  [self release];
}

@end

// Mac-specific stuff to do when the dtor is called. Nothing to do in our
// case.
void TestShell::PlatformCleanUp() {
}

// static
void TestShell::DestroyAssociatedShell(gfx::NativeWindow handle) {
  WindowMap::iterator it = window_map_.Get().find(handle);
  if (it != window_map_.Get().end()) {
    // Break the view's association with its shell before deleting the shell.
    TestShellWebView* web_view =
      static_cast<TestShellWebView*>(it->second->m_webViewHost->view_handle());
    if ([web_view isKindOfClass:[TestShellWebView class]]) {
      [web_view setShell:NULL];
    }

    delete it->second;
    window_map_.Get().erase(it);
  } else {
    LOG(ERROR) << "Failed to find shell for window during destroy";
  }
}

// static
void TestShell::PlatformShutdown() {
  // for each window in the window list, release it and destroy its shell
  for (WindowList::iterator it = TestShell::windowList()->begin();
       it != TestShell::windowList()->end();
       ++it) {
    DestroyAssociatedShell(*it);
    [*it release];
  }
  // assert if we have anything left over, that would be bad.
  DCHECK(window_map_.Get().size() == 0);

  // Dump the pasteboards we built up.
  [DumpRenderTreePasteboard releaseLocalPasteboards];
}

// static
void TestShell::InitializeTestShell(bool layout_test_mode) {
  // This should move to a per-process platform-specific initialization function
  // when one exists.

  window_list_ = new WindowList;
  layout_test_mode_ = layout_test_mode;

  web_prefs_ = new WebPreferences;

  // mmap the data pack which holds strings used by WebCore. This is only
  // a fatal error if we're bundled, which means we might be running layout
  // tests. This is a harmless failure for test_shell_tests.
  g_resource_data_pack = new base::DataPack;
  NSString *resource_path =
      [mac_util::MainAppBundle() pathForResource:@"test_shell"
                                          ofType:@"pak"];
  FilePath resources_pak_path([resource_path fileSystemRepresentation]);
  if (!g_resource_data_pack->Load(resources_pak_path)) {
    LOG(FATAL) << "failed to load test_shell.pak";
  }

  ResetWebPreferences();

  // Load the Ahem font, which is used by layout tests.
  const char* ahem_path_c;
  NSString* ahem_path = [[mac_util::MainAppBundle() resourcePath]
      stringByAppendingPathComponent:@"AHEM____.TTF"];
  ahem_path_c = [ahem_path fileSystemRepresentation];
  FSRef ahem_fsref;
  if (!mac_util::FSRefFromPath(ahem_path_c, &ahem_fsref)) {
    DLOG(FATAL) << "FSRefFromPath " << ahem_path_c;
  } else {
    // The last argument is an ATSFontContainerRef that can be passed to
    // ATSFontDeactivate to unload the font.  Since the font is only loaded
    // for this process, and it's always wanted, don't keep track of it.
    if (ATSFontActivateFromFileReference(&ahem_fsref,
                                         kATSFontContextLocal,
                                         kATSFontFormatUnspecified,
                                         NULL,
                                         kATSOptionFlagsDefault,
                                         NULL) != noErr) {
      DLOG(FATAL) << "ATSFontActivateFromFileReference " << ahem_path_c;
    }
  }
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
                                       NSResizableWindowMask )
                              backing:NSBackingStoreBuffered
                                defer:NO];
  [m_mainWnd setTitle:@"TestShell"];

  // Add to our map
  window_map_.Get()[m_mainWnd] = this;

  // Create a window delegate to watch for when it's asked to go away. It will
  // clean itself up so we don't need to hold a reference.
  [m_mainWnd setDelegate:[[WindowCloseDelegate alloc] init]];

  // Rely on the window delegate to clean us up rather than immediately
  // releasing when the window gets closed. We use the delegate to do
  // everything from the autorelease pool so the shell isn't on the stack
  // during cleanup (ie, a window close from javascript).
  [m_mainWnd setReleasedWhenClosed:NO];

  // Create a webview. Note that |web_view| takes ownership of this shell so we
  // will get cleaned up when it gets destroyed.
  m_webViewHost.reset(
      WebViewHost::Create([m_mainWnd contentView],
                          delegate_.get(),
                          *TestShell::web_prefs_));
  webView()->SetUseEditorDelegate(true);
  delegate_->RegisterDragDrop();
  TestShellWebView* web_view =
      static_cast<TestShellWebView*>(m_webViewHost->view_handle());
  [web_view setShell:this];

  // create buttons
  NSRect button_rect = [[m_mainWnd contentView] bounds];
  button_rect.origin.y = window_rect.size.height - URLBAR_HEIGHT +
      (URLBAR_HEIGHT - BUTTON_HEIGHT) / 2;
  button_rect.size.height = BUTTON_HEIGHT;
  button_rect.origin.x += BUTTON_MARGIN;
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
  button_rect.origin.x += BUTTON_MARGIN;
  button_rect.size.width = [[m_mainWnd contentView] bounds].size.width -
      button_rect.origin.x - BUTTON_MARGIN;
  m_editWnd = [[NSTextField alloc] initWithFrame:button_rect];
  [[m_mainWnd contentView] addSubview:m_editWnd];
  [m_editWnd setAutoresizingMask:(NSViewWidthSizable | NSViewMinYMargin)];
  [m_editWnd setTarget:web_view];
  [m_editWnd setAction:@selector(takeURLStringValueFrom:)];
  [[m_editWnd cell] setWraps:NO];
  [[m_editWnd cell] setScrollable:YES];

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
  NSWindow* window = *(TestShell::windowList()->begin());
  WindowMap::iterator it = window_map_.Get().find(window);
  if (it != window_map_.Get().end())
    TestShell::Dump(it->second);
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
  // long. We use the passed value and let the scripts flag override
  // the value as needed.
  NSTimeInterval timeout_seconds = GetLayoutTestTimeoutInSeconds();
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

void TestShell::InteractiveSetFocus(WebWidgetHost* host, bool enable) {
#if 0
  if (enable)
    ::SetFocus(host->view_handle());
  else if (::GetFocus() == host->view_handle())
    ::SetFocus(NULL);
#endif
}

// static
void TestShell::DestroyWindow(gfx::NativeWindow windowHandle) {
  // This code is like -cleanup: on our window delegate.  This call needs to be
  // able to force down a window for tests, so it closes down the window making
  // sure it cleans up the window delegate and the test shells list of windows
  // and map of windows to shells.

  TestShell::RemoveWindowFromList(windowHandle);
  TestShell::DestroyAssociatedShell(windowHandle);

  id windowDelegate = [windowHandle delegate];
  DCHECK(windowDelegate);
  [windowHandle setDelegate:nil];
  [windowDelegate release];

  [windowHandle close];
  [windowHandle autorelease];
}

WebWidget* TestShell::CreatePopupWidget(WebView* webview) {
  DCHECK(!m_popupHost);
  m_popupHost = WebWidgetHost::Create(webViewWnd(), delegate_.get());

  return m_popupHost->webwidget();
}

void TestShell::ClosePopup() {
  // PostMessage(popupWnd(), WM_CLOSE, 0, 0);
  m_popupHost = NULL;
}

void TestShell::SizeTo(int width, int height) {
  // WebViewHost::Create() sets the HTML content rect to start 32 pixels below
  // the top of the window to account for the "toolbar". We need to match that
  // here otherwise the HTML content area will be too short.
  NSRect r = [m_mainWnd contentRectForFrameRect:[m_mainWnd frame]];
  r.size.width = width;
  r.size.height = height + URLBAR_HEIGHT;
  [m_mainWnd setFrame:[m_mainWnd frameRectForContentRect:r] display:YES];
}

void TestShell::ResizeSubViews() {
  // handled by Cocoa for us
}

/* static */ void TestShell::DumpBackForwardList(std::wstring* result) {
  result->clear();
  for (WindowList::iterator iter = TestShell::windowList()->begin();
       iter != TestShell::windowList()->end(); iter++) {
    NSWindow* window = *iter;
    WindowMap::iterator it = window_map_.Get().find(window);
    if (it != window_map_.Get().end())
      webkit_glue::DumpBackForwardList(it->second->webView(), NULL, result);
    else
      LOG(ERROR) << "Failed to find shell for window during dump";
  }
}

/* static */ bool TestShell::RunFileTest(const TestParams& params) {
  // Load the test file into the first available window.
  if (TestShell::windowList()->empty()) {
    LOG(ERROR) << "No windows open.";
    return false;
  }

  NSWindow* window = *(TestShell::windowList()->begin());
  TestShell* shell = window_map_.Get()[window];
  DCHECK(shell);
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

  if (strstr(params.test_url.c_str(), "loading/"))
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

void TestShell::LoadURLForFrame(const wchar_t* url,
                                const wchar_t* frame_name) {
  if (!url)
    return;

  std::string url8 = WideToUTF8(url);

  bool bIsSVGTest = strstr(url8.c_str(), "W3C-SVG-1.1") > 0;

  if (bIsSVGTest) {
    SizeTo(kSVGTestWindowWidth, kSVGTestWindowHeight);
  } else {
    // only resize back to the default when running tests
    if (layout_test_mode())
      SizeToDefault();
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

// static
std::string TestShell::RewriteLocalUrl(const std::string& url) {
  // Convert file:///tmp/LayoutTests urls to the actual location on disk.
  const char kPrefix[] = "file:///tmp/LayoutTests/";
  const int kPrefixLen = arraysize(kPrefix) - 1;

  std::string new_url(url);
  if (url.compare(0, kPrefixLen, kPrefix, kPrefixLen) == 0) {
    FilePath replace_path;
    PathService::Get(base::DIR_EXE, &replace_path);
    replace_path = replace_path.DirName().DirName().Append(
        "webkit/data/layout_tests/LayoutTests/");
    new_url = std::string("file://") + replace_path.value() +
        url.substr(kPrefixLen);
  }

  return new_url;
}

// static
void TestShell::ShowStartupDebuggingDialog() {
  NSAlert* alert = [[[NSAlert alloc] init] autorelease];
  alert.messageText = @"Attach to me?";
  alert.informativeText = @"This would probably be a good time to attach your "
      "debugger.";
  [alert addButtonWithTitle:@"OK"];

  [alert runModal];
}

StringPiece TestShell::NetResourceProvider(int key) {
  // TODO(port): Return the requested resource.
  NOTIMPLEMENTED();
  return StringPiece();
}

//-----------------------------------------------------------------------------

namespace webkit_glue {

string16 GetLocalizedString(int message_id) {
  StringPiece res;
  if (!g_resource_data_pack->Get(message_id, &res)) {
    LOG(FATAL) << "failed to load webkit string with id " << message_id;
  }

  return string16(reinterpret_cast<const char16*>(res.data()),
                  res.length() / 2);
}

std::string GetDataResource(int resource_id) {
  switch (resource_id) {
  case IDR_BROKENIMAGE: {
    // Use webkit's broken image icon (16x16)
    static std::string broken_image_data;
    if (broken_image_data.empty()) {
      FilePath path = GetResourcesFilePath();
      // In order to match WebKit's colors for the missing image, we have to
      // use a PNG. The GIF doesn't have the color range needed to correctly
      // match the TIFF they use in Safari.
      path = path.AppendASCII("missingImage.png");
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
    return std::string("Feed preview for {{URL}}");
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
    return TestShell::NetResourceProvider(resource_id).as_string();

  default:
    break;
  }

  return std::string();
}

bool GetPlugins(bool refresh, std::vector<WebPluginInfo>* plugins) {
  return NPAPI::PluginList::Singleton()->GetPlugins(refresh, plugins);
}

bool DownloadUrl(const std::string& url, NSWindow* caller_window) {
  return false;
}

void DidLoadPlugin(const std::string& filename) {
}

void DidUnloadPlugin(const std::string& filename) {
}

}  // namespace webkit_glue
