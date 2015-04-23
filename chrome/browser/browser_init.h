// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_INIT_H_
#define CHROME_BROWSER_BROWSER_INIT_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"

class Browser;
class CommandLine;
class GURL;
class PrefService;
class Profile;
class TabContents;

// class containing helpers for BrowserMain to spin up a new instance and
// initialize the profile.
class BrowserInit {
 public:
  BrowserInit() {};
  ~BrowserInit() {};

  // Adds a url to be opened during first run. This overrides the standard
  // tabs shown at first run.
  void AddFirstRunTab(const GURL& url) {
    first_run_tabs_.push_back(url);
  }

  // This function is equivalent to ProcessCommandLine but should only be
  // called during actual process startup.
  bool Start(const CommandLine& cmd_line, const std::wstring& cur_dir,
             Profile* profile, int* return_code) {
    return ProcessCmdLineImpl(cmd_line, cur_dir, true, profile, return_code,
                              this);
  }

  // This function performs command-line handling and is invoked when
  // process starts as well as when we get a start request from another
  // process (via the WM_COPYDATA message). |command_line| holds the command
  // line we need to process - either from this process or from some other one
  // (if |process_startup| is true and we are being called from
  //  ProcessSingleton::OnCopyData).
  static bool ProcessCommandLine(const CommandLine& cmd_line,
                                 const std::wstring& cur_dir,
                                 bool process_startup, Profile* profile,
                                 int* return_code) {
    return ProcessCmdLineImpl(cmd_line, cur_dir, process_startup, profile,
                              return_code, NULL);
  }

  template <class AutomationProviderClass>
  static void CreateAutomationProvider(const std::string& channel_id,
                                       Profile* profile,
                                       size_t expected_tabs);

  // Returns true if the browser is coming up.
  static bool InProcessStartup();

  // LaunchWithProfile ---------------------------------------------------------
  //
  // Assists launching the application and appending the initial tabs for a
  // browser window.

  class LaunchWithProfile {
   public:
    // There are two ctors. The first one implies a NULL browser_init object
    // and thus no access to distribution-specific first-run behaviors. The
    // second one is always called when the browser starts even if it is not
    // the first run.
    LaunchWithProfile(const std::wstring& cur_dir,
                      const CommandLine& command_line);
    LaunchWithProfile(const std::wstring& cur_dir,
                      const CommandLine& command_line,
                      BrowserInit* browser_init);
    ~LaunchWithProfile() { }

    // Creates the necessary windows for startup. Returns true on success,
    // false on failure. process_startup is true if Chrome is just
    // starting up. If process_startup is false, it indicates Chrome was
    // already running and the user wants to launch another instance.
    bool Launch(Profile* profile, bool process_startup);

    // Opens the list of urls. If browser is non-null and a tabbed browser, the
    // URLs are opened in it. Otherwise a new tabbed browser is created and the
    // URLs are added to it. The browser the tabs are added to is returned,
    // which is either |browser| or the newly created browser.
    Browser* OpenURLsInBrowser(Browser* browser,
                               bool process_startup,
                               const std::vector<GURL>& urls);

   private:
    // If the process was launched with the web application command line flag,
    // e.g. --app=http://www.google.com/, opens a web application browser and
    // returns true. If there is no web application command line flag speciifed,
    // returns false to specify default processing.
    bool OpenApplicationURL(Profile* profile);

    // Does the following:
    // . If the user's startup pref is to restore the last session (or the
    //   command line flag is present to force using last session), it is
    //   restored, and true is returned.
    // . If the user's startup pref is to launch a specific set of URLs, and
    //   urls_to_open is empty, the user specified set of URLs is openned.
    //
    // Otherwise false is returned.
    bool OpenStartupURLs(bool is_process_startup,
                         const std::vector<GURL>& urls_to_open);

    // If the last session didn't exit cleanly and tab is a web contents tab,
    // an infobar is added allowing the user to restore the last session.
    void AddCrashedInfoBarIfNecessary(TabContents* tab);

    // Returns the list of URLs to open from the command line. The returned
    // vector is empty if the user didn't specify any URLs on the command line.
    std::vector<GURL> GetURLsFromCommandLine(Profile* profile);

    // Adds additional startup URLs to the specified vector.
    void AddStartupURLs(std::vector<GURL>* startup_urls) const;

    // Checks whether Chrome is still the default browser (unless the user
    // previously instructed not to do so) and warns the user if it is not.
    void CheckDefaultBrowser(Profile* profile);

    std::wstring cur_dir_;
    const CommandLine& command_line_;
    Profile* profile_;
    BrowserInit* browser_init_;
    DISALLOW_COPY_AND_ASSIGN(LaunchWithProfile);
  };

 private:
  static bool ProcessCmdLineImpl(const CommandLine& command_line,
                                 const std::wstring& cur_dir,
                                 bool process_startup, Profile* profile,
                                 int* return_code, BrowserInit* browser_init);

  // Additional tabs to open during first run.
  std::vector<GURL> first_run_tabs_;

  DISALLOW_COPY_AND_ASSIGN(BrowserInit);
};

#endif  // CHROME_BROWSER_BROWSER_INIT_H_
