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

#include "base/basictypes.h"
#include "chrome/browser/automation/automation_browser_tracker.h"
#include "chrome/browser/automation/automation_tab_tracker.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/ipc_sync_channel.h"
#include "chrome/common/notification_observer.h"
#include "chrome/test/automation/automation_messages.h"
#include "chrome/views/event.h"
#include "webkit/glue/find_in_page_request.h"

#if defined(OS_WIN)
// TODO(port): enable these.
#include "chrome/browser/automation/automation_autocomplete_edit_tracker.h"
#include "chrome/browser/automation/automation_constrained_window_tracker.h"
#include "chrome/browser/automation/automation_window_tracker.h"
enum AutomationMsg_NavigationResponseValues;
#endif

class LoginHandler;
class NavigationControllerRestoredObserver;
class ExternalTabContainer;
struct AutocompleteMatchData;

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
  // The template argument NavigationCodeType facilitate the creation of the
  // approriate NavigationNotificationObserver instance, which subscribes to
  // the events published by the NotificationService and sends out a response
  // to the IPC message.
  template<class NavigationCodeType>
  NotificationObserver* AddNavigationStatusListener(
      NavigationController* tab, IPC::Message* reply_message,
      NavigationCodeType success_code,
      NavigationCodeType auth_needed_code,
      NavigationCodeType failed_code);

  void RemoveNavigationStatusListener(NotificationObserver* obs);

  // Add an observer for the TabStrip. Currently only Tab append is observed. A
  // navigation listener is created on successful notification of tab append. A
  // pointer to the added navigation observer is returned. This object should
  // NOT be deleted and should be released by calling the corresponding
  // RemoveTabStripObserver method.
  NotificationObserver* AddTabStripObserver(Browser* parent,
                                            int32 routing_id,
                                            IPC::Message* reply_message);
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

  IPC::Message* reply_message_release() {
    IPC::Message* reply_message = reply_message_;
    reply_message_ = NULL;
    return reply_message;
  }

 private:
  // IPC Message callbacks.
  void CloseBrowser(int handle, IPC::Message* reply_message);
  void CloseBrowserAsync(int browser_handle);
  void ActivateTab(int handle, int at_index, int* status);
  void AppendTab(int handle, const GURL& url, IPC::Message* reply_message);
  void CloseTab(int tab_handle, bool wait_until_closed,
                IPC::Message* reply_message);

  void GetActiveTabIndex(int handle, int* active_tab_index);
  void GetCookies(const GURL& url, int handle, int* value_size,
                  std::string* value);
  void SetCookie(const GURL& url,
                 const std::string value,
                 int handle,
                 int* response_value);
  void GetBrowserWindowCount(int* window_count);
  void GetShowingAppModalDialog(bool* showing_dialog, int* dialog_button);
  void ClickAppModalDialogButton(int button, bool* success);
  void GetBrowserWindow(int index, int* handle);
  void GetLastActiveBrowserWindow(int* handle);
  void GetActiveWindow(int* handle);
#if defined(OS_WIN)
  // TODO(port): Replace HWND.
  void GetWindowHWND(int handle, HWND* win32_handle);
#endif  // defined(OS_WIN)
  void ExecuteBrowserCommand(int handle, int command, bool* success);
  void ExecuteBrowserCommandWithNotification(int handle, int command,
                                             IPC::Message* reply_message);
  void WindowGetViewBounds(int handle, int view_id, bool screen_coordinates,
                           bool* success, gfx::Rect* bounds);
#if defined(OS_WIN)
  // TODO(port): Replace POINT.
  void WindowSimulateDrag(int handle,
                          std::vector<POINT> drag_path,
                          int flags,
                          bool press_escape_en_route,
                          IPC::Message* reply_message);
  void WindowSimulateClick(const IPC::Message& message,
                          int handle,
                          POINT click,
                          int flags);
#endif  // defined(OS_WIN)
  void WindowSimulateKeyPress(const IPC::Message& message,
                              int handle,
                              wchar_t key,
                              int flags);
  void SetWindowVisible(int handle, bool visible, bool* result);
  void IsWindowActive(int handle, bool* success, bool* is_active);
  void ActivateWindow(int handle);

  void GetTabCount(int handle, int* tab_count);
  void GetTab(int win_handle, int tab_index, int* tab_handle);
#if defined(OS_WIN)
  // TODO(port): Replace HWND.
  void GetTabHWND(int handle, HWND* tab_hwnd);
#endif  // defined(OS_WIN)
  void GetTabProcessID(int handle, int* process_id);
  void GetTabTitle(int handle, int* title_string_size, std::wstring* title);
  void GetTabURL(int handle, bool* success, GURL* url);
  void HandleUnused(const IPC::Message& message, int handle);
  void NavigateToURL(int handle, const GURL& url, IPC::Message* reply_message);
  void NavigationAsync(int handle, const GURL& url, bool* status);
  void GoBack(int handle, IPC::Message* reply_message);
  void GoForward(int handle, IPC::Message* reply_message);
  void Reload(int handle, IPC::Message* reply_message);
  void SetAuth(int tab_handle, const std::wstring& username,
               const std::wstring& password, IPC::Message* reply_message);
  void CancelAuth(int tab_handle, IPC::Message* reply_message);
  void NeedsAuth(int tab_handle, bool* needs_auth);
  void GetRedirectsFrom(int tab_handle,
                        const GURL& source_url,
                        IPC::Message* reply_message);
  void ExecuteJavascript(int handle,
                         const std::wstring& frame_xpath,
                         const std::wstring& script,
                         IPC::Message* reply_message);
  void GetShelfVisibility(int handle, bool* visible);
  void SetFilteredInet(const IPC::Message& message, bool enabled);

#if defined(OS_WIN)
  // TODO(port): Replace POINT.
  void ScheduleMouseEvent(views::View* view,
                          views::Event::EventType type,
                          POINT point,
                          int flags);
#endif  // defined(OS_WIN)
  void GetFocusedViewID(int handle, int* view_id);

  // Helper function to find the browser window that contains a given
  // NavigationController and activate that tab.
  // Returns the Browser if found.
  Browser* FindAndActivateTab(NavigationController* contents);

  // Apply an accelerator with id (like IDC_BACK, IDC_FORWARD ...)
  // to the Browser with given handle.
  void ApplyAccelerator(int handle, int id);

  void GetConstrainedWindowCount(int handle, int* count);
  void GetConstrainedWindow(int handle, int index, int* cwindow_handle);

  void GetConstrainedTitle(int handle, int* title,
                           std::wstring* title_string_size);

  void GetConstrainedWindowBounds(int handle, bool* exists,
                                  gfx::Rect* rect);

  // This function has been deprecated, please use HandleFindRequest.
  void HandleFindInPageRequest(int handle,
                               const std::wstring& find_request,
                               int forward,
                               int match_case,
                               int* active_ordinal,
                               int* matches_found);

  // Responds to the FindInPage request, retrieves the search query parameters,
  // launches an observer to listen for results and issues a StartFind request.
  void HandleFindRequest(int handle,
                         const FindInPageRequest& request,
                         IPC::Message* reply_message);

  // Responds to requests to open the FindInPage window.
  void HandleOpenFindInPageRequest(const IPC::Message& message,
                                   int handle);

  // Get the visibility state of the Find window.
  void GetFindWindowVisibility(int handle, bool* visible);

  // Responds to requests to find the location of the Find window.
  void HandleFindWindowLocationRequest(int handle, int* x, int* y);

  // Get the visibility state of the Bookmark bar.
  void GetBookmarkBarVisibility(int handle, bool* visible, bool* animating);

  // Responds to InspectElement request
  void HandleInspectElementRequest(int handle,
                                   int x,
                                   int y,
                                   IPC::Message* reply_message);

  void GetDownloadDirectory(int handle,
                            std::wstring* download_directory);

  // Retrieves a Browser from a Window and vice-versa.
  void GetWindowForBrowser(int window_handle, bool* success, int* handle);
  void GetBrowserForWindow(int window_handle, bool* success,
                           int* browser_handle);

  void GetAutocompleteEditForBrowser(int browser_handle, bool* success,
                                     int* autocomplete_edit_handle);

  void OpenNewBrowserWindow(int show_command);

  void ShowInterstitialPage(int tab_handle,
                            const std::string& html_text,
                            IPC::Message* reply_message);
  void HideInterstitialPage(int tab_handle, bool* success);

#if defined(OS_WIN)
  // TODO(port): Re-enable.
  void CreateExternalTab(HWND parent, const gfx::Rect& dimensions,
                         unsigned int style, HWND* tab_container_window,
                         int* tab_handle);
  void NavigateInExternalTab(
      int handle, const GURL& url,
      AutomationMsg_NavigationResponseValues* status);
  // The container of an externally hosted tab calls this to reflect any
  // accelerator keys that it did not process. This gives the tab a chance
  // to handle the keys
  void ProcessUnhandledAccelerator(const IPC::Message& message, int handle,
                                   const MSG& msg);

  void SetInitialFocus(const IPC::Message& message, int handle, bool reverse);

  // See comment in AutomationMsg_WaitForTabToBeRestored.
  void WaitForTabToBeRestored(int tab_handle, IPC::Message* reply_message);

  // This sets the keyboard accelerators to be used by an externally
  // hosted tab. This call is not valid on a regular tab hosted within
  // Chrome.
  void SetAcceleratorsForTab(int handle, HACCEL accel_table,
                             int accel_entry_count, bool* status);

  void OnTabReposition(int tab_handle,
                       const IPC::Reposition_Params& params);
#endif  // defined(OS_WIN)

  // Gets the security state for the tab associated to the specified |handle|.
  void GetSecurityState(int handle, bool* success,
                        SecurityStyle* security_style, int* ssl_cert_status,
                        int* mixed_content_status);

  // Gets the page type for the tab associated to the specified |handle|.
  void GetPageType(int handle, bool* success,
                   NavigationEntry::PageType* page_type);

  // Simulates an action on the SSL blocking page at the tab specified by
  // |handle|. If |proceed| is true, it is equivalent to the user pressing the
  // 'Proceed' button, if false the 'Get me out of there button'.
  // Not that this fails if the tab is not displaying a SSL blocking page.
  void ActionOnSSLBlockingPage(int handle,
                               bool proceed,
                               IPC::Message* reply_message);

  // Brings the browser window to the front and activates it.
  void BringBrowserToFront(int browser_handle, bool* success);

  // Checks to see if a command on the browser's CommandController is enabled.
  void IsPageMenuCommandEnabled(int browser_handle,
                                int message_num,
                                bool* menu_item_enabled);

  // Prints the current tab immediately.
  void PrintNow(int tab_handle, IPC::Message* reply_message);

  // Save the current web page.
  void SavePage(int tab_handle,
                const std::wstring& file_name,
                const std::wstring& dir_path,
                int type,
                bool* success);

  // Retrieves the visible text from the autocomplete edit.
  void GetAutocompleteEditText(int autocomplete_edit_handle,
                               bool* success, std::wstring* text);

  // Sets the visible text from the autocomplete edit.
  void SetAutocompleteEditText(int autocomplete_edit_handle,
                               const std::wstring& text,
                               bool* success);

  // Retrieves if a query to an autocomplete provider is in progress.
  void AutocompleteEditIsQueryInProgress(int autocomplete_edit_handle,
                                         bool* success,
                                         bool* query_in_progress);

  // Retrieves the individual autocomplete matches displayed by the popup.
  void AutocompleteEditGetMatches(int autocomplete_edit_handle,
                                  bool* success,
                                  std::vector<AutocompleteMatchData>* matches);

  // Handler for a message sent by the automation client.
  void OnMessageFromExternalHost(int handle, const std::string& message,
                                 const std::string& origin,
                                 const std::string& target);

  // Retrieves the number of SSL related info-bars currently showing in |count|.
  void GetSSLInfoBarCount(int handle, int* count);

  // Causes a click on the link of the info-bar at |info_bar_index|.  If
  // |wait_for_navigation| is true, it sends the reply after a navigation has
  // occurred.
  void ClickSSLInfoBarLink(int handle, int info_bar_index,
                           bool wait_for_navigation,
                           IPC::Message* reply_message);

  // Retrieves the last time a navigation occurred for the tab.
  void GetLastNavigationTime(int handle, int64* last_navigation_time);

  // Waits for a new navigation in the tab if none has happened since
  // |last_navigation_time|.
  void WaitForNavigation(int handle,
                         int64 last_navigation_time,
                         IPC::Message* reply_message);

  // Sets the int value for preference with name |name|.
  void SetIntPreference(int handle,
                        const std::wstring& name,
                        int value,
                        bool* success);

  // Sets the string value for preference with name |name|.
  void SetStringPreference(int handle,
                           const std::wstring& name,
                           const std::wstring& value,
                           bool* success);

  // Gets the bool value for preference with name |name|.
  void GetBooleanPreference(int handle,
                            const std::wstring& name,
                            bool* success,
                            bool* value);

  // Sets the bool value for preference with name |name|.
  void SetBooleanPreference(int handle,
                            const std::wstring& name,
                            bool value,
                            bool* success);

  // Gets the current used encoding name of the page in the specified tab.
  void GetPageCurrentEncoding(int tab_handle, std::wstring* current_encoding);

  // Uses the specified encoding to override the encoding of the page in the
  // specified tab.
  void OverrideEncoding(int tab_handle,
                        const std::wstring& encoding_name,
                        bool* success);

  void SavePackageShouldPromptUser(bool should_prompt);

  // Convert a tab handle into a WebContents. If |tab| is non-NULL a pointer
  // to the tab is also returned. Returns NULL in case of failure or if the tab
  // is not of the WebContents type.
  WebContents* GetWebContentsForHandle(int handle, NavigationController** tab);

  ExternalTabContainer* GetExternalTabForHandle(int handle);

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
  scoped_ptr<AutomationBrowserTracker> browser_tracker_;
  scoped_ptr<AutomationTabTracker> tab_tracker_;
#if defined(OS_WIN)
  // TODO(port): Enable as trackers get ported.
  scoped_ptr<AutomationConstrainedWindowTracker> cwindow_tracker_;
  scoped_ptr<AutomationWindowTracker> window_tracker_;
  scoped_ptr<AutomationAutocompleteEditTracker> autocomplete_edit_tracker_;
#endif
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

  IPC::Message* reply_message_;

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
