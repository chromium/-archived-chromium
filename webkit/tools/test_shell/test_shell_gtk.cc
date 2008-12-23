// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_shell.h"

#include <errno.h>
#include <fcntl.h>
#include <fontconfig/fontconfig.h>
#include <gtk/gtk.h>
#include <signal.h>
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

// TODO(deanm): Needed for the localized string shim.
#include "webkit_strings.h"

namespace {

// Convert a FilePath into an FcChar* (used by fontconfig).
// The pointer only lives for the duration for the expression.
const FcChar8* FilePathAsFcChar(const FilePath& path) {
  return reinterpret_cast<const FcChar8*>(path.value().c_str());
}

}

// static
void TestShell::InitializeTestShell(bool layout_test_mode) {
  window_list_ = new WindowList;
  layout_test_mode_ = layout_test_mode;

  web_prefs_ = new WebPreferences;

  ResetWebPreferences();

  // We wish to make the layout tests reproducable with respect to fonts. Skia
  // uses fontconfig to resolve font family names from WebKit into actual font
  // files found on the current system. This means that fonts vary based on the
  // system and also on the fontconfig configuration.
  //
  // To avoid this we initialise fontconfig here and install a configuration
  // which only knows about a few, select, fonts.

  FilePath resources_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &resources_path);
  resources_path = resources_path.Append("webkit/tools/test_shell/resources/");

  // We have fontconfig parse a config file from our resources directory. This
  // sets a number of aliases ("sans"->"Arial" etc), but doesn't include any
  // font directories.

  FcInit();

  FcConfig* fontcfg = FcConfigCreate();
  FilePath fontconfig_path = resources_path.Append("linux-fontconfig-config");
  if (!FcConfigParseAndLoad(fontcfg, FilePathAsFcChar(fontconfig_path),
                            true)) {
    LOG(FATAL) << "Failed to parse fontconfig config file";
  }

  // This is the list of fonts that fontconfig will know about. It will try its
  // best to match based only on the fonts here in. The paths are where these
  // fonts are found on our Ubuntu boxes.
  static const char *const fonts[] = {
    "/usr/share/fonts/truetype/msttcorefonts/Arial.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Arial_Bold.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Arial_Bold_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Arial_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Comic_Sans_MS.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Comic_Sans_MS_Bold.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Courier_New.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Courier_New_Bold.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Courier_New_Bold_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Courier_New_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Impact.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Times_New_Roman.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Times_New_Roman_Bold.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Times_New_Roman_Bold_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Times_New_Roman_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Verdana.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Verdana_Bold.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Verdana_Bold_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Verdana_Italic.ttf",
    "/usr/share/fonts/truetype/ttf-lucida/LucidaSansRegular.ttf",
    NULL
  };
  for (size_t i = 0; fonts[i]; ++i) {
    if (access(fonts[i], R_OK)) {
      LOG(FATAL) << "You are missing " << fonts[i] << ". "
                 << "Try installing msttcorefonts. Also see "
                 << "http://code.google.com/p/chromium/wiki/"
                 << "LinuxBuildInstructions";
    }
    if (!FcConfigAppFontAddFile(fontcfg, (FcChar8 *) fonts[i]))
      LOG(FATAL) << "Failed to load font " << fonts[i];
  }

  // Also load the layout-test-specific "Ahem" font.
  FilePath ahem_path = resources_path.Append("AHEM____.TTF");
  if (!FcConfigAppFontAddFile(fontcfg, FilePathAsFcChar(ahem_path))) {
    LOG(FATAL) << "Failed to load font " << ahem_path.value().c_str();
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
  // The GTK widgets will be destroyed, which will free the associated
  // objects.  So we don't need the scoped_ptr to free the webViewHost.
  m_webViewHost.release();
}

// GTK callbacks ------------------------------------------------------
namespace {

// Callback for when the main window is destroyed.
gboolean MainWindowDestroyed(GtkWindow* window, TestShell* shell) {

  TestShell::RemoveWindowFromList(GTK_WIDGET(window));

  if (TestShell::windowList()->empty() || shell->is_modal()) {
    MessageLoop::current()->PostTask(FROM_HERE,
                                     new MessageLoop::QuitTask());
  }

  delete shell;

  return FALSE;  // Don't stop this message.
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
  // Turn off the labels on the toolbar buttons.
  gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);

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

  // Enables output of "EDDITING DELEGATE: " debugging lines in the layout test
  // output.
  webView()->SetUseEditorDelegate(true);

  gtk_container_add(GTK_CONTAINER(m_mainWnd), vbox);
  gtk_widget_show_all(m_mainWnd);
  toolbar_height_ = toolbar->allocation.height +
      gtk_box_get_spacing(GTK_BOX(vbox));

  // LoadURL will do a resize (which uses toolbar_height_), so make sure we
  // don't call LoadURL until we've completed all of our GTK setup.
  if (!startingURL.empty())
    LoadURL(startingURL.c_str());

  bool bIsSVGTest = startingURL.find(L"W3C-SVG-1.1") != std::wstring::npos;
  if (bIsSVGTest)
    SizeToSVG();
  else
    SizeToDefault();

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

static void AlarmHandler(int signatl) {
  // If the alarm alarmed, kill the process since we have a really bad hang.
  puts("#TEST_TIMED_OUT\n");
  puts("#EOF\n");
  fflush(stdout);
  exit(0);
}

void TestShell::WaitTestFinished() {
  DCHECK(!test_is_pending_) << "cannot be used recursively";

  test_is_pending_ = true;

  // Install an alarm signal handler that will kill us if we time out.
  signal(SIGALRM, AlarmHandler);
  alarm(GetLayoutTestTimeoutInSeconds());

  // TestFinished() will post a quit message to break this loop when the page
  // finishes loading.
  while (test_is_pending_)
    MessageLoop::current()->Run();

  // Remove the alarm.
  alarm(0);
  signal(SIGALRM, SIG_DFL);
}

void TestShell::InteractiveSetFocus(WebWidgetHost* host, bool enable) {
  GtkWidget* widget = GTK_WIDGET(host->view_handle());

  if (enable) {
    gtk_widget_grab_focus(widget);
  } else if (gtk_widget_is_focus(widget)) {
    GtkWidget *toplevel = gtk_widget_get_toplevel(widget);
    if (GTK_WIDGET_TOPLEVEL(toplevel))
      gtk_window_set_focus(GTK_WINDOW(toplevel), NULL);
  }
}

void TestShell::DestroyWindow(gfx::NativeWindow windowHandle) {
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
  GtkWidget* drawing_area = m_popupHost->view_handle();
  GtkWidget* window =
      gtk_widget_get_parent(gtk_widget_get_parent(drawing_area));
  gtk_widget_destroy(window);
  m_popupHost = NULL;
}

void TestShell::ResizeSubViews() {
  // This function is used on Windows to re-layout the window on a resize.
  // GTK manages layout for us so we do nothing.
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
        if (fwrite(data_utf8.c_str(), 1, data_utf8.size(), stdout) !=
            data_utf8.size()) {
          LOG(FATAL) << "Short write to stdout, disk full?";
        }
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

  bool bIsSVGTest = wcsstr(url, L"W3C-SVG-1.1") > 0;

  if (bIsSVGTest)
    SizeToSVG();
  else {
    // only resize back to the default when running tests
    if (layout_test_mode())
      SizeToDefault();
  }

  std::wstring frame_string;
  if (frame_name)
    frame_string = frame_name;

  std::wstring path(url);
  GURL gurl;
  // PathExists will reject any string with no leading '/'
  // as well as empty strings.
  if (file_util::AbsolutePath(&path))
    gurl = net::FilePathToFileURL(path);
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
  if (!fgets(filenamebuffer, sizeof(filenamebuffer), stdin))
    return false;  // EOF on stdin
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
  GtkWidget* dialog = gtk_message_dialog_new(
    NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "attach to me?");
  gtk_window_set_title(GTK_WINDOW(dialog), "test_shell");
  gtk_dialog_run(GTK_DIALOG(dialog));  // Runs a nested message loop.
  gtk_widget_destroy(dialog);
}

//-----------------------------------------------------------------------------

namespace webkit_glue {

// TODO(deanm): This is just a shim for now.  We need to extend GRIT to do
// proper resources on Linux, and figure out exactly how we'll do localization.
// For now this is just a copy of webkit_strings_en-US.rc in switch form.
std::wstring GetLocalizedString(int message_id) {
  const char* str = NULL;

  switch (message_id) {
    case IDS_SEARCHABLE_INDEX_INTRO: 
      str = "This is a searchable index. Enter search keywords: ";
      break;
    case IDS_FORM_SUBMIT_LABEL: 
      str = "Submit";
      break;
    case IDS_FORM_INPUT_ALT: 
      str = "Submit";
      break;
    case IDS_FORM_RESET_LABEL: 
      str = "Reset";
      break;
    case IDS_FORM_FILE_BUTTON_LABEL: 
      str = "Choose File";
      break;
    case IDS_FORM_FILE_NO_FILE_LABEL: 
      str = "No file chosen";
      break;
    case IDS_FORM_FILE_NO_FILE_DRAG_LABEL: 
      str = "Drag file here";
      break;
    case IDS_RECENT_SEARCHES_NONE: 
      str = "No recent searches";
      break;
    case IDS_RECENT_SEARCHES: 
      str = "Recent Searches";
      break;
    case IDS_RECENT_SEARCHES_CLEAR: 
      str = "Clear Recent Searches";
      break;
    case IDS_IMAGE_TITLE_FOR_FILENAME: 
      str = "%s%d\xc3\x97%d";
      break;
    case IDS_AX_ROLE_WEB_AREA: 
      str = "web area";
      break;
    case IDS_AX_ROLE_LINK: 
      str = "link";
      break;
    case IDS_AX_ROLE_LIST_MARKER: 
      str = "list marker";
      break;
    case IDS_AX_ROLE_IMAGE_MAP: 
      str = "image map";
      break;
    case IDS_AX_ROLE_HEADING: 
      str = "heading";
      break;
    case IDS_AX_BUTTON_ACTION_VERB: 
      str = "press";
      break;
    case IDS_AX_RADIO_BUTTON_ACTION_VERB: 
      str = "select";
      break;
    case IDS_AX_TEXT_FIELD_ACTION_VERB: 
      str = "activate";
      break;
    case IDS_AX_CHECKED_CHECK_BOX_ACTION_VERB: 
      str = "uncheck";
      break;
    case IDS_AX_UNCHECKED_CHECK_BOX_ACTION_VERB: 
      str = "check";
      break;
    case IDS_AX_LINK_ACTION_VERB: 
      str = "jump";
      break;
    case IDS_KEYGEN_HIGH_GRADE_KEY: 
      str = "2048 (High Grade)";
      break;
    case IDS_KEYGEN_MED_GRADE_KEY: 
      str = "1024 (Medium Grade)";
      break;
    case IDS_DEFAULT_PLUGIN_GET_PLUGIN_MSG: 
      str = "$1 plugin is not installed";
      break;
    case IDS_DEFAULT_PLUGIN_GET_PLUGIN_MSG_NO_PLUGIN_NAME: 
      str = "The required plugin is not installed";
      break;
    case IDS_DEFAULT_PLUGIN_GET_PLUGIN_MSG_2: 
      str = "Click here to download plugin";
      break;
    case IDS_DEFAULT_PLUGIN_REFRESH_PLUGIN_MSG: 
      str = "After installing the plugin, click here to refresh";
      break;
    case IDS_DEFAULT_PLUGIN_NO_PLUGIN_AVAILABLE_MSG: 
      str = "No plugin available to display this content";
      break;
    case IDS_DEFAULT_PLUGIN_DOWNLOADING_PLUGIN_MSG: 
      str = "Downloading plugin...";
      break;
    case IDS_DEFAULT_PLUGIN_GET_THE_PLUGIN_BTN_MSG: 
      str = "Get Plugin";
      break;
    case IDS_DEFAULT_PLUGIN_CANCEL_PLUGIN_DOWNLOAD_MSG: 
      str = "Cancel";
      break;
    case IDS_DEFAULT_PLUGIN_CONFIRMATION_DIALOG_TITLE: 
      str = "$1 plugin needed";
      break;
    case IDS_DEFAULT_PLUGIN_CONFIRMATION_DIALOG_TITLE_NO_PLUGIN_NAME: 
      str = "Additional plugin needed";
      break;
    case IDS_DEFAULT_PLUGIN_USER_OPTION_MSG: 
      str = "Please confirm that you would like to install the $1 plugin. You should only install plugins that you trust.";
      break;
    case IDS_DEFAULT_PLUGIN_USER_OPTION_MSG_NO_PLUGIN_NAME: 
      str = "Please confirm that you would like to install this plugin. You should only install plugins that you trust.";
      break;
    case IDS_DEFAULT_PLUGIN_USE_OPTION_CONFIRM: 
      str = "Install";
      break;
    case IDS_DEFAULT_PLUGIN_USE_OPTION_CANCEL: 
      str = "Cancel";
      break;
    case IDS_DEFAULT_PLUGIN_DOWNLOAD_FAILED_MSG: 
      str = "Failed to install plugin from $1";
      break;
    case IDS_DEFAULT_PLUGIN_INSTALLATION_FAILED_MSG: 
      str = "Plugin installation failed";
      break;
    default:
      NOTIMPLEMENTED();
      str = "No string for this identifier!";
  }

  return UTF8ToWide(str);
}

bool GetPlugins(bool refresh, std::vector<WebPluginInfo>* plugins) {
  // TODO(port): Implement plugins someday.  Don't let the error message
  // of NOTIMPLEMENTED into our layout test diffs.
  // NOTIMPLEMENTED();
  return false;
}

}  // namespace webkit_glue
