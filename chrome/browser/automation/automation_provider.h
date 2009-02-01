// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This implements a browser-side endpoint for UI automation activity.
// The client-side endpoint is implemented by AutomationProxy.
// The entire lifetime of this object should be contained within that of
// the BrowserProcess, and in particular the NotificationService that's
// hung off of it.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_PROVIDER_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_PROVIDER_H_

#include <map>
#include <string>
#include <vector>

#include "chrome/browser/automation/automation_browser_tracker.h"
#include "chrome/browser/automation/automation_constrained_window_tracker.h"
#include "chrome/browser/automation/automation_tab_tracker.h"
#include "chrome/browser/automation/automation_window_tracker.h"
#include "chrome/browser/automation/automation_autocomplete_edit_tracker.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/history/history.h"
#include "chrome/common/ipc_channel_proxy.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/notification_observer.h"
#include "chrome/views/event.h"

class LoginHandler;
class NavigationControllerRestoredObserver;

class AutomationProvider : public base::RefCounted<AutomationProvider>,
                           public IPC::Channel::Listener,
                           public IPC::Message::Sender {
 public:
  explicit AutomationProvider(Profile* profile);
  virtual ~AutomationProvider();

  // Establishes a connection to an automation client, if present.
  // An AutomationProxy should be established (probably in a different process)
  // before calling this.
  void ConnectToChannel(const std::wstring& channel_id);

  // Sets the number of tabs that we expect; when this number of tabs has
  // loaded, an AutomationMsg_InitialLoadsComplete message is sent.
  void SetExpectedTabCount(size_t expected_tabs);

  // Add a listener for navigation status notification. Currently only
  // navigation completion is observed; when the navigation completes, the
  // completed_response object is sent; if the server requires authentication,
  // we instead send the auth_needed_response object.  A pointer to the added
  // navigation observer is returned. This object should NOT be deleted and
  // should be released by calling the corresponding
  // RemoveNavigationStatusListener method.
  NotificationObserver* AddNavigationStatusListener(
      NavigationController* tab, IPC::Message* completed_response,
      IPC::Message* auth_needed_response);
  void RemoveNavigationStatusListener(NotificationObserver* obs);

  // Add an observer for the TabStrip. Currently only Tab append is observed. A
  // navigation listener is created on successful notification of tab append. A
  // pointer to the added navigation observer is returned. This object should
  // NOT be deleted and should be released by calling the corresponding
  // RemoveTabStripObserver method.
  NotificationObserver* AddTabStripObserver(Browser* parent,
                                            int32 routing_id);
  void RemoveTabStripObserver(NotificationObserver* obs);

  // Get the index of a particular NavigationController object
  // in the given parent window.  This method uses
  // TabStrip::GetIndexForNavigationController to get the index.
  int GetIndexForNavigationController(const NavigationController* controller,
                                      const Browser* parent) const;

  // Add or remove a non-owning reference to a tab's LoginHandler.  This is for
  // when a login prompt is shown for HTTP/FTP authentication.
  // TODO(mpcomplete): The login handling is a fairly special purpose feature.
  // Eventually we'll probably want ways to interact with the ChromeView of the
  // login window in a generic manner, such that it can be used for anything,
  // not just logins.
  void AddLoginHandler(NavigationController* tab, LoginHandler* handler);
  void RemoveLoginHandler(NavigationController* tab);

  // IPC implementations
  virtual bool Send(IPC::Message* msg);
  virtual void OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelError();

  // Received response from inspector controller
  void ReceivedInspectElementResponse(int num_resources);

 private:
  // IPC Message callbacks.
  void CloseBrowser(const IPC::Message& message, int handle);
  void ActivateTab(const IPC::Message& message, int handle, int at_index);
  void AppendTab(const IPC::Message& message, int handle, const GURL& url);
  void CloseTab(const IPC::Message& message,
                int tab_handle,
                bool wait_until_closed);

  void GetActiveTabIndex(const IPC::Message& message, int handle);
  void GetCookies(const IPC::Message& message, const GURL& url, int handle);
  void SetCookie(const IPC::Message& message,
                 const GURL& url,
                 const std::string value,
                 int handle);
  void GetBrowserWindowCount(const IPC::Message& message);
  void GetShowingAppModalDialog(const IPC::Message& message);
  void ClickAppModalDialogButton(const IPC::Message& message,
                                 int button);
  void GetBrowserWindow(const IPC::Message& message, int index);
  void GetLastActiveBrowserWindow(const IPC::Message& message);
  void GetActiveWindow(const IPC::Message& message);
  void GetWindowHWND(const IPC::Message& message, int handle);
  void ExecuteBrowserCommand(const IPC::Message& message,
                             int handle,
                             int command);
  void WindowGetViewBounds(const IPC::Message& message,
                           int handle,
                           int view_id,
                           bool screen_coordinates);
  void WindowSimulateDrag(const IPC::Message& message,
                          int handle,
                          std::vector<POINT> drag_path,
                          int flags,
                          bool press_escape_en_route);
  void WindowSimulateClick(const IPC::Message& message,
                          int handle,
                          POINT click,
                          int flags);
  void WindowSimulateKeyPress(const IPC::Message& message,
                              int handle,
                              wchar_t key,
                              int flags);
  void SetWindowVisible(const IPC::Message& message, int handle, bool visible);
  void IsWindowActive(const IPC::Message& message, int handle);
  void ActivateWindow(const IPC::Message& message, int handle);

  void GetTabCount(const IPC::Message& message, int handle);
  void GetTab(const IPC::Message& message, int win_handle, int tab_index);
  void GetTabHWND(const IPC::Message& message, int handle);
  void GetTabProcessID(const IPC::Message& message, int handle);
  void GetTabTitle(const IPC::Message& message, int handle);
  void GetTabURL(const IPC::Message& message, int handle);
  void HandleUnused(const IPC::Message& message, int handle);
  void NavigateToURL(const IPC::Message& message, int handle, const GURL& url);
  void NavigationAsync(const IPC::Message& message,
                       int handle,
                       const GURL& url);
  void GoBack(const IPC::Message& message, int handle);
  void GoForward(const IPC::Message& message, int handle);
  void Reload(const IPC::Message& message, int handle);
  void SetAuth(const IPC::Message& message, int tab_handle,
               const std::wstring& username, const std::wstring& password);
  void CancelAuth(const IPC::Message& message, int tab_handle);
  void NeedsAuth(const IPC::Message& message, int tab_handle);
  void GetRedirectsFrom(const IPC::Message& message,
                        int tab_handle,
                        const GURL& source_url);
  void ExecuteJavascript(const IPC::Message& message,
                         int handle,
                         const std::wstring& frame_xpath,
                         const std::wstring& script);
  void GetShelfVisibility(const IPC::Message& message, int handle);
  void SetFilteredInet(const IPC::Message& message, bool enabled);

  void ScheduleMouseEvent(views::View* view,
                          views::Event::EventType type,
                          POINT point,
                          int flags);
  void GetFocusedViewID(const IPC::Message& message, int handle);

  // Helper function to find the browser window that contains a given
  // NavigationController and activate that tab.
  // Returns the Browser if found.
  Browser* FindAndActivateTab(NavigationController* contents);

  // Apply an accelerator with id (like IDC_BACK, IDC_FORWARD ...)
  // to the Browser with given handle.
  void ApplyAccelerator(int handle, int id);

  void GetConstrainedWindowCount(const IPC::Message& message,
                                 int handle);
  void GetConstrainedWindow(const IPC::Message& message, int handle, int index);

  void GetConstrainedTitle(const IPC::Message& message, int handle);

  void GetConstrainedWindowBounds(const IPC::Message& message,
                                  int handle);

  // This function has been deprecated, please use HandleFindRequest.
  void HandleFindInPageRequest(const IPC::Message& message,
                               int handle,
                               const std::wstring& find_request,
                               int forward,
                               int match_case);

  // Responds to the FindInPage request, retrieves the search query parameters,
  // launches an observer to listen for results and issues a StartFind request.
  void HandleFindRequest(const IPC::Message& message,
                         int handle,
                         const FindInPageRequest& request);

  // Responds to requests to open the FindInPage window.
  void HandleOpenFindInPageRequest(const IPC::Message& message,
                                   int handle);

  // Get the visibility state of the Find window.
  void GetFindWindowVisibility(const IPC::Message& message, int handle);

  // Responds to requests to find the location of the Find window.
  void HandleFindWindowLocationRequest(const IPC::Message& message, int handle);

  // Get the visibility state of the Bookmark bar.
  void GetBookmarkBarVisitility(const IPC::Message& message, int handle);

  // Responds to InspectElement request
  void HandleInspectElementRequest(const IPC::Message& message,
                                   int handle,
                                   int x,
                                   int y);

  void GetDownloadDirectory(const IPC::Message& message, int handle);

  // Retrieves a Browser from a Window and vice-versa.
  void GetWindowForBrowser(const IPC::Message& message, int window_handle);
  void GetBrowserForWindow(const IPC::Message& message, int browser_handle);

  void GetAutocompleteEditForBrowser(const IPC::Message& message,
                                     int browser_handle);

  void OpenNewBrowserWindow(int show_command);

  void ShowInterstitialPage(const IPC::Message& message,
                            int tab_handle,
                            const std::string& html_text);
  void HideInterstitialPage(const IPC::Message& message, int tab_handle);

  void CreateExternalTab(const IPC::Message& message, HWND parent,
                         const gfx::Rect& dimensions, unsigned int style);
  void NavigateInExternalTab(const IPC::Message& message, int handle,
                             const GURL& url);
  // The container of an externally hosted tab calls this to reflect any
  // accelerator keys that it did not process. This gives the tab a chance
  // to handle the keys
  void ProcessUnhandledAccelerator(const IPC::Message& message, int handle,
                                   const MSG& msg);

  // See comment in AutomationMsg_WaitForTabToBeRestored.
  void WaitForTabToBeRestored(const IPC::Message& message, int tab_handle);

  // This sets the keyboard accelerators to be used by an externally
  // hosted tab. This call is not valid on a regular tab hosted within
  // Chrome.
  void SetAcceleratorsForTab(const IPC::Message& message, int handle,
                             HACCEL accel_table, int accel_entry_count);

  // Gets the security state for the tab associated to the specified |handle|.
  void GetSecurityState(const IPC::Message& message, int handle);

  // Gets the page type for the tab associated to the specified |handle|.
  void GetPageType(const IPC::Message& message, int handle);

  // Simulates an action on the SSL blocking page at the tab specified by
  // |handle|. If |proceed| is true, it is equivalent to the user pressing the
  // 'Proceed' button, if false the 'Get me out of there button'.
  // Not that this fails if the tab is not displaying a SSL blocking page.
  void ActionOnSSLBlockingPage(const IPC::Message& message,
                               int handle,
                               bool proceed);

  // Brings the browser window to the front and activates it.
  void BringBrowserToFront(const IPC::Message& message, int browser_handle);

  // Checks to see if a command on the browser's CommandController is enabled.
  void IsPageMenuCommandEnabled(const IPC::Message& message,
                                int browser_handle,
                                int message_num);

  // Prints the current tab immediately.
  void PrintNow(const IPC::Message& message, int tab_handle);

  // Save the current web page.
  void SavePage(const IPC::Message& message,
                int tab_handle,
                const std::wstring& file_name,
                const std::wstring& dir_path,
                int type);

  // Retrieves the visible text from the autocomplete edit.
  void GetAutocompleteEditText(const IPC::Message& message,
                               int autocomplete_edit_handle);

  // Sets the visible text from the autocomplete edit.
  void SetAutocompleteEditText(const IPC::Message& message,
                               int autocomplete_edit_handle,
                               const std::wstring& text);

  // Retrieves if a query to an autocomplete provider is in progress.
  void AutocompleteEditIsQueryInProgress(const IPC::Message& message,
                                         int autocomplete_edit_handle);

  // Retrieves the individual autocomplete matches displayed by the popup.
  void AutocompleteEditGetMatches(const IPC::Message& message,
                                  int autocomplete_edit_handle);

  // Handler for a message sent by the automation client.
  void OnMessageFromExternalHost(int handle, const std::string& target,
                                 const std::string& message);

  // Retrieves the number of SSL related info-bars currently showing in |count|.
  void GetSSLInfoBarCount(const IPC::Message& message, int handle);

  // Causes a click on the link of the info-bar at |info_bar_index|.  If
  // |wait_for_navigation| is true, it sends the reply after a navigation has
  // occurred.
  void ClickSSLInfoBarLink(const IPC::Message& message,
                           int handle,
                           int info_bar_index,
                           bool wait_for_navigation);

  // Retrieves the last time a navigation occurred for the tab.
  void GetLastNavigationTime(const IPC::Message& message, int handle);

  // Waits for a new navigation in the tab if none has happened since
  // |last_navigation_time|.
  void WaitForNavigation(const IPC::Message& message,
                         int handle,
                         int64 last_navigation_time);

  // Sets the int value for preference with name |name|.
  void SetIntPreference(const IPC::Message& message,
                        int handle,
                        const std::wstring& name,
                        int value);

  // Sets the string value for preference with name |name|.
  void SetStringPreference(const IPC::Message& message,
                           int handle,
                           const std::wstring& name,
                           const std::wstring& value);

  // Gets the bool value for preference with name |name|.
  void GetBooleanPreference(const IPC::Message& message,
                            int handle,
                            const std::wstring& name);

  // Sets the bool value for preference with name |name|.
  void SetBooleanPreference(const IPC::Message& message,
                            int handle,
                            const std::wstring& name,
                            bool value);

  // Gets the current used encoding name of the page in the specified tab.
  void GetPageCurrentEncoding(const IPC::Message& message, int tab_handle);

  // Uses the specified encoding to override the encoding of the page in the
  // specified tab.
  void OverrideEncoding(const IPC::Message& message,
                        int tab_handle,
                        const std::wstring& encoding_name);

  void SavePackageShouldPromptUser(const IPC::Message& message,
                                   bool should_prompt);

  // Convert a tab handle into a WebContents. If |tab| is non-NULL a pointer
  // to the tab is also returned. Returns NULL in case of failure or if the tab
  // is not of the WebContents type.
  WebContents* GetWebContentsForHandle(int handle, NavigationController** tab);

  // Callback for history redirect queries.
  virtual void OnRedirectQueryComplete(
      HistoryService::Handle request_handle,
      GURL from_url,
      bool success,
      HistoryService::RedirectList* redirects);

  typedef ObserverList<NotificationObserver> NotificationObserverList;
  typedef std::map<NavigationController*, LoginHandler*> LoginHandlerMap;

  scoped_ptr<IPC::ChannelProxy> channel_;
  scoped_ptr<NotificationObserver> initial_load_observer_;
  scoped_ptr<NotificationObserver> new_tab_ui_load_observer_;
  scoped_ptr<NotificationObserver> find_in_page_observer_;
  scoped_ptr<NotificationObserver> dom_operation_observer_;
  scoped_ptr<NotificationObserver> dom_inspector_observer_;
  scoped_ptr<AutomationTabTracker> tab_tracker_;
  scoped_ptr<AutomationConstrainedWindowTracker> cwindow_tracker_;
  scoped_ptr<AutomationWindowTracker> window_tracker_;
  scoped_ptr<AutomationBrowserTracker> browser_tracker_;
  scoped_ptr<AutomationAutocompleteEditTracker> autocomplete_edit_tracker_;
  scoped_ptr<NavigationControllerRestoredObserver> restore_tracker_;
  LoginHandlerMap login_handler_map_;
  NotificationObserverList notification_observer_list_;

  // Handle for an in-process redirect query. We expect only one redirect query
  // at a time (we should have only one caller, and it will block while waiting
  // for the results) so there is only one handle. When non-0, indicates a
  // query in progress. The routing ID will be set when the query is valid so
  // we know where to send the response.
  HistoryService::Handle redirect_query_;
  int redirect_query_routing_id_;

  // routing id for inspect element request so that we can send back the
  // response later
  int inspect_element_routing_id_;

  // Consumer for asynchronous history queries.
  CancelableRequestConsumer consumer_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(AutomationProvider);
};

// When life started, the AutomationProvider class was a singleton and was meant
// only for UI tests. It had specific behavior (like for example, when the
// channel was shut down. it closed all open Browsers). The new
// AutomationProvider serves other purposes than just UI testing. This class is
// meant to provide the OLD functionality for backward compatibility
class TestingAutomationProvider : public AutomationProvider,
                                  public BrowserList::Observer,
                                  public NotificationObserver {
 public:
  explicit TestingAutomationProvider(Profile* profile);
  virtual ~TestingAutomationProvider();

  // BrowserList::Observer implementation
  // Called immediately after a browser is added to the list
  virtual void OnBrowserAdded(const Browser* browser) {
  }
  // Called immediately before a browser is removed from the list
  virtual void OnBrowserRemoving(const Browser* browser);

  // IPC implementations
  virtual void OnChannelError();

 private:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  void OnRemoveProvider();  // Called via PostTask
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_PROVIDER_H_
