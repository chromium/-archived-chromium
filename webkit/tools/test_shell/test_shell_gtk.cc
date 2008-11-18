// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_shell.h"

#include <errno.h>
#include <fcntl.h>
#include <fontconfig/fontconfig.h>
#include <gtk/gtk.h>
#include <unistd.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "net/base/mime_util.h"
#include "net/base/net_util.h"
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

  // We wish to make the layout tests reproducable with respect to fonts. Skia
  // uses fontconfig to resolve font family names from WebKit into actual font
  // files found on the current system. This means that fonts vary based on the
  // system and also on the fontconfig configuration.
  //
  // To avoid this we initialise fontconfig here and install a configuration
  // which only knows about a few, select, fonts.

  // This is the list of fonts that fontconfig will know about. It will try its
  // best to match based only on the fonts here in. The paths are where these
  // fonts are found on our Ubuntu boxes.
  static const char *const fonts[] = {
    "/usr/share/fonts/truetype/msttcorefonts/Arial.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Arial_Bold.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Arial_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Arial_Bold_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Courier_New.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Courier_New_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Courier_New_Bold.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Courier_New_Bold_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Times_New_Roman.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Times_New_Roman_Bold.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Times_New_Roman_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Times_New_Roman_Bold_Italic.ttf",
    NULL
  };

  FcInit();
  FcConfig* fontcfg = FcConfigCreate();
  for (unsigned i = 0; fonts[i]; ++i) {
    if (!FcConfigAppFontAddFile(fontcfg, (FcChar8 *) fonts[i]))
      LOG(FATAL) << "Failed to load font " << fonts[i];
  }

  if (!FcConfigSetCurrent(fontcfg))
    LOG(FATAL) << "Failed to set the default font configuration";
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

// GTK callbacks ------------------------------------------------------
namespace {

// Callback for when the main window is destroyed.
void MainWindowDestroyed(GtkWindow* window, TestShell* shell) {
  TestShell::RemoveWindowFromList(GTK_WIDGET(window));

  if (TestShell::windowList()->empty() || shell->is_modal()) {
    MessageLoop::current()->PostTask(FROM_HERE,
                                     new MessageLoop::QuitTask());
  }

  delete shell;
}

// Callback for when you click the back button.
void BackButtonClicked(GtkButton* button, TestShell* shell) {
  shell->GoBackOrForward(-1);
}

// Callback for when you click the forward button.
void ForwardButtonClicked(GtkButton* button, TestShell* shell) {
  shell->GoBackOrForward(1);
}

// Callback for when you click the stop button.
void StopButtonClicked(GtkButton* button, TestShell* shell) {
  shell->webView()->StopLoading();
}

// Callback for when you click the reload button.
void ReloadButtonClicked(GtkButton* button, TestShell* shell) {
  shell->Reload();
}

// Callback for when you press enter in the URL box.
void URLEntryActivate(GtkEntry* entry, TestShell* shell) {
  const gchar* url = gtk_entry_get_text(entry);
  shell->LoadURL(UTF8ToWide(url).c_str());
}

};

bool TestShell::Initialize(const std::wstring& startingURL) {
  m_mainWnd = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(m_mainWnd), "Test Shell");
  gtk_window_set_default_size(GTK_WINDOW(m_mainWnd), 640, 480);
  g_signal_connect(G_OBJECT(m_mainWnd), "destroy",
                   G_CALLBACK(MainWindowDestroyed), this);

  g_object_set_data(G_OBJECT(m_mainWnd), "test-shell", this);

  GtkWidget* vbox = gtk_vbox_new(FALSE, 0);

  GtkWidget* toolbar = gtk_toolbar_new();

  GtkToolItem* back = gtk_tool_button_new_from_stock(GTK_STOCK_GO_BACK);
  g_signal_connect(G_OBJECT(back), "clicked",
                   G_CALLBACK(BackButtonClicked), this);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), back, -1 /* append */);

  GtkToolItem* forward = gtk_tool_button_new_from_stock(GTK_STOCK_GO_FORWARD);
  g_signal_connect(G_OBJECT(forward), "clicked",
                   G_CALLBACK(ForwardButtonClicked), this);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), forward, -1 /* append */);

  GtkToolItem* reload = gtk_tool_button_new_from_stock(GTK_STOCK_REFRESH);
  g_signal_connect(G_OBJECT(reload), "clicked",
                   G_CALLBACK(ReloadButtonClicked), this);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), reload, -1 /* append */);

  GtkToolItem* stop = gtk_tool_button_new_from_stock(GTK_STOCK_STOP);
  g_signal_connect(G_OBJECT(stop), "clicked",
                   G_CALLBACK(StopButtonClicked), this);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), stop, -1 /* append */);

  m_editWnd = gtk_entry_new();
  g_signal_connect(G_OBJECT(m_editWnd), "activate",
                   G_CALLBACK(URLEntryActivate), this);
  gtk_entry_set_text(GTK_ENTRY(m_editWnd), WideToUTF8(startingURL).c_str());

  GtkToolItem* tool_item = gtk_tool_item_new();
  gtk_container_add(GTK_CONTAINER(tool_item), m_editWnd);
  gtk_tool_item_set_expand(tool_item, TRUE);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tool_item, -1 /* append */);

  gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
  m_webViewHost.reset(WebViewHost::Create(vbox, delegate_, *TestShell::web_prefs_));

  if (!startingURL.empty())
    LoadURL(startingURL.c_str());

  gtk_container_add(GTK_CONTAINER(m_mainWnd), vbox);
  gtk_widget_show_all(m_mainWnd);
  toolbar_height_ = toolbar->allocation.height +
      gtk_box_get_spacing(GTK_BOX(vbox));

  return true;
}

void TestShell::TestFinished() {
  if(!test_is_pending_)
    return;

  test_is_pending_ = false;
  MessageLoop::current()->Quit();
}

void TestShell::SizeTo(int width, int height) {
  gtk_window_resize(GTK_WINDOW(m_mainWnd), width, height + toolbar_height_);
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

void TestShell::InteractiveSetFocus(WebWidgetHost* host, bool enable) {
  GtkWidget* widget = GTK_WIDGET(host->window_handle());

  if (enable) {
    gtk_widget_grab_focus(widget);
  } else if (gtk_widget_is_focus(widget)) {
    GtkWidget *toplevel = gtk_widget_get_toplevel(widget);
    if (GTK_WIDGET_TOPLEVEL(toplevel))
      gtk_window_set_focus(GTK_WINDOW(toplevel), NULL);
  }
}

void TestShell::DestroyWindow(gfx::WindowHandle windowHandle) {
  RemoveWindowFromList(windowHandle);
  gtk_widget_destroy(windowHandle);
}

WebWidget* TestShell::CreatePopupWidget(WebView* webview) {
  GtkWidget* popupwindow = gtk_window_new(GTK_WINDOW_POPUP);
  GtkWidget* vbox = gtk_vbox_new(FALSE, 0);
  WebWidgetHost* host = WebWidgetHost::Create(vbox, delegate_);
  gtk_container_add(GTK_CONTAINER(popupwindow), vbox);
  m_popupHost = host;

  return host->webwidget();
}

void TestShell::ClosePopup() {
  DCHECK(m_popupHost);
  GtkWidget* drawing_area = m_popupHost->window_handle();
  GtkWidget* window =
      gtk_widget_get_parent(gtk_widget_get_parent(drawing_area));
  gtk_widget_destroy(window);
  m_popupHost->WindowDestroyed();
  m_popupHost = NULL;
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

  GURL gurl;
  // PathExists will reject any string with no leading '/'
  // as well as empty strings.
  if (file_util::PathExists(url))
    gurl = net::FilePathToFileURL(url);
  else
    gurl = GURL(WideToUTF8(url));

  navigation_controller_->LoadEntry(new TestNavigationEntry(
    -1, gurl, std::wstring(), frame_string));
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
  NOTIMPLEMENTED();
  return L"No string for this identifier!";
}

bool GetPlugins(bool refresh, std::vector<WebPluginInfo>* plugins) {
  NOTIMPLEMENTED();
  return false;
}

ScreenInfo GetScreenInfo(gfx::ViewHandle window) {
  return GetScreenInfoHelper(window);
}

}  // namespace webkit_glue
