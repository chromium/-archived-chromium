// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_WEB_CONTENTS_H_
#define CHROME_BROWSER_TAB_CONTENTS_WEB_CONTENTS_H_

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/render_view_host_manager.h"
#include "net/base/load_states.h"
#include "webkit/glue/password_form.h"
#include "webkit/glue/webpreferences.h"

#if defined(OS_MACOSX) || defined(OS_LINUX)
// Remove when we've finished porting the supporting classes.
#include "chrome/common/temp_scaffolding_stubs.h"
#elif defined(OS_WIN)
#include "chrome/browser/download/save_package.h"
#include "chrome/browser/fav_icon_helper.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/shell_dialogs.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/gears_api.h"
#endif

class AutofillForm;
class AutofillManager;
class InterstitialPageDelegate;
class LoadNotificationDetails;
class PasswordManager;
class PluginInstaller;
class RenderProcessHost;
class RenderViewHost;
class RenderViewHostFactory;
class RenderWidgetHost;
struct ThumbnailScore;
struct ViewHostMsg_FrameNavigate_Params;
struct ViewHostMsg_DidPrintPage_Params;
class WebContentsView;

namespace base {
class WaitableEvent;
}

namespace webkit_glue {
struct WebApplicationInfo;
}

namespace IPC {
class Message;
}

// WebContents represents the contents of a tab that shows web pages. It embeds
// a RenderViewHost (via RenderViewHostManager) to actually display the page.
class WebContents : public TabContents,
                    public RenderViewHostDelegate,
                    public RenderViewHostManager::Delegate,
                    public SelectFileDialog::Listener {
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
              base::WaitableEvent* modal_dialog_event);

  static void RegisterUserPrefs(PrefService* prefs);

  // Getters -------------------------------------------------------------------

  // Returns the AutofillManager, creating it if necessary.
  AutofillManager* GetAutofillManager();

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

  const std::wstring& encoding() const { return encoding_; }
  void set_encoding(const std::wstring& encoding) {
    encoding_ = encoding;
  }

  // TabContents (public overrides) --------------------------------------------

  virtual void Destroy();
  virtual WebContents* AsWebContents() { return this; }
  virtual SiteInstance* GetSiteInstance() const;
  virtual std::wstring GetStatusText() const;
  virtual bool NavigateToPendingEntry(bool reload);
  virtual void Stop();
  virtual void Cut();
  virtual void Copy();
  virtual void Paste();
  virtual void DisassociateFromPopupCount();
  virtual void DidBecomeSelected();
  virtual void WasHidden();
  virtual void ShowContents();
  virtual void HideContents();
  virtual void SetDownloadShelfVisible(bool visible);
  virtual void PopupNotificationVisibilityChanged(bool visible);

  // Retarded pass-throughs to the view.
  // TODO(brettw) fix this, tab contents shouldn't have these methods, probably
  // it should be killed altogether.
  virtual void CreateView();
#if defined(OS_WIN)
  virtual HWND GetContainerHWND() const;
  virtual HWND GetContentHWND();
  virtual void GetContainerBounds(gfx::Rect *out) const;
#endif

  // Web apps ------------------------------------------------------------------

  // Tell Gears to create a shortcut for the current page.
  void CreateShortcut();

  // Interstitials -------------------------------------------------------------

  // Various other systems need to know about our interstitials.
  bool showing_interstitial_page() const {
    return render_manager_.interstitial_page() != NULL;
  }

  // Sets the passed passed interstitial as the currently showing interstitial.
  // |interstitial_page| should be non NULL (use the remove_interstitial_page
  // method to unset the interstitial) and no interstitial page should be set
  // when there is already a non NULL interstitial page set.
  void set_interstitial_page(InterstitialPage* interstitial_page) {
    render_manager_.set_interstitial_page(interstitial_page);
  }

  // Unsets the currently showing interstitial.
  void remove_interstitial_page() {
    render_manager_.remove_interstitial_page();
  }

  // Returns the currently showing interstitial, NULL if no interstitial is
  // showing.
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

  // Override the encoding and reload the page by sending down
  // ViewMsg_SetPageEncoding to the renderer. |UpdateEncoding| is kinda
  // the opposite of this, by which 'browser' is notified of
  // the encoding of the current tab from 'renderer' (determined by
  // auto-detect, http header, meta, bom detection, etc).
  void override_encoding(const std::wstring& encoding) {
    set_encoding(encoding);
    render_view_host()->SetPageEncoding(encoding);
  }

  void CrossSiteNavigationCanceled() {
    render_manager_.CrossSiteNavigationCanceled();
  }

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
  virtual RenderViewHostDelegate::Save* GetSaveDelegate() const;
  virtual Profile* GetProfile() const;
  virtual void RendererReady(RenderViewHost* render_view_host);
  virtual void RendererGone(RenderViewHost* render_view_host);
  virtual void DidNavigate(RenderViewHost* render_view_host,
                           const ViewHostMsg_FrameNavigate_Params& params);
  virtual void UpdateState(RenderViewHost* render_view_host,
                           int32 page_id,
                           const std::string& state);
  virtual void UpdateTitle(RenderViewHost* render_view_host,
                           int32 page_id,
                           const std::wstring& title);
  virtual void UpdateEncoding(RenderViewHost* render_view_host,
                              const std::wstring& encoding);
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
  virtual void DidFailProvisionalLoadWithError(RenderViewHost* render_view_host,
                                               bool is_main_frame,
                                               int error_code,
                                               const GURL& url);
  virtual void UpdateFavIconURL(RenderViewHost* render_view_host,
                                int32 page_id, const GURL& icon_url);
  virtual void DidDownloadImage(RenderViewHost* render_view_host,
                                int id,
                                const GURL& image_url,
                                bool errored,
                                const SkBitmap& image);
  virtual void RequestOpenURL(const GURL& url, const GURL& referrer,
                              WindowOpenDisposition disposition);
  virtual void DomOperationResponse(const std::string& json_string,
                                    int automation_id);
  virtual void ProcessExternalHostMessage(const std::string& receiver,
                                          const std::string& message);
  virtual void GoToEntryAtOffset(int offset);
  virtual void GetHistoryListCount(int* back_list_count,
                                   int* forward_list_count);
  virtual void RunFileChooser(bool multiple_files,
                              const std::wstring& title,
                              const std::wstring& default_file,
                              const std::wstring& filter);
  virtual void RunJavaScriptMessage(const std::wstring& message,
                                    const std::wstring& default_prompt,
                                    const int flags,
                                    IPC::Message* reply_msg,
                                    bool* did_suppress_message);
  virtual void RunBeforeUnloadConfirm(const std::wstring& message,
                                      IPC::Message* reply_msg);
  virtual void ShowModalHTMLDialog(const GURL& url, int width, int height,
                                   const std::string& json_arguments,
                                   IPC::Message* reply_msg);
  virtual void PasswordFormsSeen(const std::vector<PasswordForm>& forms);
  virtual void AutofillFormSubmitted(const AutofillForm& form);
  virtual void GetAutofillSuggestions(const std::wstring& field_name,
      const std::wstring& user_text, int64 node_id, int request_id);
  virtual void PageHasOSDD(RenderViewHost* render_view_host,
                           int32 page_id, const GURL& url, bool autodetected);
  virtual void InspectElementReply(int num_resources);
  virtual void DidGetPrintedPagesCount(int cookie, int number_pages);
  virtual void DidPrintPage(const ViewHostMsg_DidPrintPage_Params& params);
  virtual GURL GetAlternateErrorPageURL() const;
  virtual WebPreferences GetWebkitPrefs();
  virtual void OnMissingPluginStatus(int status);
  virtual void OnCrashedPlugin(const FilePath& plugin_path);
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
  virtual gfx::Rect GetRootWindowResizerRect() const;
  virtual void RendererUnresponsive(RenderViewHost* render_view_host,
                                    bool is_during_unload);
  virtual void RendererResponsive(RenderViewHost* render_view_host);
  virtual void LoadStateChanged(const GURL& url, net::LoadState load_state);
  virtual void OnDidGetApplicationInfo(
      int32 page_id,
      const webkit_glue::WebApplicationInfo& info);
  virtual void OnEnterOrSpace();
  virtual bool CanTerminate() const;


  // SelectFileDialog::Listener ------------------------------------------------

  virtual void FileSelected(const std::wstring& path, void* params);
  virtual void MultiFilesSelected(const std::vector<std::wstring>& files,
                                  void* params);
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
#if defined(OS_WIN)
  friend class WebContentsViewWin;
#elif defined(OS_MACOSX)
  friend class WebContentsViewMac;
#endif

  // So InterstitialPage can access SetIsLoading.
  friend class InterstitialPage;

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

  // Closes all child windows (constrained popups) when the domain changes.
  // Supply the new and old URLs, and this function will figure out when the
  // domain changing conditions are met.
  void MaybeCloseChildWindows(const GURL& previous_url,
                              const GURL& current_url);

  // Updates the starred state from the bookmark bar model. If the state has
  // changed, the delegate is notified.
  void UpdateStarredStateForCurrentURL();

  // Send the alternate error page URL to the renderer. This method is virtual
  // so special html pages can override this (e.g., the new tab page).
  virtual void UpdateAlternateErrorPageURL();

  // Send webkit specific settings to the renderer.
  void UpdateWebPreferences();

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

  // Saves the given title to the navigation entry and does associated work. It
  // will update history and the view for the new title, and also synthesize
  // titles for file URLs that have none (so we require that the URL of the
  // entry already be set).
  //
  // This is used as the backend for state updates, which include a new title,
  // or the dedicated set title message. It returns true if the new title is
  // different and was therefore updated.
  bool UpdateTitleForEntry(NavigationEntry* entry, const std::wstring& title);

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

  // Maps from handle to page_id.
  typedef std::map<HistoryService::Handle, int32> HistoryRequestMap;
  HistoryRequestMap history_requests_;

  // System time at which the current load was started.
  base::TimeTicks current_load_start_;

  // Whether we have a (non-empty) title for the current page.
  // Used to prevent subsequent title updates from affecting history. This
  // prevents some weirdness because some AJAXy apps use titles for status
  // messages.
  bool received_page_title_;

  // SavePackage, lazily created.
  scoped_refptr<SavePackage> save_package_;

  // Tracks our pending CancelableRequests. This maps pending requests to
  // page IDs so that we know whether a given callback still applies. The
  // page ID -1 means no page ID was set.
  CancelableRequestConsumerT<int32, -1> cancelable_consumer_;

  // Whether the current URL is starred
  bool is_starred_;

#if defined(OS_WIN)
  // Handle to an event that's set when the page is showing a message box (or
  // equivalent constrained window).  Plugin processes check this to know if
  // they should pump messages then.
  ScopedHandle message_box_active_;
#endif

  // AutofillManager, lazily created.
  scoped_ptr<AutofillManager> autofill_manager_;

  // PasswordManager, lazily created.
  scoped_ptr<PasswordManager> password_manager_;

  // PluginInstaller, lazily created.
  scoped_ptr<PluginInstaller> plugin_installer_;

  // Handles downloading favicons.
  FavIconHelper fav_icon_helper_;

  // Dialog box used for choosing files to upload from file form fields.
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  // The time that the last javascript message was dismissed.
  base::TimeTicks last_javascript_message_dismissal_;

  // True if the user has decided to block future javascript messages. This is
  // reset on navigations to false on navigations.
  bool suppress_javascript_messages_;

  // When a navigation occurs, we record its contents MIME type. It can be
  // used to check whether we can do something for some special contents.
  std::string contents_mime_type_;

  // Character encoding. TODO(jungshik) : convert to std::string
  std::wstring encoding_;

  PendingInstall pending_install_;

  // The last time that the download shelf was made visible.
  base::TimeTicks last_download_shelf_show_;

  // The current load state and the URL associated with it.
  net::LoadState load_state_;
  std::wstring load_state_host_;

  DISALLOW_COPY_AND_ASSIGN(WebContents);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_WEB_CONTENTS_H_
