// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_shell.h"

#include <errno.h>
#include <fcntl.h>
#include <gtk/gtk.h>
#include <unistd.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "net/base/mime_util.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/resource_loader_bridge.h"
#include "webkit/glue/webdatasource.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/webview.h"
#include "webkit/tools/test_shell/test_navigation_controller.h"
#include "webkit/tools/test_shell/test_webview_delegate.h"

// static
void TestShell::InitializeTestShell(bool interactive) {
  window_list_ = new WindowList;
  web_prefs_ = new WebPreferences;
  interactive_ = interactive;
}

// static
bool TestShell::CreateNewWindow(const std::wstring& startingURL,
                                TestShell** result) {
  TestShell *shell = new TestShell();
  if (!shell->Initialize(startingURL))
    return false;
  if (result)
    *result = shell;
  TestShell::windowList()->push_back(shell->m_mainWnd);
  return true;
}

void TestShell::PlatformCleanUp() {
}

bool TestShell::Initialize(const std::wstring& startingURL) {
  m_mainWnd = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(m_mainWnd), "Test Shell");
  gtk_window_set_default_size(GTK_WINDOW(m_mainWnd), 640, 480);

  GtkWidget* vbox = gtk_vbox_new(FALSE, 0);

  GtkWidget* toolbar = gtk_toolbar_new();
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar),
                     gtk_tool_button_new_from_stock(GTK_STOCK_GO_BACK),
                     -1 /* append */);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar),
                     gtk_tool_button_new_from_stock(GTK_STOCK_GO_FORWARD),
                     -1 /* append */);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar),
                     gtk_tool_button_new_from_stock(GTK_STOCK_REFRESH),
                     -1 /* append */);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar),
                     gtk_tool_button_new_from_stock(GTK_STOCK_STOP),
                     -1 /* append */);

  m_editWnd = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(m_editWnd), WideToUTF8(startingURL).c_str());

  GtkToolItem* tool_item = gtk_tool_item_new();
  gtk_container_add(GTK_CONTAINER(tool_item), m_editWnd);
  gtk_tool_item_set_expand(tool_item, TRUE);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar),
                     tool_item,
                     -1 /* append */);

  gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
  m_webViewHost.reset(WebViewHost::Create(vbox, delegate_, *TestShell::web_prefs_));

  if (!startingURL.empty())
    LoadURL(startingURL.c_str());

  gtk_container_add(GTK_CONTAINER(m_mainWnd), vbox);
  gtk_widget_show_all(m_mainWnd);

  return true;
}

void TestShell::TestFinished() {
  NOTIMPLEMENTED();
}

void TestShell::SizeTo(int width, int height) {
  NOTIMPLEMENTED();
}

void TestShell::WaitTestFinished() {
  DCHECK(!test_is_pending_) << "cannot be used recursively";

  test_is_pending_ = true;

  // TODO(agl): Here windows forks a watchdog thread, but I'm punting on that
  // for the moment. On POSIX systems we probably want to install a signal
  // handler and use alarm(2).

  // TestFinished() will post a quit message to break this loop when the page
  // finishes loading.
  while (test_is_pending_)
    MessageLoop::current()->Run();
}

void TestShell::SetFocus(WebWidgetHost* host, bool enable) {
  // TODO(agl): port the body of this function
  NOTIMPLEMENTED();
}

void TestShell::DestroyWindow(gfx::WindowHandle windowHandle) {
  NOTIMPLEMENTED();
}

WebWidget* TestShell::CreatePopupWidget(WebView* webview) {
  NOTIMPLEMENTED();
  return NULL;
}

void TestShell::ResizeSubViews() {
  // The GTK functions to do this are deprecated because it's not really
  // something that X windows supports. It's not clear exactly what should be
  // done here.
  NOTIMPLEMENTED();
}

/* static */ std::string TestShell::DumpImage(
        WebFrame* web_frame,
        const std::wstring& file_name) {
  // Windows uses some platform specific bitmap functions here.
  // TODO(agl): port
  NOTIMPLEMENTED();
  return "00000000000000000000000000000000";
}

/* static */ void TestShell::DumpBackForwardList(std::wstring* result) {
  result->clear();
  for (WindowList::iterator iter = TestShell::windowList()->begin();
       iter != TestShell::windowList()->end(); iter++) {
      GtkWidget* window = *iter;
      TestShell* shell =
          static_cast<TestShell*>(g_object_get_data(G_OBJECT(window), "test-shell"));
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

  GtkWidget* window = *(TestShell::windowList()->begin());
  TestShell* shell =
      static_cast<TestShell*>(g_object_get_data(G_OBJECT(window), "test-shell"));
  shell->ResetTestController();

  // ResetTestController may have closed the window we were holding on to. 
  // Grab the first window again.
  window = *(TestShell::windowList()->begin());
  shell = static_cast<TestShell*>(g_object_get_data(G_OBJECT(window), "test-shell"));
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

  // TODO(agl): Maybe make the window hidden in the future. Window does this
  // by positioning it off the screen but the GTK function to do this is
  // deprecated and appears to have been removed.

  shell->ResizeSubViews();

  if (strstr(filename, "loading/") || strstr(filename, "loading\\"))
    shell->layout_test_controller()->SetShouldDumpFrameLoadCallbacks(true);

  shell->test_is_preparing_ = true;

  const std::wstring wstr = UTF8ToWide(filename);
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

void TestShell::LoadURLForFrame(const wchar_t* url,
                                const wchar_t* frame_name) {
    if (!url)
        return;

    std::wstring frame_string;
    if (frame_name)
        frame_string = frame_name;

    LOG(INFO) << "Loading " << WideToUTF8(url) << " in frame '"
              << WideToUTF8(frame_string) << "'";

    navigation_controller_->LoadEntry(new TestNavigationEntry(
        -1, GURL(WideToUTF8(url)), std::wstring(), frame_string));
}

static void WriteTextToFile(const std::wstring& data,
                            const FilePath& filepath)
{
  // This function does the same thing as the Windows version except that it
  // takes a FilePath. We should be using WriteFile in base/file_util.h, but
  // the patch to add the FilePath version of that file hasn't landed yet, so
  // this is another TODO(agl) for the merging.
  const int fd = open(filepath.value().c_str(), O_TRUNC | O_WRONLY | O_CREAT, 0600);
  if (fd < 0)
    return;
  const std::string data_utf8 = WideToUTF8(data);
  ssize_t n;
  do {
    n = write(fd, data_utf8.data(), data.size());
  } while (n == -1 && errno == EINTR);
  close(fd);
}


// TODO(agl):
// This version of PromptForSaveFile uses FilePath, which is what the real
// version should be using. However, I don't want to step on tony's toes (as he
// is also editing this file), so this is a hack until we merge the files again.
// (There is also a PromptForSaveFile member in TestShell which returns a wstring)
static bool PromptForSaveFile(const char* prompt_title,
                              FilePath* result)
{
  char filenamebuffer[512];
  printf("Enter filename for \"%s\"\n", prompt_title);
  fgets(filenamebuffer, sizeof(filenamebuffer), stdin);
  *result = FilePath(filenamebuffer);
  return true;
}

void TestShell::DumpDocumentText()
{
  FilePath file_path;
  if (!::PromptForSaveFile("Dump document text", &file_path))
      return;

  WriteTextToFile(webkit_glue::DumpDocumentText(webView()->GetMainFrame()),
                  file_path);
}

void TestShell::DumpRenderTree()
{
  FilePath file_path;
  if (!::PromptForSaveFile("Dump render tree", &file_path))
      return;

  WriteTextToFile(webkit_glue::DumpRenderer(webView()->GetMainFrame()),
                  file_path);
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

//-----------------------------------------------------------------------------

namespace webkit_glue {

std::wstring GetLocalizedString(int message_id) {
  NOTREACHED();
  return L"No string for this identifier!";
}

bool GetPlugins(bool refresh, std::vector<WebPluginInfo>* plugins) {
  NOTIMPLEMENTED();
  return false;
}

ScreenInfo GetScreenInfo(gfx::ViewHandle window) {
  NOTIMPLEMENTED();
  return ScreenInfo();
}

}  // namespace webkit_glue
