// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_BROWSER_WEB_CONTENTS_H_
#define CHROME_BROWSER_WEB_CONTENTS_H_

#include <hash_map>

#include "chrome/browser/fav_icon_helper.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/render_view_host_delegate.h"
#include "chrome/browser/render_view_host_manager.h"
#include "chrome/browser/save_package.h"
#include "chrome/browser/shell_dialogs.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/web_app.h"
#include "chrome/views/hwnd_view_container.h"

class FindInPageController;
class InterstitialPageDelegate;
class NavigationProfiler;
class PasswordManager;
class PluginInstaller;
class RenderViewHost;
class RenderViewHostFactory;
class RenderWidgetHost;
class RenderWidgetHostHWND;
class SadTabView;
struct WebDropData;
class WebDropTarget;

class WebContents : public TabContents,
                    public RenderViewHostDelegate,
                    public RenderViewHostManager::Delegate,
                    public ChromeViews::HWNDViewContainer,
                    public SelectFileDialog::Listener,
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

  virtual void CreateView(HWND parent_hwnd, const gfx::Rect& initial_bounds);
  virtual HWND GetContainerHWND() const { return GetHWND(); }
  virtual void GetContainerBounds(gfx::Rect *out) const;
  virtual void ShowContents();
  virtual void HideContents();
  virtual void SizeContents(const gfx::Size& size);

  // Causes the renderer to invoke the onbeforeunload event handler.  The
  // result will be returned via ViewMsg_ShouldClose.
  virtual void FirePageBeforeUnload();

  // Close the page after the page has responded that it can be closed via
  // ViewMsg_ShouldClose. This is where the page itself is closed. The
  // unload handler is triggered here, which can block with a dialog, but cannot
  // cancel the close of the page.
  virtual void FirePageUnload();


  // TabContents
  virtual WebContents* AsWebContents() { return this; }
  virtual bool Navigate(const NavigationEntry& entry, bool reload);
  virtual void Stop();
  virtual void DidBecomeSelected();
  virtual void WasHidden();
  virtual void Destroy();
  virtual SkBitmap GetFavIcon();
  virtual std::wstring GetStatusText() const;

  // Find functions
  virtual void StartFinding(int request_id,
                            const std::wstring& search_string,
                            bool forward,
                            bool match_case,
                            bool find_next);
  virtual void StopFinding(bool clear_selection);
  virtual void OpenFindInPageWindow(const Browser& browser);
  virtual void ReparentFindWindow(HWND new_parent);
  virtual bool AdvanceFindSelection(bool forward_direction);

  // Text zoom
  virtual void AlterTextSize(text_zoom::TextSize size);

  // Change encoding of page.
  virtual void SetPageEncoding(const std::wstring& encoding_name);

  bool is_starred() const { return is_starred_; }

  // Set whether the contents should block javascript message boxes or not.
  // Default is not to block any message boxes.
  void SetSuppressJavascriptMessageBoxes(bool suppress_javascript_messages);

  // Return true if the WebContents is doing performance profiling
  bool is_profiling() const { return is_profiling_; }

  // Various other systems need to know about our interstitials.
  bool showing_interstitial_page() const {
    return render_manager_.showing_interstitial_page();
  }
  bool showing_repost_interstitial() const {
    return render_manager_.showing_repost_interstitial();
  }

  // Check with the global navigation profiler on whether to enable
  // profiling. Return true if profiling needs to be enabled, return
  // false otherwise.
  bool EnableProfiling();

  // Return the global navigation profiler.
  NavigationProfiler* GetNavigationProfiler();

  // Overridden from TabContents to remember at what time the download bar was
  // shown.
  void SetDownloadShelfVisible(bool visible);

  // Returns the SavePackage which manages the page saving job.
  SavePackage* get_save_package() const { return save_package_.get(); }

  // Whether or not the info bar is visible. This delegates to
  // the ChromeFrame method InfoBarVisibilityChanged.
  void SetInfoBarVisible(bool visible);
  virtual bool IsInfoBarVisible() { return info_bar_visible_; }

  // Whether or not the FindInPage bar is visible.
  void SetFindInPageVisible(bool visible);

  // Create the InfoBarView and returns it if none has been created.
  // Just returns existing InfoBarView if it is already created.
  virtual InfoBarView* GetInfoBarView();

  // Prepare for saving page.
  void OnSavePage();

  // Save page with the main HTML file path, the directory for saving resources,
  // and the save type: HTML only or complete web page.
  void SavePage(const std::wstring& main_file, const std::wstring& dir_path,
                SavePackage::SavePackageType save_type);

  // Get all savable resource links from current webpage, include main
  // frame and sub-frame.
  void GetAllSavableResourceLinksForCurrentPage(const GURL& page_url);

  // Get html data by serializing all frames of current page with lists
  // which contain all resource links that have local copy.
  // The parameter links contain original URLs of all saved links.
  // The parameter local_paths contain corresponding local file paths of
  // all saved links, which matched with vector:links one by one.
  // The parameter local_directory_name is relative path of directory which
  // contain all saved auxiliary files included all sub frames and resouces.
  void GetSerializedHtmlDataForCurrentPageWithLocalLinks(
      const std::vector<std::wstring>& links,
      const std::vector<std::wstring>& local_paths,
      const std::wstring& local_directory_name);

  // Locates a sub frame with given xpath and executes the given
  // javascript in its context.
  void ExecuteJavascriptInWebFrame(const std::wstring& frame_xpath,
                                   const std::wstring& jscript);

  // Locates a sub frame with given xpath and logs a message to its
  // console.
  void AddMessageToConsole(const std::wstring& frame_xpath,
                           const std::wstring& message,
                           ConsoleMessageLevel level);

  // Request the corresponding render view to perform these operations
  void Undo();
  void Redo();
  void Replace(const std::wstring& text);
  void Delete();
  void SelectAll();

  // Sets the WebApp for this WebContents.
  void SetWebApp(WebApp* web_app);
  WebApp* web_app() { return web_app_.get(); }

  // Return whether this tab contents was created to contain an application.
  bool IsWebApplication() const;

  // Tell Gears to create a shortcut for the current page.
  void CreateShortcut();

  // Tell the render view to perform a file upload. |form| is the name or ID of
  // the form that should be used to perform the upload. |file| is the name or
  // ID of the file input that should be set to |file_path|. |submit| is the
  // name or ID of the submit button. If non empty, the submit button will be
  // pressed. If not, the form will be filled with the information but the user
  // will perform the post operation.
  //
  // |other_values| contains a list of key value pairs separated by '\n'.
  // Each key value pair is of the form key=value where key is a form name or
  // ID and value is the desired value.
  void StartFileUpload(const std::wstring& file_path,
                       const std::wstring& form,
                       const std::wstring& file,
                       const std::wstring& submit,
                       const std::wstring& other_values);

  // JavascriptMessageBoxHandler calls this when the dialog is closed.
  void OnJavaScriptMessageBoxClosed(IPC::Message* reply_msg, bool success,
    const std::wstring& prompt);

  void CopyImageAt(int x, int y);
  void InspectElementAt(int x, int y);
  void ShowJavaScriptConsole();
  void AllowDomAutomationBindings();

  // Tell the render view to fill in a form and optionally submit it.
  void FillForm(const FormData& form);

  // Tell the render view to fill a password form and trigger autocomplete
  // in the case of multiple matching logins.
  void FillPasswordForm(const PasswordFormDomManager::FillData& form_data);

  // D&d drop target messages that get forwarded on to the render view host.
  void DragTargetDragEnter(const WebDropData& drop_data,
                           const gfx::Point& client_pt,
                           const gfx::Point& screen_pt);
  void DragTargetDragOver(const gfx::Point& client_pt,
                          const gfx::Point& screen_pt);
  void DragTargetDragLeave();
  void DragTargetDrop(const gfx::Point& client_pt,
                      const gfx::Point& screen_pt);

  // Called by PluginInstaller to start installation of missing plugin.
  void InstallMissingPlugin();

  // Returns the PasswordManager, creating it if necessary.
  PasswordManager* GetPasswordManager();

  // Returns the PluginInstaller, creating it if necessary.
  PluginInstaller* GetPluginInstaller();

  // Return the currently active RenderProcessHost, RenderViewHost, and
  // SiteInstance, respectively.  Each of these may change over time.  Callers
  // should be aware that the SiteInstance could be deleted if its ref count
  // drops to zero (i.e., if all RenderViewHosts and NavigationEntries that
  // use it are deleted).
  RenderProcessHost* process() const {
    return render_manager_.current_host()->process();
  }
  RenderViewHost* render_view_host() const {
    return render_manager_.current_host();
  }
  SiteInstance* site_instance() const {
    return render_manager_.current_host()->site_instance();
  }
  RenderWidgetHostView* view() const {
    return render_manager_.current_view();
  }

  // Overridden from TabContents to return the window of the
  // RenderWidgetHostView.
  virtual HWND GetContentHWND();

  // Handling the drag and drop of files into the content area.
  virtual bool CanDisplayFile(const std::wstring& full_path);

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

  virtual void WillCaptureContents();
  virtual void DidCaptureContents();

  virtual void Cut();
  virtual void Copy();
  virtual void Paste();

  // The rest of the system wants to interact with the delegate our render view
  // host manager has. See those setters for more.
  InterstitialPageDelegate* interstitial_page_delegate() const {
    return render_manager_.interstitial_delegate();
  }
  void set_interstitial_delegate(InterstitialPageDelegate* delegate) {
    render_manager_.set_interstitial_delegate(delegate);
  }

  // Displays the specified html in the current page. This method can be used to
  // show temporary pages (such as security error pages).  It can be hidden by
  // calling HideInterstitialPage, in which case the original page is restored.
  // An optional delegate may be passed, it is not owned by the WebContents.
  void ShowInterstitialPage(const std::string& html_text,
                            InterstitialPageDelegate* delegate) {
    render_manager_.ShowInterstitialPage(html_text, delegate);
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

  // Allows the WebContents to react when a cross-site response is ready to be
  // delivered to a pending RenderViewHost.  We must first run the onunload
  // handler of the old RenderViewHost before we can allow it to proceed.
  void OnCrossSiteResponse(int new_render_process_host_id,
                           int new_request_id) {
    render_manager_.OnCrossSiteResponse(new_render_process_host_id,
                                        new_request_id);
  }

  // Returns true if the active NavigationEntry's page_id equals page_id.
  bool IsActiveEntry(int32 page_id);

  const std::string& contents_mime_type() const {
    return contents_mime_type_;
  }

  // Returns true if this WebContents will notify about disconnection.
  bool notify_disconnection() const { return notify_disconnection_; }

  void set_override_encoding(const std::wstring& override_encoding) {
    override_encoding_ = override_encoding;
  }

 protected:
  FRIEND_TEST(WebContentsTest, OnMessageReceived);

  // Should be deleted via CloseContents.
  virtual ~WebContents();

  // RenderViewHostDelegate
  virtual RenderViewHostDelegate::FindInPage* GetFindInPageDelegate();
  virtual Profile* GetProfile() const;

  virtual void CreateView(int route_id, HANDLE modal_dialog_event);
  virtual void CreateWidget(int route_id);
  virtual void ShowView(int route_id,
                        WindowOpenDisposition disposition,
                        const gfx::Rect& initial_pos,
                        bool user_gesture);
  virtual void ShowWidget(int route_id, const gfx::Rect& initial_pos);
  virtual void RendererReady(RenderViewHost* render_view_host);
  virtual void RendererGone(RenderViewHost* render_view_host);
  virtual void DidNavigate(RenderViewHost* render_view_host,
                           const ViewHostMsg_FrameNavigate_Params& params);
  virtual void UpdateRenderViewSize();
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
  virtual void DidGetPrintedPagesCount(int cookie, int number_pages);
  virtual void DidPrintPage(const ViewHostMsg_DidPrintPage_Params& params);
  virtual GURL GetAlternateErrorPageURL() const;
  virtual WebPreferences GetWebkitPrefs();
  virtual void OnMissingPluginStatus(int status);
  virtual void OnCrashedPlugin(const std::wstring& plugin_path);
  virtual void OnJSOutOfMemory();
  virtual void OnReceivedSavableResourceLinksForCurrentPage(
      const std::vector<GURL>& resources_list,
      const std::vector<GURL>& referrers_list,
      const std::vector<GURL>& frames_list);
  virtual void OnReceivedSerializedHtmlData(const GURL& frame_url,
                                            const std::string& data,
                                            int32 status);
  virtual void ShouldClosePage(bool proceed) {
    render_manager_.ShouldClosePage(proceed);
  }
  virtual bool CanBlur() const;
  virtual void RendererUnresponsive(RenderViewHost* render_view_host);
  virtual void RendererResponsive(RenderViewHost* render_view_host);
  virtual void LoadStateChanged(const GURL& url, net::LoadState load_state);

  // Notification that a page has an OpenSearch description document available
  // at url. This checks to see if we should generate a keyword based on the
  // OSDD, and if necessary uses TemplateURLFetcher to download the OSDD
  // and create a keyword.
  virtual void PageHasOSDD(RenderViewHost* render_view_host,
                           int32 page_id, const GURL& url, bool autodetected);

  virtual void OnDidGetApplicationInfo(
      int32 page_id,
      const webkit_glue::WebApplicationInfo& info);

  // Overridden from TabContents.
  virtual void SetInitialFocus(bool reverse);

  // Handle reply from inspect element request
  virtual void InspectElementReply(int num_resources);

  // Handle keyboard events not processed by the renderer.
  virtual void HandleKeyboardEvent(const WebKeyboardEvent& event);

  // Notifies the RenderWidgetHost instance about the fact that the
  // page is loading, or done loading and calls the base implementation.
  void SetIsLoading(bool is_loading, LoadNotificationDetails* details);

  // Overridden from SelectFileDialog::Listener:
  virtual void FileSelected(const std::wstring& path, void* params);
  virtual void FileSelectionCanceled(void* params);

  // Another part of RenderViewHostManager::Delegate.
  //
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
  friend class TestWebContents;

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

  bool ScrollZoom(int scroll_type);
  void WheelZoom(int distance);

  // Backend for LoadURL that optionally creates a history entry. The
  // transition type will be ignored if a history entry is not created.
  void LoadURL(const std::wstring& url, bool create_history_entry,
               PageTransition::Type transition);

  // Windows Event handlers
  virtual void OnDestroy();
  virtual void OnHScroll(int scroll_type, short position, HWND scrollbar);
  virtual void OnMouseLeave();
  virtual LRESULT OnMouseRange(UINT msg, WPARAM w_param, LPARAM l_param);
  virtual void OnPaint(HDC junk_dc);
  virtual LRESULT OnReflectedMessage(UINT msg, WPARAM w_param, LPARAM l_param);
  virtual void OnSetFocus(HWND window);
  virtual void OnVScroll(int scroll_type, short position, HWND scrollbar);
  virtual void OnWindowPosChanged(WINDOWPOS* window_pos);

  // Callback from HistoryService for our request for a favicon.
  void OnFavIconData(HistoryService::Handle handle,
                     bool know_favicon,
                     scoped_refptr<RefCountedBytes> data,
                     bool expired);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Helper functions for sending notifications.
  void NotifySwapped();
  void NotifyConnected();
  void NotifyDisconnected();

  // Called by OnMsgNavigate to update history state.
  virtual void UpdateHistoryForNavigation(const GURL& display_url,
      const ViewHostMsg_FrameNavigate_Params& params);

  // If params has a searchable form, this tries to create a new keyword.
  void GenerateKeywordIfNecessary(
      const ViewHostMsg_FrameNavigate_Params& params);

  // Sets up the View that holds the rendered web page, receives messages for
  // it and contains page plugins.
  RenderWidgetHostHWND* CreatePageView(RenderViewHost* render_view_host);

  // Navigation helpers --------------------------------------------------------
  //
  // These functions are helpers for Navigate() and DidNavigate().

  // Creates a new navigation entry (malloced, the caller will have to free)
  // for the given committed load. Used by DidNavigate. Will not return NULL.
  NavigationEntry* CreateNavigationEntryForCommit(
      const ViewHostMsg_FrameNavigate_Params& params);

  // Handles post-navigation tasks specific to some set of frames. DidNavigate()
  // calls these with newly created navigation entry for this navigation BEFORE
  // that entry has been committed to the navigation controller. The functions
  // can update the entry as needed.
  //
  // First the frame-specific version (main or sub) will be called to update the
  // entry as needed after it was created by CreateNavigationEntryForCommit.
  //
  // Then DidNavigateAnyFramePreCommit will be called with the now-complete
  // entry for further processing that is not specific to the type of frame.
  void DidNavigateMainFramePreCommit(
      const ViewHostMsg_FrameNavigate_Params& params,
      NavigationEntry* entry);
  void DidNavigateSubFramePreCommit(
      const ViewHostMsg_FrameNavigate_Params& params,
      NavigationEntry* entry);
  void DidNavigateAnyFramePreCommit(
      const ViewHostMsg_FrameNavigate_Params& params,
      NavigationEntry* entry);

  // Handles post-navigation tasks in DidNavigate AFTER the entry has been
  // committed to the navigation controller. See WillNavigate* above. Note that
  // the navigation entry is not provided since it may be invalid/changed after
  // being committed.
  void DidNavigateMainFramePostCommit(
      const ViewHostMsg_FrameNavigate_Params& params);
  void DidNavigateAnyFramePostCommit(
      RenderViewHost* render_view_host,
      const ViewHostMsg_FrameNavigate_Params& params);

  // Called when navigating the main frame to close all child windows if the
  // domain is changing.
  void MaybeCloseChildWindows(const ViewHostMsg_FrameNavigate_Params& params);

  // Broadcasts a notification for the provisional load committing, used by
  // DidNavigate.
  void BroadcastProvisionalLoadCommit(
      RenderViewHost* render_view_host,
      const ViewHostMsg_FrameNavigate_Params& params);

  // Convenience method that returns true if navigating to the specified URL
  // from the current one is an in-page navigation (jumping to a ref in the
  // page).
  bool IsInPageNavigation(const GURL& url) const;

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

  // RenderViewHostManager::Delegate pass-throughs -----------------------------

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
  virtual void UpdateRenderViewSizeForRenderManager() {
    UpdateRenderViewSize();
  }
  virtual void NotifySwappedFromRenderManager() {
    NotifySwapped();
  }
  virtual NavigationController* GetControllerForRenderManager() {
    return controller();
  }
  
  // Profiling -----------------------------------------------------------------

  // Logs the commit of the load for profiling purposes. Used by DidNavigate.
  void HandleProfilingForDidNavigate(
      const ViewHostMsg_FrameNavigate_Params& params);

  // If performance profiling is enabled, save current PageLoadTracker entry
  // to visited page list.
  void SaveCurrentProfilingEntry();

  // If performance profiling is enabled, create a new PageLoadTracker entry
  // when navigating to a new page.
  void CreateNewProfilingEntry(const GURL& url);

  // Enumerate and 'un-parent' any plugin windows that are children
  // of this web contents.
  void DetachPluginWindows();
  static BOOL CALLBACK EnumPluginWindowsCallback(HWND window, LPARAM param);

  // Data ----------------------------------------------------------------------

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

  // Whether the WebContents is doing performance profiling
  bool is_profiling_;

  // System time at which the current load was started.
  TimeTicks current_load_start_;

  // Whether we have a (non-empty) title for the current page.
  // Used to prevent subsequent title updates from affecting history.
  bool has_page_title_;

  // SavePackage, lazily created.
  scoped_refptr<SavePackage> save_package_;

  // InfoBarView, lazily created.
  scoped_ptr<InfoBarView> info_bar_view_;

  // Whether the info bar view is visible.
  bool info_bar_visible_;

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

  // A drop target object that handles drags over this WebContents.
  scoped_refptr<WebDropTarget> drop_target_;

  // The SadTab renderer.
  scoped_ptr<SadTabView> sad_tab_;

  // This flag is true while we are in the photo-booth.  See dragged_tab.cc.
  bool capturing_contents_;

  // Handles downloading favicons.
  FavIconHelper fav_icon_helper_;

  // Dialog box used for choosing files to upload from file form fields.
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  // Info bar for crashed plugin message.
  // IMPORTANT: This instance is owned by the InfoBarView. It is valid
  // only if InfoBarView::GetChildIndex for this view is valid.
  InfoBarMessageView* crashed_plugin_info_bar_;

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

  // These maps hold on to the pages/widgets that we created on behalf of the
  // renderer that haven't shown yet.
  typedef stdext::hash_map<int, WebContents*> PendingViews;
  PendingViews pending_views_;

  typedef stdext::hash_map<int, RenderWidgetHost*> PendingWidgets;
  PendingWidgets pending_widgets_;

  // Non-null if we're displaying content for a web app.
  scoped_refptr<WebApp> web_app_;

  // Specified encoding which is used to override current tab's encoding.
  std::wstring override_encoding_;

  DISALLOW_COPY_AND_ASSIGN(WebContents);
};

#endif  // CHROME_BROWSER_WEB_CONTENTS_H_
