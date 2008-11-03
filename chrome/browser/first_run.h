// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_H_
#define CHROME_BROWSER_FIRST_RUN_H_

#include "base/basictypes.h"
#include "base/command_line.h"
#include "chrome/browser/browser_process_impl.h"

class Profile;

// This class contains the chrome first-run installation actions needed to
// fully test the custom installer. It also contains the opposite actions to
// execute during uninstall. When the first run UI is ready we won't
// do the actions unconditionally. Currently the only action is to create a
// desktop shortcut.
//
// The way we detect first-run is by looking at a 'sentinel' file.
// If it does not exists we understand that we need to do the first time
// install work for this user. After that the sentinel file is created.
class FirstRun {
 public:
  // Returns true if this is the first time chrome is run for this user.
  static bool IsChromeFirstRun();
  // Creates the desktop shortcut to chrome for the current user. Returns
  // false if it fails. It will overwrite the shortcut if it exists.
  static bool CreateChromeDesktopShortcut();
  // Creates the quick launch shortcut to chrome for the current user. Returns
  // false if it fails. It will overwrite the shortcut if it exists.
  static bool CreateChromeQuickLaunchShortcut();
  // Creates the sentinel file that signals that chrome has been configured.
  static bool CreateSentinel();
  // Removes the sentinel file created in ConfigDone(). Returns false if the
  // sentinel file could not be removed.
  static bool RemoveSentinel();
  // Imports settings in a separate process. It spawns a second dedicated
  // browser process that just does the import with the import progress UI.
  static bool ImportSettings(Profile* profile, int browser,
                             int items_to_import, HWND parent_window);
  // Import browser items in this process. The browser and the items to
  // import are encoded int the command line. This function is paired with
  // FirstRun::ImportSettings(). This function might or might not show
  // a visible UI depending on the cmdline parameters.
  static int ImportNow(Profile* profile, const CommandLine& cmdline);

  // These are the possible results of calling ProcessMasterPreferences.
  // Some of the results can be combined, so they are bit flags.
  enum MasterPrefResult {
    MASTER_PROFILE_NOT_FOUND          = 0,
    MASTER_PROFILE_ERROR              = 1,
    MASTER_PROFILE_SHOW_EULA          = 2,
    MASTER_PROFILE_NO_FIRST_RUN_UI    = 4,
    MASTER_PROFILE_DO_FIRST_RUN_UI    = 8
  };

  // The master preferences is a JSON file with the same entries as the
  // 'Default\Preferences' file. This function locates this file from
  // master_pref_path or if that path is empty from the default location
  // which is '<path to chrome.exe>\master_preferences', and process it
  // so it becomes the default preferences in profile pointed by user_data_dir.
  //
  // This function destroys any existing prefs file and it is meant to be
  // invoked only on first run.
  //
  // A prototypical 'master_preferences' file looks like this:
  //
  // {
  //   "distribution": {
  //      "skip_first_run_ui": true,
  //      "show_welcome_page": true,
  //      "import_search_engine": true,
  //      "import_history": false
  //   },
  //   "browser": {
  //      "show_home_button": true
  //   },
  //   "bookmark_bar": {
  //      "show_on_all_tabs": true
  //   },
  //   "homepage": "http://example.org",
  //   "homepage_is_newtabpage": false
  // }
  //
  // A reserved "distribution" entry in the file is used to group related
  // installation properties. This entry will be ignored at other times.
  //
  // Currently only the following return values are used:
  // MASTER_PROFILE_NOT_FOUND : Typical outcome for organic installs.
  // MASTER_PROFILE_ERROR : A critical error processing the master profile.
  // MASTER_PROFILE_NO_FIRST_RUN_UI : skip first run dialogs.
  // MASTER_PROFILE_DO_FIRST_RUN_UI : show the first run dialogs.
  static MasterPrefResult ProcessMasterPreferences(
      const std::wstring& user_data_dir,
      const std::wstring& master_prefs_path);

  // Sets the kShouldShowFirstRunBubble local state pref so that the browser
  // shows the bubble once the main message loop gets going. Returns false if
  // the pref could not be set.
  static bool SetShowFirstRunBubblePref();

  // Sets the kShouldShowWelcomePage local state pref so that the browser
  // loads the welcome tab once the message loop gets going. Returns false
  // if the pref could not be set.
  static bool SetShowWelcomePagePref();

 private:
  // This class is for scoping purposes.
  DISALLOW_COPY_AND_ASSIGN(FirstRun);
};

// This class contains the actions that need to be performed when an upgrade
// is required. This involves mainly swapping the chrome exe and relaunching
// the new browser.
class Upgrade {
 public:
  // If the new_chrome.exe exists (placed by the installer then is swapped
  // to chrome.exe and the old chrome is renamed to old_chrome.exe. If there
  // is no new_chrome.exe or the swap fails the return is false;
  static bool SwapNewChromeExeIfPresent();
  // Launches chrome again simulating a 'user' launch. If chrome could not
  // be launched the return is false.
  static bool RelaunchChromeBrowser(const CommandLine& command_line);
};

// A subclass of BrowserProcessImpl that does not have a GoogleURLTracker
// so we don't fetch as we have no IO thread (see bug #1292702).
class FirstRunBrowserProcess : public BrowserProcessImpl {
 public:
  FirstRunBrowserProcess(CommandLine& command_line)
      : BrowserProcessImpl(command_line) {
  }
  virtual ~FirstRunBrowserProcess() { }

  virtual GoogleURLTracker* google_url_tracker() { return NULL; }

 private:
  DISALLOW_COPY_AND_ASSIGN(FirstRunBrowserProcess);
};

// Show the First Run UI to the user, allowing them to create shortcuts for
// the app, import their bookmarks and other data from another browser into
// |profile| and perhaps some other tasks.
void OpenFirstRunDialog(Profile* profile);

#endif  // CHROME_BROWSER_FIRST_RUN_H_

