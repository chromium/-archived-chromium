// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_init.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/event_recorder.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "chrome/app/locales/locale_settings.h"
#include "chrome/app/result_codes.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/automation/automation_provider_list.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/session_startup_pref.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/web_app_launcher.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "net/base/cookie_monster.h"
#include "webkit/glue/webkit_glue.h"

#include "chromium_strings.h"
#include "generated_resources.h"

namespace {

// A delegate for the InfoBar shown when the previous session has crashed. The
// bar deletes itself automatically after it is closed.
// TODO(timsteele): This delegate can leak when a tab is closed, see
// http://crbug.com/6520
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

static bool in_startup = false;

// static
bool BrowserInit::InProcessStartup() {
  return in_startup;
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

  CommandLine parsed_command_line(L"");
  parsed_command_line.ParseFromString(command_line_);
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
  profile->InitExtensions();
  if (parsed_command_line.HasSwitch(switches::kInstallExtension)) {
    std::wstring path_string =
      parsed_command_line.GetSwitchValue(switches::kInstallExtension);
    FilePath path = FilePath::FromWStringHack(path_string);
    profile->GetExtensionsService()->InstallExtension(path);
  }

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
  std::vector<std::wstring> params = command_line.GetLooseValues();
  for (size_t i = 0; i < params.size(); ++i) {
    const std::wstring& value = params[i];
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
          std::max(static_cast<int>(parsed_command_line.GetLooseValues().size()),
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
        std::max(static_cast<int>(parsed_command_line.GetLooseValues().size()),
                 0);
    if (expected_tabs == 0) {
      silent_launch = true;
    }
    CreateAutomationProvider<AutomationProvider>(automation_channel_id,
                                                 profile, expected_tabs);
  }

  // Start up the extensions service.
  profile->InitExtensions();

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
