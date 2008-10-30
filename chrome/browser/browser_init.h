// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_INIT_H__
#define CHROME_BROWSER_BROWSER_INIT_H__

#include <atlbase.h>
#include <atlwin.h>
#include <windows.h>
#include <string>
#include <vector>

#include "base/gfx/rect.h"
#include "chrome/browser/profile_manager.h"

class Browser;
class CommandLine;
class GURL;
class Profile;
class TabContents;

// Scoper class containing helpers for BrowserMain to spin up a new instance
// and initialize the profile.
class BrowserInit {
 public:
  // Returns true if the browser is coming up.
  static bool InProcessStartup();

  // MessageWindow -------------------------------------------------------------
  //
  // Class for dealing with the invisible global message window for IPC. This
  // window allows different browser processes to communicate with each other.
  // It is named according to the user data directory, so we can be sure that
  // no more than one copy of the application can be running at once with a
  // given data directory.

  class MessageWindow {
   public:
    explicit MessageWindow(const std::wstring& user_data_dir);
    ~MessageWindow();

    // Returns true if another process was found and notified, false if we
    // should continue with this process. Roughly based on Mozilla
    //
    // TODO(brettw): this will not handle all cases. If two process start up too
    // close to each other, the window might not have been created yet for the
    // first one, so this function won't find it.
    bool NotifyOtherProcess(int show_cmd);

    // Create the toplevel message window for IPC.
    void Create();

    // Blocks the dispatch of CopyData messages.
    void Lock() {
      locked_ = true;
    }

    // Allows the dispatch of CopyData messages.
    void Unlock() {
      locked_ = false;
    }

    // This ugly behemoth handles startup commands sent from another process.
    LRESULT OnCopyData(HWND hwnd, const COPYDATASTRUCT* cds);

    // Looks for zombie renderer and plugin processes that could have survived.
    void HuntForZombieChromeProcesses();

   private:
    LRESULT CALLBACK WndProc(HWND hwnd,
                             UINT message,
                             WPARAM wparam,
                             LPARAM lparam);

    static LRESULT CALLBACK WndProcStatic(HWND hwnd,
                                          UINT message,
                                          WPARAM wparam,
                                          LPARAM lparam) {
      MessageWindow* msg_wnd = reinterpret_cast<MessageWindow*>(
          GetWindowLongPtr(hwnd, GWLP_USERDATA));
      return msg_wnd->WndProc(hwnd, message, wparam, lparam);
    }

    HWND remote_window_;  // The HWND_MESSAGE of another browser.
    HWND window_;  // The HWND_MESSAGE window.
    bool locked_;

    DISALLOW_EVIL_CONSTRUCTORS(MessageWindow);
  };

  // LaunchWithProfile ---------------------------------------------------------
  //
  // Assists launching the application and appending the initial tabs for a
  // browser window.

  class LaunchWithProfile {
   public:
    LaunchWithProfile(const std::wstring& cur_dir,
                      const std::wstring& cmd_line,
                      int show_command);
    ~LaunchWithProfile() { }

    // Creates the necessary windows for startup. Returns true on success,
    // false on failure. process_startup is true if Chrome is just
    // starting up. If process_startup is false, it indicates Chrome was
    // already running and the user wants to launch another instance.
    bool Launch(Profile* profile, bool process_startup);

   private:
    // Creates a new tabbed browser.
    //
    // Note that the window returned by this function may not be visible yet.
    Browser* CreateTabbedBrowser();

    // Does the following:
    // . If the user's startup pref is to restore the last session (or the
    //   command line flag is present to force using last session), it is
    //   restored, and true is returned.
    // . If the user's startup pref is to launch a specific set of URLs, and
    //   urls_to_open is empty, the user specified set of URLs is openned.
    //
    // Otherwise false is returned.
    bool OpenStartupURLs(bool is_process_startup,
                         const CommandLine& command_line,
                         const std::vector<GURL>& urls_to_open);

    // Opens the list of urls. If browser is non-null and a tabbed browser, the
    // URLs are opened in it. Otherwise a new tabbed browser is created and the
    // URLs are added to it. The browser the tabs are added to is returned,
    // which is either |browser| or the newly created browser.
    Browser* OpenURLsInBrowser(Browser* browser,
                               bool process_startup,
                               const std::vector<GURL>& urls);

    // If the last session didn't exit cleanly and tab is a web contents tab,
    // an infobar is added allowing the user to restore the last session.
    void AddCrashedInfoBarIfNecessary(TabContents* tab);

    // Returns the list of URLs to open from the command line. The returned
    // vector is empty if the user didn't specify any URLs on the command line.
    std::vector<GURL> GetURLsFromCommandLine(const CommandLine& command_line,
                                             Profile* profile);

    std::wstring cur_dir_;
    std::wstring command_line_;
    int show_command_;
    Profile* profile_;

    // Bounds for the browser.
    gfx::Rect start_position_;

    DISALLOW_EVIL_CONSTRUCTORS(LaunchWithProfile);
  };

  // This function performs command-line handling and is invoked when
  // process starts as well as when we get a start request from another
  // process (via the WM_COPYDATA message). The process_startup flag
  // indicates if this is being called from the process startup code or
  // the WM_COPYDATA handler.
  static bool ProcessCommandLine(const CommandLine& parsed_command_line,
                                 const std::wstring& cur_dir,
                                 PrefService* prefs, int show_command,
                                 bool process_startup, Profile* profile,
                                 int* return_code);

  // Helper function to launch a new browser based on command-line arguments
  // This function takes in a specific profile to use.
  static bool LaunchBrowser(const CommandLine& parsed_command_line,
                            Profile* profile, int show_command,
                            const std::wstring& cur_dir, bool process_startup,
                            int* return_code);

  template <class AutomationProviderClass>
  static void CreateAutomationProvider(const std::wstring& channel_id,
                                       Profile* profile,
                                       size_t expected_tabs);

 private:
  // Does the work of LaunchBrowser returning the result.
  static bool LaunchBrowserImpl(const CommandLine& parsed_command_line,
                                Profile* profile, int show_command,
                                const std::wstring& cur_dir,
                                bool process_startup, int* return_code);

  // This class is for scoping purposes.
  BrowserInit();
  DISALLOW_EVIL_CONSTRUCTORS(BrowserInit);
};

#endif  // CHROME_BROWSER_BROWSER_INIT_H__

