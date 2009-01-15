// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_init.h"

#include <shellapi.h>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/event_recorder.h"
#include "base/file_util.h"
#include "base/histogram.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "chrome/app/locales/locale_settings.h"
#include "chrome/app/result_codes.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/dom_ui/new_tab_ui.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/first_run.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/session_startup_pref.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/url_fixer_upper.h"
#include "chrome/browser/web_app_launcher.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "net/base/cookie_monster.h"
#include "net/base/net_util.h"
#include "webkit/glue/webkit_glue.h"

#include "chromium_strings.h"
#include "generated_resources.h"

namespace {

// A delegate for the InfoBar shown when the previous session has crashed. The
// bar deletes itself automatically after it is closed.
class SessionCrashedInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  explicit SessionCrashedInfoBarDelegate(TabContents* contents)
      : ConfirmInfoBarDelegate(contents),
        profile_(contents->profile()) {
  }

  // Overridden from ConfirmInfoBarDelegate:
  virtual void InfoBarClosed() {
    delete this;
  }
  virtual std::wstring GetMessageText() const {
    return l10n_util::GetString(IDS_SESSION_CRASHED_VIEW_MESSAGE);
  }
  virtual SkBitmap* GetIcon() const {
    return ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_INFOBAR_RESTORE_SESSION);
  }
  virtual int GetButtons() const { return BUTTON_OK; }
  virtual std::wstring GetButtonLabel(InfoBarButton button) const {
    return l10n_util::GetString(IDS_SESSION_CRASHED_VIEW_RESTORE_BUTTON);
  }
  virtual bool Accept() {
    // Restore the session.
    SessionRestore::RestoreSession(profile_, NULL, true, false,
                                   std::vector<GURL>());
    return true;
  }

 private:
  // The Profile that we restore sessions from.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SessionCrashedInfoBarDelegate);
};

void SetOverrideHomePage(const CommandLine& command_line, PrefService* prefs) {
  // If homepage is specified on the command line, canonify & store it.
  if (command_line.HasSwitch(switches::kHomePage)) {
    std::wstring browser_directory;
    PathService::Get(base::DIR_CURRENT, &browser_directory);
    std::wstring new_homepage = URLFixerUpper::FixupRelativeFile(
        browser_directory,
        command_line.GetSwitchValue(switches::kHomePage));
    prefs->transient()->SetString(prefs::kHomePage, new_homepage);
    prefs->transient()->SetBoolean(prefs::kHomePageIsNewTabPage, false);
  }
}

// Checks the visiblilty of the enumerated window and signals once a visible
// window has been found.
BOOL CALLBACK BrowserWindowEnumeration(HWND window, LPARAM param) {
  bool* result = reinterpret_cast<bool*>(param);
  *result = IsWindowVisible(window) != 0;
  // Stops enumeration if a visible window has been found.
  return !*result;
}

SessionStartupPref GetSessionStartupPref(Profile* profile,
                                         const CommandLine& command_line) {
  SessionStartupPref pref = SessionStartupPref::GetStartupPref(profile);
  if (command_line.HasSwitch(switches::kRestoreLastSession))
    pref.type = SessionStartupPref::LAST;
  if (command_line.HasSwitch(switches::kIncognito) &&
      pref.type == SessionStartupPref::LAST) {
    // We don't store session information when incognito. If the user has
    // chosen to restore last session and launched incognito, fallback to
    // default launch behavior.
    pref.type = SessionStartupPref::DEFAULT;
  }
  return pref;
}

}  // namespace

// MessageWindow --------------------------------------------------------------

static bool in_startup = false;

// static
bool BrowserInit::InProcessStartup() {
  return in_startup;
}

BrowserInit::MessageWindow::MessageWindow(const std::wstring& user_data_dir)
    : window_(NULL),
      locked_(false) {
  // Look for a Chrome instance that uses the same profile directory:
  remote_window_ = FindWindowEx(HWND_MESSAGE,
                                NULL,
                                chrome::kMessageWindowClass,
                                user_data_dir.c_str());
}

BrowserInit::MessageWindow::~MessageWindow() {
  if (window_)
    DestroyWindow(window_);
}

bool BrowserInit::MessageWindow::NotifyOtherProcess() {
  if (!remote_window_)
    return false;

  // Found another window, send our command line to it
  // format is "START\0<<<current directory>>>\0<<<commandline>>>".
  std::wstring to_send(L"START\0", 6);  // want the NULL in the string.
  std::wstring cur_dir;
  if (!PathService::Get(base::DIR_CURRENT, &cur_dir))
    return false;
  to_send.append(cur_dir);
  to_send.append(L"\0", 1);  // Null separator.
  to_send.append(GetCommandLineW());
  to_send.append(L"\0", 1);  // Null separator.

  // Allow the current running browser window making itself the foreground
  // window (otherwise it will just flash in the taskbar).
  DWORD process_id = 0;
  DWORD thread_id = GetWindowThreadProcessId(remote_window_, &process_id);
  DCHECK(process_id);
  AllowSetForegroundWindow(process_id);

  // Gives 20 seconds timeout for the current browser process to respond.
  const int kTimeout = 20000;
  COPYDATASTRUCT cds;
  cds.dwData = 0;
  cds.cbData = static_cast<DWORD>((to_send.length() + 1) * sizeof(wchar_t));
  cds.lpData = const_cast<wchar_t*>(to_send.c_str());
  DWORD_PTR result = 0;
  if (SendMessageTimeout(remote_window_,
                         WM_COPYDATA,
                         NULL,
                         reinterpret_cast<LPARAM>(&cds),
                         SMTO_ABORTIFHUNG,
                         kTimeout,
                         &result)) {
    return true;
  }

  // The window is hung. Scan for every window to find a visible one.
  bool visible_window = false;
  EnumThreadWindows(thread_id,
                    &BrowserWindowEnumeration,
                    reinterpret_cast<LPARAM>(&visible_window));

  // If there is a visible browser window, ask the user before killing it.
  if (visible_window) {
    std::wstring text = l10n_util::GetString(IDS_BROWSER_HUNGBROWSER_MESSAGE);
    std::wstring caption = l10n_util::GetString(IDS_PRODUCT_NAME);
    if (IDYES != win_util::MessageBox(NULL, text, caption,
                                      MB_YESNO | MB_ICONSTOP | MB_TOPMOST)) {
      // The user denied. Quit silently.
      return true;
    }
  }

  // Time to take action. Kill the browser process.
  base::KillProcess(process_id, ResultCodes::HUNG, true);
  remote_window_ = NULL;
  return false;
}

void BrowserInit::MessageWindow::Create() {
  DCHECK(!window_);
  DCHECK(!remote_window_);
  HINSTANCE hinst = GetModuleHandle(NULL);

  WNDCLASSEX wc = {0};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = BrowserInit::MessageWindow::WndProcStatic;
  wc.hInstance = hinst;
  wc.lpszClassName = chrome::kMessageWindowClass;
  RegisterClassEx(&wc);

  std::wstring user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);

  // Set the window's title to the path of our user data directory so other
  // Chrome instances can decide if they should forward to us or not.
  window_ = CreateWindow(chrome::kMessageWindowClass, user_data_dir.c_str(),
                         0, 0, 0, 0, 0, HWND_MESSAGE, 0, hinst, 0);
  DCHECK(window_);

  win_util::SetWindowUserData(window_, this);
}

LRESULT BrowserInit::MessageWindow::OnCopyData(HWND hwnd,
                                               const COPYDATASTRUCT* cds) {
  // Ignore the request if the browser process is already in shutdown path.
  if (!g_browser_process || g_browser_process->IsShuttingDown())
    return TRUE;

  // If locked, it means we are not ready to process this message because
  // we are probably in a first run critical phase.
  if (locked_)
    return TRUE;

  // We should have enough room for the shortest command (min_message_size)
  // and also be a multiple of wchar_t bytes.
  static const int min_message_size = 7;
  if (cds->cbData < min_message_size || cds->cbData % sizeof(wchar_t) != 0) {
    LOG(WARNING) << "Invalid WM_COPYDATA, length = " << cds->cbData;
    return TRUE;
  }

  // We split the string into 4 parts on NULLs.
  const std::wstring msg(static_cast<wchar_t*>(cds->lpData),
                         cds->cbData / sizeof(wchar_t));
  const std::wstring::size_type first_null = msg.find_first_of(L'\0');
  if (first_null == 0 || first_null == std::wstring::npos) {
    // no NULL byte, don't know what to do
    LOG(WARNING) << "Invalid WM_COPYDATA, length = " << msg.length() <<
      ", first null = " << first_null;
    return TRUE;
  }

  // Decode the command, which is everything until the first NULL.
  if (msg.substr(0, first_null) == L"START") {
    // Another instance is starting parse the command line & do what it would
    // have done.
    LOG(INFO) << "Handling STARTUP request from another process";
    const std::wstring::size_type second_null =
      msg.find_first_of(L'\0', first_null + 1);
    if (second_null == std::wstring::npos ||
        first_null == msg.length() - 1 || second_null == msg.length()) {
      LOG(WARNING) << "Invalid format for start command, we need a string in 4 "
        "parts separated by NULLs";
      return TRUE;
    }

    // Get current directory.
    const std::wstring cur_dir =
      msg.substr(first_null + 1, second_null - first_null);

    const std::wstring::size_type third_null =
        msg.find_first_of(L'\0', second_null + 1);
    if (third_null == std::wstring::npos ||
        third_null == msg.length()) {
      LOG(WARNING) << "Invalid format for start command, we need a string in 4 "
        "parts separated by NULLs";
    }

    // Get command line.
    const std::wstring cmd_line =
      msg.substr(second_null + 1, third_null - second_null);

    CommandLine parsed_command_line(cmd_line);
    PrefService* prefs = g_browser_process->local_state();
    DCHECK(prefs);

    std::wstring user_data_dir;
    PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    Profile* profile = profile_manager->GetDefaultProfile(user_data_dir);
    if (!profile) {
      // We should only be able to get here if the profile already exists and
      // has been created.
      NOTREACHED();
      return TRUE;
    }
    ProcessCommandLine(parsed_command_line, cur_dir, prefs, false, profile,
                       NULL);
    return TRUE;
  }
  return TRUE;
}

LRESULT CALLBACK BrowserInit::MessageWindow::WndProc(HWND hwnd,
                                                     UINT message,
                                                     WPARAM wparam,
                                                     LPARAM lparam) {
  switch (message) {
    case WM_COPYDATA:
      return OnCopyData(reinterpret_cast<HWND>(wparam),
                        reinterpret_cast<COPYDATASTRUCT*>(lparam));
    default:
      break;
  }

  return ::DefWindowProc(hwnd, message, wparam, lparam);
}

void BrowserInit::MessageWindow::HuntForZombieChromeProcesses() {
  // Detecting dead renderers is simple:
  // - The process is named chrome.exe.
  // - The process' parent doesn't exist anymore.
  // - The process doesn't have a chrome::kMessageWindowClass window.
  // If these conditions hold, the process is a zombie renderer or plugin.

  // Retrieve the list of browser processes on start. This list is then used to
  // detect zombie renderer process or plugin process.
  class ZombieDetector : public base::ProcessFilter {
   public:
    ZombieDetector() {
      for (HWND window = NULL;;) {
        window = FindWindowEx(HWND_MESSAGE,
                              window,
                              chrome::kMessageWindowClass,
                              NULL);
        if (!window)
          break;
        DWORD process = 0;
        GetWindowThreadProcessId(window, &process);
        if (process)
          browsers_.push_back(process);
      }
      // We are also a browser, regardless of having the window or not.
      browsers_.push_back(::GetCurrentProcessId());
    }

    virtual bool Includes(uint32 pid, uint32 parent_pid) const {
      // Don't kill ourself eh.
      if (GetCurrentProcessId() == pid)
        return false;

      // Is this a browser? If so, ignore it.
      if (std::find(browsers_.begin(), browsers_.end(), pid) != browsers_.end())
        return false;

      // Is the parent a browser? If so, ignore it.
      if (std::find(browsers_.begin(), browsers_.end(), parent_pid)
          != browsers_.end())
        return false;

      // The chrome process is orphan.
      return true;
    }

   protected:
    std::vector<uint32> browsers_;
  };

  ZombieDetector zombie_detector;
  base::KillProcesses(L"chrome.exe", ResultCodes::HUNG, &zombie_detector);
}


// LaunchWithProfile ----------------------------------------------------------

BrowserInit::LaunchWithProfile::LaunchWithProfile(const std::wstring& cur_dir,
                                                  const std::wstring& cmd_line)
    : command_line_(cmd_line),
      cur_dir_(cur_dir) {
}

bool BrowserInit::LaunchWithProfile::Launch(Profile* profile,
                                            bool process_startup) {
  DCHECK(profile);
  profile_ = profile;

  CommandLine parsed_command_line(command_line_);
  if (parsed_command_line.HasSwitch(switches::kDnsLogDetails))
    chrome_browser_net::EnableDnsDetailedLog(true);
  if (parsed_command_line.HasSwitch(switches::kDnsPrefetchDisable))
    chrome_browser_net::EnableDnsPrefetch(false);

  if (parsed_command_line.HasSwitch(switches::kDumpHistogramsOnExit)) {
    StatisticsRecorder::set_dump_on_exit(true);
  }

  if (parsed_command_line.HasSwitch(switches::kRemoteShellPort)) {
    if (!RenderProcessHost::run_renderer_in_process()) {
      std::wstring port_str =
        parsed_command_line.GetSwitchValue(switches::kRemoteShellPort);
      int64 port = StringToInt64(port_str);
      if (port > 0 && port < 65535) {
        g_browser_process->InitDebuggerWrapper(static_cast<int>(port));
      } else {
        DLOG(WARNING) << "Invalid port number " << port;
      }
    }
  }

  if (parsed_command_line.HasSwitch(switches::kEnableFileCookies))
    net::CookieMonster::EnableFileScheme();

  if (parsed_command_line.HasSwitch(switches::kUserAgent)) {
    webkit_glue::SetUserAgent(WideToUTF8(
        parsed_command_line.GetSwitchValue(switches::kUserAgent)));
  }

#ifndef NDEBUG
  if (parsed_command_line.HasSwitch(switches::kApp)) {
    NOTREACHED();
  }
#endif  // NDEBUG

  std::vector<GURL> urls_to_open = GetURLsFromCommandLine(parsed_command_line,
                                                          profile_);

  Browser* browser = NULL;

  // Always attempt to restore the last session. OpenStartupURLs only opens the
  // home pages if no additional URLs were passed on the command line.
  bool opened_startup_urls =
      OpenStartupURLs(process_startup, parsed_command_line, urls_to_open);

  if (!opened_startup_urls) {
    if (urls_to_open.empty()) {
      urls_to_open.push_back(GURL());  // New tab page.
      PrefService* prefs = g_browser_process->local_state();
      if (prefs->IsPrefRegistered(prefs::kShouldShowWelcomePage) &&
          prefs->GetBoolean(prefs::kShouldShowWelcomePage)) {
        // Reset the preference so we don't show the welcome page next time.
        prefs->ClearPref(prefs::kShouldShowWelcomePage);

        // Add the welcome page.
        std::wstring welcome_url = l10n_util::GetString(IDS_WELCOME_PAGE_URL);
        urls_to_open.push_back(GURL(welcome_url));
      }
    } else {
      browser = BrowserList::GetLastActive();
    }

    browser = OpenURLsInBrowser(browser, process_startup, urls_to_open);
  }

  // It is possible to end here with a NULL 'browser'. For example if the user
  // has tweaked the startup session restore preferences.

  if (browser) {
    // If we're recording or playing back, startup the EventRecorder now
    // unless otherwise specified.
    if (!parsed_command_line.HasSwitch(switches::kNoEvents)) {
      std::wstring script_path;
      PathService::Get(chrome::FILE_RECORDED_SCRIPT, &script_path);

      bool record_mode = parsed_command_line.HasSwitch(switches::kRecordMode);
      bool playback_mode =
          parsed_command_line.HasSwitch(switches::kPlaybackMode);

      if (record_mode && chrome::kRecordModeEnabled)
        base::EventRecorder::current()->StartRecording(script_path);
      if (playback_mode)
        base::EventRecorder::current()->StartPlayback(script_path);
    }
  }

  // Start up the extensions service
  if (parsed_command_line.HasSwitch(switches::kEnableExtensions))
    profile->GetExtensionsService()->Init();

  return true;
}

bool BrowserInit::LaunchWithProfile::OpenStartupURLs(
    bool is_process_startup,
    const CommandLine& command_line,
    const std::vector<GURL>& urls_to_open) {
  SessionStartupPref pref = GetSessionStartupPref(profile_, command_line);
  switch (pref.type) {
    case SessionStartupPref::LAST:
      if (!is_process_startup)
        return false;

      if (!profile_->DidLastSessionExitCleanly() &&
          !command_line.HasSwitch(switches::kRestoreLastSession)) {
        // The last session crashed. It's possible automatically loading the
        // page will trigger another crash, locking the user out of chrome.
        // To avoid this, don't restore on startup but instead show the crashed
        // infobar.
        return false;
      }
      SessionRestore::RestoreSessionSynchronously(profile_, urls_to_open);
      return true;

    case SessionStartupPref::URLS:
      // When the user launches the app only open the default set of URLs if
      // we aren't going to open any URLs on the command line.
      if (urls_to_open.empty()) {
        if (pref.urls.empty())
          return false;  // No URLs to open.
        OpenURLsInBrowser(NULL, is_process_startup, pref.urls);
        return true;
      }
      return false;

    default:
      return false;
  }
}

Browser* BrowserInit::LaunchWithProfile::OpenURLsInBrowser(
    Browser* browser,
    bool process_startup,
    const std::vector<GURL>& urls) {
  DCHECK(!urls.empty());
  if (!browser || browser->type() != Browser::TYPE_NORMAL)
    browser = Browser::Create(profile_);

  for (size_t i = 0; i < urls.size(); ++i) {
    TabContents* tab = browser->AddTabWithURL(
        urls[i], GURL(), PageTransition::START_PAGE, (i == 0), NULL);
    if (i == 0 && process_startup)
      AddCrashedInfoBarIfNecessary(tab);
  }
  browser->window()->Show();
  return browser;
}

void BrowserInit::LaunchWithProfile::AddCrashedInfoBarIfNecessary(
    TabContents* tab) {
  // Assume that if the user is launching incognito they were previously
  // running incognito so that we have nothing to restore from.
  if (!profile_->DidLastSessionExitCleanly() &&
      !profile_->IsOffTheRecord()) {
    // The last session didn't exit cleanly. Show an infobar to the user
    // so that they can restore if they want. The delegate deletes itself when
    // it is closed.
    tab->AddInfoBar(new SessionCrashedInfoBarDelegate(tab));
  }
}

std::vector<GURL> BrowserInit::LaunchWithProfile::GetURLsFromCommandLine(
    const CommandLine& command_line, Profile* profile) {
  std::vector<GURL> urls;
  if (command_line.GetLooseValueCount() > 0) {
    for (CommandLine::LooseValueIterator iter =
         command_line.GetLooseValuesBegin();
         iter != command_line.GetLooseValuesEnd(); ++iter) {
      std::wstring value = *iter;
      // Handle Vista way of searching - "? <search-term>"
      if (value.find(L"? ") == 0) {
        const TemplateURL* const default_provider =
            profile->GetTemplateURLModel()->GetDefaultSearchProvider();
        if (!default_provider || !default_provider->url()) {
          // No search provider available. Just treat this as regular URL.
          urls.push_back(GURL(URLFixerUpper::FixupRelativeFile(cur_dir_,
                                                               value)));
          continue;
        }
        const TemplateURLRef* const search_url = default_provider->url();
        DCHECK(search_url->SupportsReplacement());
        urls.push_back(GURL(search_url->ReplaceSearchTerms(*default_provider,
            value.substr(2), TemplateURLRef::NO_SUGGESTIONS_AVAILABLE,
            std::wstring())));
      } else {
        // This will create a file URL or a regular URL.
        urls.push_back(GURL(URLFixerUpper::FixupRelativeFile(cur_dir_, value)));
      }
    }
  }
  return urls;
}

bool BrowserInit::ProcessCommandLine(const CommandLine& parsed_command_line,
    const std::wstring& cur_dir, PrefService* prefs, bool process_startup,
    Profile* profile, int* return_code) {
  DCHECK(profile);
  if (process_startup) {
    const std::wstring popup_count_string =
        parsed_command_line.GetSwitchValue(switches::kOmniBoxPopupCount);
    if (!popup_count_string.empty()) {
      const int popup_count = std::max(0, _wtoi(popup_count_string.c_str()));
      AutocompleteResult::set_max_matches(popup_count);
      AutocompleteProvider::set_max_matches(popup_count / 2);
    }

    if (parsed_command_line.HasSwitch(switches::kDisablePromptOnRepost))
      NavigationController::DisablePromptOnRepost();

    const std::wstring tab_count_string =
        parsed_command_line.GetSwitchValue(
            switches::kTabCountToLoadOnSessionRestore);
    if (!tab_count_string.empty()) {
      const int tab_count = std::max(0, _wtoi(tab_count_string.c_str()));
      SessionRestore::num_tabs_to_load_ = static_cast<size_t>(tab_count);
    }

    // Look for the testing channel ID ONLY during process startup
    if (parsed_command_line.HasSwitch(switches::kTestingChannelID)) {
      std::wstring testing_channel_id =
        parsed_command_line.GetSwitchValue(switches::kTestingChannelID);
      // TODO(sanjeevr) Check if we need to make this a singleton for
      // compatibility with the old testing code
      // If there are any loose parameters, we expect each one to generate a
      // new tab; if there are none then we get one homepage tab.
      CreateAutomationProvider<TestingAutomationProvider>(
          testing_channel_id,
          profile,
          std::max(static_cast<int>(parsed_command_line.GetLooseValueCount()),
                   1));
    }
  }

  // Allow the command line to override the persisted setting of home page.
  SetOverrideHomePage(parsed_command_line, profile->GetPrefs());

  if (parsed_command_line.HasSwitch(switches::kBrowserStartRenderersManually))
    prefs->transient()->SetBoolean(prefs::kStartRenderersManually, true);

  bool silent_launch = false;
  if (parsed_command_line.HasSwitch(switches::kAutomationClientChannelID)) {
    std::wstring automation_channel_id =
      parsed_command_line.GetSwitchValue(switches::kAutomationClientChannelID);
    // If there are any loose parameters, we expect each one to generate a
    // new tab; if there are none then we have no tabs
    size_t expected_tabs =
        std::max(static_cast<int>(parsed_command_line.GetLooseValueCount()),
                 0);
    if (expected_tabs == 0) {
      silent_launch = true;
    }
    CreateAutomationProvider<AutomationProvider>(automation_channel_id,
                                                 profile, expected_tabs);
  }
  // If we don't want to launch a new browser window or tab (in the case
  // of an automation request), we are done here.
  if (!silent_launch) {
    return LaunchBrowser(parsed_command_line, profile, cur_dir,
                         process_startup, return_code);
  }
  return true;
}

bool BrowserInit::LaunchBrowser(const CommandLine& parsed_command_line,
                                Profile* profile, const std::wstring& cur_dir,
                                bool process_startup, int* return_code) {
  in_startup = process_startup;
  bool result = LaunchBrowserImpl(parsed_command_line, profile, cur_dir,
                                  process_startup, return_code);
  in_startup = false;
  return result;
}

template <class AutomationProviderClass>
void BrowserInit::CreateAutomationProvider(const std::wstring& channel_id,
                                           Profile* profile,
                                           size_t expected_tabs) {
  scoped_refptr<AutomationProviderClass> automation =
      new AutomationProviderClass(profile);
  automation->ConnectToChannel(channel_id);
  automation->SetExpectedTabCount(expected_tabs);

  AutomationProviderList* list =
      g_browser_process->InitAutomationProviderList();
  DCHECK(list);
  list->AddProvider(automation);
}

bool BrowserInit::LaunchBrowserImpl(const CommandLine& parsed_command_line,
                                    Profile* profile,
                                    const std::wstring& cur_dir,
                                    bool process_startup,
                                    int* return_code) {
  DCHECK(profile);

  // Continue with the off-the-record profile from here on if --incognito
  if (parsed_command_line.HasSwitch(switches::kIncognito))
    profile = profile->GetOffTheRecordProfile();

  // Are we starting an application?
  std::wstring app_url = parsed_command_line.GetSwitchValue(switches::kApp);
  if (!app_url.empty()) {
    GURL url(app_url);
    // If the application is started for a mailto:url, this machine has some
    // old configuration that we should ignore. This hack saves us from some
    // infinite loops where we keep forwarding mailto: to the system, resulting
    // in the system asking us to open the mailto app.
    if (url.SchemeIs("mailto"))
      url = GURL("about:blank");

    WebAppLauncher::Launch(profile, url);
    return true;
  }

  LaunchWithProfile lwp(cur_dir, parsed_command_line.command_line_string());
  bool launched = lwp.Launch(profile, process_startup);
  if (!launched) {
    LOG(ERROR) << "launch error";
    if (return_code != NULL) {
      *return_code = ResultCodes::INVALID_CMDLINE_URL;
    }
    return false;
  }

  return true;
}
