// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_H_
#define CHROME_BROWSER_FIRST_RUN_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gfx/native_widget_types.h"
#include "chrome/browser/browser_process_impl.h"

class CommandLine;
class FilePath;
class Profile;
class ProcessSingleton;

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
  static bool ImportSettings(Profile* profile, int browser_type,
                             int items_to_import,
                             gfx::NativeView parent_window);
  // Import browser items in this process. The browser and the items to
  // import are encoded int the command line. This function is paired with
  // FirstRun::ImportSettings(). This function might or might not show
  // a visible UI depending on the cmdline parameters.
  static int ImportNow(Profile* profile, const CommandLine& cmdline);

  // The master preferences is a JSON file with the same entries as the
  // 'Default\Preferences' file. This function locates this file from
  // master_pref_path or if that path is empty from the default location
  // which is '<path to chrome.exe>\master_preferences', and process it
  // so it becomes the default preferences in profile pointed by user_data_dir.
  // After processing the file, the function returns true if showing the
  // first run dialog is needed, and returns false if skipping first run
  // dialogs. The detailed settings in the preference file is reported via
  // preference_details.
  //
  // This function destroys any existing prefs file and it is meant to be
  // invoked only on first run.
  //
  // See chrome/installer/util/master_preferences.h for a description of
  // 'master_preferences' file.
  static bool ProcessMasterPreferences(const FilePath& user_data_dir,
                                       const FilePath& master_prefs_path,
                                       int* preference_details,
                                       std::vector<std::wstring>* new_tabs);

  // Sets the kShouldShowFirstRunBubble local state pref so that the browser
  // shows the bubble once the main message loop gets going. Returns false if
  // the pref could not be set.
  static bool SetShowFirstRunBubblePref();

  // Sets the kShouldUseOEMFirstRunBubble local state pref so that the
  // browser shows the OEM first run bubble once the main message loop
  // gets going. Returns false if the pref could not be set.
  static bool SetOEMFirstRunBubblePref();

  // Sets the kShouldShowWelcomePage local state pref so that the browser
  // loads the welcome tab once the message loop gets going. Returns false
  // if the pref could not be set.
  static bool SetShowWelcomePagePref();

 private:
  // This class is for scoping purposes.
  DISALLOW_IMPLICIT_CONSTRUCTORS(FirstRun);
};

// This class contains the actions that need to be performed when an upgrade
// is required. This involves mainly swapping the chrome exe and relaunching
// the new browser.
class Upgrade {
 public:
  // Possible results of ShowTryChromeDialog().
  enum TryResult {
    TD_TRY_CHROME,          // Launch chrome right now.
    TD_NOT_NOW,             // Don't launch chrome. Exit now.
    TD_UNINSTALL_CHROME,    // Initiate chrome uninstall and exit.
    TD_DIALOG_ERROR,        // An error occurred creating the dialog.
    TD_LAST_ENUM
  };

  // Check if current chrome.exe is already running as a browser process by
  // trying to create a Global event with name same as full path of chrome.exe.
  // This method caches the handle to this event so on subsequent calls also
  // it can first close the handle and check for any other process holding the
  // handle to the event.
  static bool IsBrowserAlreadyRunning();

  // Launches chrome again simulating a 'user' launch. If chrome could not
  // be launched the return is false.
  static bool RelaunchChromeBrowser(const CommandLine& command_line);

  // If the new_chrome.exe exists (placed by the installer then is swapped
  // to chrome.exe and the old chrome is renamed to old_chrome.exe. If there
  // is no new_chrome.exe or the swap fails the return is false;
  static bool SwapNewChromeExeIfPresent();

  // Shows a modal dialog asking the user to give chrome another try. See
  // above for the possible outcomes of the function. This is an experimental,
  // non-localized dialog.
  static TryResult ShowTryChromeDialog();
};

// A subclass of BrowserProcessImpl that does not have a GoogleURLTracker
// so we don't fetch as we have no IO thread (see bug #1292702).
class FirstRunBrowserProcess : public BrowserProcessImpl {
 public:
  FirstRunBrowserProcess(const CommandLine& command_line)
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
// |process_singleton| is used to lock the handling of CopyData messages
// while the First Run UI is visible.
// Returns true if the user clicked "Start", false if the user pressed "Cancel"
// or closed the dialog.
bool OpenFirstRunDialog(Profile* profile, ProcessSingleton* process_singleton);

#endif  // CHROME_BROWSER_FIRST_RUN_H_
