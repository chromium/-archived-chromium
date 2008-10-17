// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_CONTENTS_H_
#define CHROME_BROWSER_WEB_CONTENTS_H_

#include "base/hash_tables.h"
#include "chrome/browser/download/save_package.h"
#include "chrome/browser/fav_icon_helper.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/render_view_host_delegate.h"
#include "chrome/browser/render_view_host_manager.h"
#include "chrome/browser/shell_dialogs.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/web_app.h"

class FindInPageController;
class InterstitialPageDelegate;
class PasswordManager;
class PluginInstaller;
class RenderViewHost;
class RenderViewHostFactory;
class RenderWidgetHost;
class SadTabView;
class WebContentsView;

// WebContents represents the contents of a tab that shows web pages. It embeds
// a RenderViewHost (via RenderViewHostManager) to actually display the page.
class WebContents : public TabContents,
                    public RenderViewHostDelegate,
                    public RenderViewHostManager::Delegate,
                    public SelectFileDialog::Listener,
                    public NotificationObserver,
                    public WebApp::Observer {
 public:
  // If instance is NULL, then creates a new process for this view.  Otherwise
  // initialize with a process already created for a different WebContents.
  // This will share the process between views in the same instance.  If
  // render_view_factory is NULL, this will create RenderViewHost objects
  // directly.
  WebContents(Profile* profile,
              SiteInstance* instance,
              RenderViewHostFactory* render_view_factory,
              int routing_id,
              HANDLE modal_dialog_event);

  static void RegisterUserPrefs(PrefService* prefs);

  // Getters -------------------------------------------------------------------

  // Returns the PasswordManager, creating it if necessary.
  PasswordManager* GetPasswordManager();

  // Returns the PluginInstaller, creating it if necessary.
  PluginInstaller* GetPluginInstaller();

  // Returns the SavePackage which manages the page saving job. May be NULL.
  SavePackage* save_package() const { return save_package_.get(); }

  // Return the currently active RenderProcessHost and RenderViewHost. Each of
  // these may change over time.
  RenderProcessHost* process() const {
    return render_manager_.current_host()->process();
  }
  RenderViewHost* render_view_host() const {
    return render_manager_.current_host();
  }

  // The WebContentsView will never change and is guaranteed non-NULL.
  WebContentsView* view() const {
    return view_.get();
  }

  bool is_starred() const { return is_starred_; }

  // TabContents (public overrides) --------------------------------------------

  virtual void Destroy();
  virtual WebContents* AsWebContents() { return this; }
  virtual SiteInstance* GetSiteInstance() const;
  virtual SkBitmap GetFavIcon();
  virtual std::wstring GetStatusText() const;
  virtual bool NavigateToPendingEntry(bool reload);
  virtual void Stop();
  virtual void StartFinding(int request_id,
                            const std::wstring& search_string,
                            bool forward,
                            bool match_case,
                            bool find_next);
  virtual void StopFinding(bool clear_selection);
  virtual void Cut();
  virtual void Copy();
  virtual void Paste();
  virtual void DidBecomeSelected();
  virtual void WasHidden();
  virtual void ShowContents();
  virtual void HideContents();
  virtual void SizeContents(const gfx::Size& size);
  virtual void SetDownloadShelfVisible(bool visible);

  // Retarded pass-throughs to the view.
  // TODO(brettw) fix this, tab contents shouldn't have these methods, probably
  // it should be killed altogether.
  virtual void CreateView(HWND parent_hwnd, const gfx::Rect& initial_bounds);
  virtual HWND GetContainerHWND() const;
  virtual HWND GetContentHWND();
  virtual void GetContainerBounds(gfx::Rect *out) const;

  // Find in page --------------------------------------------------------------

  // TODO(brettw) these should be commented.
  void OpenFindInPageWindow(const Browser& browser);
  void ReparentFindWindow(HWND new_parent);
  bool AdvanceFindSelection(bool forward_direction);
  bool IsFindWindowFullyVisible();
  bool GetFindInPageWindowLocation(int* x, int* y);
  void SetFindInPageVisible(bool visible);

  // Web apps ------------------------------------------------------------------

  // Sets the WebApp for this WebContents.
  void SetWebApp(WebApp* web_app);
  WebApp* web_app() { return web_app_.get(); }

  // Return whether this tab contents was created to contain an application.
  bool IsWebApplication() const;

  // Tell Gears to create a shortcut for the current page.
  void CreateShortcut();

  // Interstitials -------------------------------------------------------------

  // Various other systems need to know about our interstitials.
  bool showing_interstitial_page() const {
    return render_manager_.showing_interstitial_page();
  }
  bool showing_repost_interstitial() const {
    return render_manager_.showing_repost_interstitial();
  }

  // Displays the specified interstitial page. This method can be used to show
  // temporary pages (such as security error pages).  It can be hidden by
  // calling HideInterstitialPage, in which case the original page is restored.
  // |interstitial_page| is not owned by the WebContents.
  void ShowInterstitialPage(InterstitialPage* interstitial_page) {
    render_manager_.ShowInterstitialPage(interstitial_page);
  }

  // Reverts from the interstitial page to the original page.
  // If |wait_for_navigation| is true, the interstitial page is removed when
  // the original page has transitioned to the new contents.  This is useful
  // when you want to hide the interstitial page as you navigate to a new page.
  // Hiding the interstitial page right away would show the previous displayed
  // page.  If |proceed| is true, the WebContents will expect the navigation
  // to complete.  If not, it will revert to the last shown page.
  void HideInterstitialPage(bool wait_for_navigation, bool proceed) {
    render_manager_.HideInterstitialPage(wait_for_navigation, proceed);
  }

  // Returns the interstitial page currently shown if any, NULL otherwise.
  InterstitialPage* interstitial_page() const {
    return render_manager_.interstitial_page();
  }

  // Misc state & callbacks ----------------------------------------------------

  // Set whether the contents should block javascript message boxes or not.
  // Default is not to block any message boxes.
  void set_suppress_javascript_messages(bool suppress_javascript_messages) {
    suppress_javascript_messages_ = suppress_javascript_messages;
  }

  // JavascriptMessageBoxHandler calls this when the dialog is closed.
  void OnJavaScriptMessageBoxClosed(IPC::Message* reply_msg,
                                    bool success,
                                    const std::wstring& prompt);

  // Prepare for saving page.
  void OnSavePage();

  // Save page with the main HTML file path, the directory for saving resources,
  // and the save type: HTML only or complete web page.
  void SavePage(const std::wstring& main_file, const std::wstring& dir_path,
                SavePackage::SavePackageType save_type);

  // Displays asynchronously a print preview (generated by the renderer) if not
  // already displayed and ask the user for its preferred print settings with
  // the "Print..." dialog box. (managed by the print worker thread).
  // TODO(maruel):  Creates a snapshot of the renderer to be used for the new
  // tab for the printing facility.
  void PrintPreview();

  // Prints the current document immediately. Since the rendering is
  // asynchronous, the actual printing will not be completed on the return of
  // this function. Returns false if printing is impossible at the moment.
  bool PrintNow();

  // Returns true if the active NavigationEntry's page_id equals page_id.
  bool IsActiveEntry(int32 page_id);

  const std::string& contents_mime_type() const {
    return contents_mime_type_;
  }

  // Returns true if this WebContents will notify about disconnection.
  bool notify_disconnection() const { return notify_disconnection_; }

 protected:
  // Should be deleted via CloseContents.
  virtual ~WebContents();

  RenderWidgetHostView* render_widget_host_view() const {
    return render_manager_.current_view();
  }

  // TabContents (private overrides) -------------------------------------------

  virtual void SetInitialFocus(bool reverse);
  virtual void SetIsLoading(bool is_loading, LoadNotificationDetails* details);

  // RenderViewHostDelegate ----------------------------------------------------

  virtual RenderViewHostDelegate::View* GetViewDelegate() const;
  virtual RenderViewHostDelegate::FindInPage* GetFindInPageDelegate() const;
  virtual RenderViewHostDelegate::Save* GetSaveDelegate() const;
  virtual Profile* GetProfile() const;
  virtual void RendererReady(RenderViewHost* render_view_host);
  virtual void RendererGone(RenderViewHost* render_view_host);
  virtual void DidNavigate(RenderViewHost* render_view_host,
                           const ViewHostMsg_FrameNavigate_Params& params);
  virtual void UpdateState(RenderViewHost* render_view_host,
                           int32 page_id,
                           const GURL& url,
                           const std::wstring& title,
                           const std::string& state);
  virtual void UpdateTitle(RenderViewHost* render_view_host,
                           int32 page_id,
                           const std::wstring& title);
  virtual void UpdateEncoding(RenderViewHost* render_view_host,
                              const std::wstring& encoding_name);
  virtual void UpdateTargetURL(int32 page_id, const GURL& url);
  virtual void UpdateThumbnail(const GURL& url,
                               const SkBitmap& bitmap,
                               const ThumbnailScore& score);
  virtual void Close(RenderViewHost* render_view_host);
  virtual void RequestMove(const gfx::Rect& new_bounds);
  virtual void DidStartLoading(RenderViewHost* render_view_host, int32 page_id);
  virtual void DidStopLoading(RenderViewHost* render_view_host, int32 page_id);
  virtual void DidStartProvisionalLoadForFrame(RenderViewHost* render_view_host,
                                               bool is_main_frame,
                                               const GURL& url);
  virtual void DidRedirectProvisionalLoad(int32 page_id,
                                          const GURL& source_url,
                                          const GURL& target_url);
  virtual void DidLoadResourceFromMemoryCache(const GURL& url,
                                              const std::string& security_info);
  virtual void DidFailProvisionalLoadWithError(
      RenderViewHost* render_view_host,
      bool is_main_frame,
      int error_code,
      const GURL& url,
      bool showing_repost_interstitial);
  virtual void UpdateFavIconURL(RenderViewHost* render_view_host,
                                int32 page_id, const GURL& icon_url);
  virtual void DidDownloadImage(RenderViewHost* render_view_host,
                                int id,
                                const GURL& image_url,
                                bool errored,
                                const SkBitmap& image);
  virtual void ShowContextMenu(const ViewHostMsg_ContextMenu_Params& params);
  virtual void StartDragging(const WebDropData& drop_data);
  virtual void UpdateDragCursor(bool is_drop_target);
  virtual void RequestOpenURL(const GURL& url,
                              WindowOpenDisposition disposition);
  virtual void DomOperationResponse(const std::string& json_string,
                                    int automation_id);
  virtual void ProcessExternalHostMessage(const std::string& receiver,
                                          const std::string& message);
  virtual void GoToEntryAtOffset(int offset);
  virtual void GetHistoryListCount(int* back_list_count,
                                   int* forward_list_count);
  virtual void RunFileChooser(const std::wstring& default_file);
  virtual void RunJavaScriptMessage(const std::wstring& message,
                                    const std::wstring& default_prompt,
                                    const int flags,
                                    IPC::Message* reply_msg);
  virtual void RunBeforeUnloadConfirm(const std::wstring& message,
                                      IPC::Message* reply_msg);
  virtual void ShowModalHTMLDialog(const GURL& url, int width, int height,
                                   const std::string& json_arguments,
                                   IPC::Message* reply_msg);
  virtual void PasswordFormsSeen(const std::vector<PasswordForm>& forms);
  virtual void TakeFocus(bool reverse);
  virtual void PageHasOSDD(RenderViewHost* render_view_host,
                           int32 page_id, const GURL& url, bool autodetected);
  virtual void InspectElementReply(int num_resources);
  virtual void DidGetPrintedPagesCount(int cookie, int number_pages);
  virtual void DidPrintPage(const ViewHostMsg_DidPrintPage_Params& params);
  virtual GURL GetAlternateErrorPageURL() const;
  virtual WebPreferences GetWebkitPrefs();
  virtual void OnMissingPluginStatus(int status);
  virtual void OnCrashedPlugin(const std::wstring& plugin_path);
  virtual void OnJSOutOfMemory();
  virtual void ShouldClosePage(bool proceed) {
    render_manager_.ShouldClosePage(proceed);
  }
  // Allows the WebContents to react when a cross-site response is ready to be
  // delivered to a pending RenderViewHost.  We must first run the onunload
  // handler of the old RenderViewHost before we can allow it to proceed.
  void OnCrossSiteResponse(int new_render_process_host_id,
                           int new_request_id) {
    render_manager_.OnCrossSiteResponse(new_render_process_host_id,
                                        new_request_id);
  }
  virtual bool CanBlur() const;
  virtual void RendererUnresponsive(RenderViewHost* render_view_host);
  virtual void RendererResponsive(RenderViewHost* render_view_host);
  virtual void LoadStateChanged(const GURL& url, net::LoadState load_state);
  virtual void OnDidGetApplicationInfo(
      int32 page_id,
      const webkit_glue::WebApplicationInfo& info);
  virtual void OnEnterOrSpace();

  // Stupid render view host view pass-throughs.
  virtual void HandleKeyboardEvent(const WebKeyboardEvent& event);

  // SelectFileDialog::Listener ------------------------------------------------

  virtual void FileSelected(const std::wstring& path, void* params);
  virtual void FileSelectionCanceled(void* params);

  // RenderViewHostManager::Delegate -------------------------------------------

  virtual void BeforeUnloadFiredFromRenderManager(
      bool proceed,
      bool* proceed_to_fire_unload);
  virtual void DidStartLoadingFromRenderManager(
      RenderViewHost* render_view_host, int32 page_id) {
    DidStartLoading(render_view_host, page_id);
  }
  virtual void RendererGoneFromRenderManager(RenderViewHost* render_view_host) {
    RendererGone(render_view_host);
  }
  virtual void UpdateRenderViewSizeForRenderManager();
  virtual void NotifySwappedFromRenderManager() {
    NotifySwapped();
  }
  virtual NavigationController* GetControllerForRenderManager() {
    return controller();
  }

  // Initializes the given renderer if necessary and creates the view ID
  // corresponding to this view host. If this method is not called and the
  // process is not shared, then the WebContents will act as though the renderer
  // is not running (i.e., it will render "sad tab"). This method is
  // automatically called from LoadURL.
  //
  // If you are attaching to an already-existing RenderView, you should call
  // InitWithExistingID.
  //
  // TODO(brettw) clean this up! This logic seems out of place. This is called
  // by the RenderViewHostManager, but also overridden by the DOMUIHost. Any
  // logic that has to be here should have a more clear name.
  virtual bool CreateRenderViewForRenderManager(
      RenderViewHost* render_view_host);

 private:
  FRIEND_TEST(WebContentsTest, UpdateTitle);
  friend class TestWebContents;

  // Temporary until the view/contents separation is complete.
  friend class WebContentsViewWin;

  // When CreateShortcut is invoked RenderViewHost::GetApplicationInfo is
  // invoked. CreateShortcut caches the state of the page needed to create the
  // shortcut in PendingInstall. When OnDidGetApplicationInfo is invoked, it
  // uses the information from PendingInstall and the WebApplicationInfo
  // to create the shortcut.
  class GearsCreateShortcutCallbackFunctor;
  struct PendingInstall {
    int32 page_id;
    SkBitmap icon;
    std::wstring title;
    GURL url;
    // This object receives the GearsCreateShortcutCallback and routes the
    // message back to the WebContents, if we haven't been deleted.
    GearsCreateShortcutCallbackFunctor* callback_functor;
  };

  // NotificationObserver ------------------------------------------------------

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Navigation helpers --------------------------------------------------------
  //
  // These functions are helpers for Navigate() and DidNavigate().

  // Handles post-navigation tasks in DidNavigate AFTER the entry has been
  // committed to the navigation controller. Note that the navigation entry is
  // not provided since it may be invalid/changed after being committed. The
  // current navigation entry is in the NavigationController at this point.
  void DidNavigateMainFramePostCommit(
      const NavigationController::LoadCommittedDetails& details,
      const ViewHostMsg_FrameNavigate_Params& params);
  void DidNavigateAnyFramePostCommit(
      RenderViewHost* render_view_host,
      const NavigationController::LoadCommittedDetails& details,
      const ViewHostMsg_FrameNavigate_Params& params);

  // Called when navigating the main frame to close all child windows if the
  // domain is changing.
  void MaybeCloseChildWindows(const ViewHostMsg_FrameNavigate_Params& params);

  // Updates the starred state from the bookmark bar model. If the state has
  // changed, the delegate is notified.
  void UpdateStarredStateForCurrentURL();

  // Send the alternate error page URL to the renderer. This method is virtual
  // so special html pages can override this (e.g., the new tab page).
  virtual void UpdateAlternateErrorPageURL();

  // Send webkit specific settings to the renderer.
  void UpdateWebPreferences();

  // Return whether the optional web application is active for the current URL.
  // Call this method to check if web app properties are in effect.
  //
  // Note: This method should be used for presentation but not security. The app
  // is always active if the containing window is a web application.
  bool IsWebApplicationActive() const;

  // WebApp::Observer method. Invoked when the set of images contained in the
  // web app changes. Notifies the delegate our favicon has changed.
  virtual void WebAppImagesChanged(WebApp* web_app);

  // Called when the user dismisses the shortcut creation dialog.  'success' is
  // true if the shortcut was created.
  void OnGearsCreateShortcutDone(const GearsShortcutData& shortcut_data,
                                 bool success);

  // If our controller was restored and the page id is > than the site
  // instance's page id, the site instances page id is updated as well as the
  // renderers max page id.
  void UpdateMaxPageIDIfNecessary(SiteInstance* site_instance,
                                  RenderViewHost* rvh);

  // Called by OnMsgNavigate to update history state. Overridden by subclasses
  // that don't want to be added to history.
  virtual void UpdateHistoryForNavigation(const GURL& display_url,
      const ViewHostMsg_FrameNavigate_Params& params);

  // Misc non-view stuff -------------------------------------------------------

  // Helper functions for sending notifications.
  void NotifySwapped();
  void NotifyConnected();
  void NotifyDisconnected();

  // If params has a searchable form, this tries to create a new keyword.
  void GenerateKeywordIfNecessary(
      const ViewHostMsg_FrameNavigate_Params& params);

  // Data ----------------------------------------------------------------------

  // The corresponding view.
  scoped_ptr<WebContentsView> view_;

  // Manages creation and swapping of render views.
  RenderViewHostManager render_manager_;

  // For testing, passed to new RenderViewHost managers.
  RenderViewHostFactory* render_view_factory_;

  // Handles print preview and print job for this contents.
  printing::PrintViewManager printing_;

  // Indicates whether we should notify about disconnection of this
  // WebContents. This is used to ensure disconnection notifications only
  // happen if a connection notification has happened and that they happen only
  // once.
  bool notify_disconnection_;

  // When a navigation occurs (and is committed), we record its URL. This lets
  // us see where we are navigating from.
  GURL last_url_;

  // Maps from handle to page_id.
  typedef std::map<HistoryService::Handle, int32> HistoryRequestMap;
  HistoryRequestMap history_requests_;

  // System time at which the current load was started.
  TimeTicks current_load_start_;

  // Whether we have a (non-empty) title for the current page.
  // Used to prevent subsequent title updates from affecting history.
  bool has_page_title_;

  // SavePackage, lazily created.
  scoped_refptr<SavePackage> save_package_;

  // Handles communication with the FindInPage popup.
  scoped_ptr<FindInPageController> find_in_page_controller_;

  // Tracks our pending CancelableRequests. This maps pending requests to
  // page IDs so that we know whether a given callback still applies. The
  // page ID -1 means no page ID was set.
  CancelableRequestConsumerT<int32, -1> cancelable_consumer_;

  // Whether the current URL is starred
  bool is_starred_;

  // Handle to an event that's set when the page is showing a message box (or
  // equivalent constrained window).  Plugin processes check this to know if
  // they should pump messages then.
  ScopedHandle message_box_active_;

  // PasswordManager, lazily created.
  scoped_ptr<PasswordManager> password_manager_;

  // PluginInstaller, lazily created.
  scoped_ptr<PluginInstaller> plugin_installer_;

  // The SadTab renderer.
  // TODO(brettw) move into WebContentsView*.
  scoped_ptr<SadTabView> sad_tab_;

  // Handles downloading favicons.
  FavIconHelper fav_icon_helper_;

  // Dialog box used for choosing files to upload from file form fields.
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  // The time that the last javascript message was dismissed.
  TimeTicks last_javascript_message_dismissal_;

  // True if the user has decided to block future javascript messages. This is
  // reset on navigations to false on navigations.
  bool suppress_javascript_messages_;

  // When a navigation occurs, we record its contents MIME type. It can be
  // used to check whether we can do something for some special contents.
  std::string contents_mime_type_;

  PendingInstall pending_install_;

  // The last time that the download shelf was made visible.
  TimeTicks last_download_shelf_show_;

  // The current load state and the URL associated with it.
  net::LoadState load_state_;
  std::wstring load_state_host_;

  // Non-null if we're displaying content for a web app.
  scoped_refptr<WebApp> web_app_;

  DISALLOW_COPY_AND_ASSIGN(WebContents);
};

#endif  // CHROME_BROWSER_WEB_CONTENTS_H_
