// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_H_
#define CHROME_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "net/base/load_states.h"
#include "webkit/glue/window_open_disposition.h"

class AutofillForm;
struct ContextMenuParams;
class FilePath;
class GURL;
struct NativeWebKeyboardEvent;
class NavigationEntry;
class Profile;
struct RendererPreferences;
class RenderProcessHost;
class RenderViewHost;
class ResourceRequestDetails;
class SkBitmap;
class TabContents;
struct ThumbnailScore;
struct ViewHostMsg_DidPrintPage_Params;
struct ViewHostMsg_FrameNavigate_Params;
struct WebDropData;
class WebKeyboardEvent;
struct WebPreferences;

namespace base {
class WaitableEvent;
}

namespace gfx {
class Rect;
}

namespace IPC {
class Message;
}

namespace webkit_glue {
class AutofillForm;
struct PasswordForm;
struct WebApplicationInfo;
}

//
// RenderViewHostDelegate
//
//  An interface implemented by an object interested in knowing about the state
//  of the RenderViewHost.
//
//  This interface currently encompasses every type of message that was
//  previously being sent by TabContents itself. Some of these notifications
//  may not be relevant to all users of RenderViewHost and we should consider
//  exposing a more generic Send function on RenderViewHost and a response
//  listener here to serve that need.
//
class RenderViewHostDelegate {
 public:
  // View ----------------------------------------------------------------------
  // Functions that can be routed directly to a view-specific class.

  class View {
   public:
    // The page is trying to open a new page (e.g. a popup window). The
    // window should be created associated with the given route, but it should
    // not be shown yet. That should happen in response to ShowCreatedWindow.
    //
    // Note: this is not called "CreateWindow" because that will clash with
    // the Windows function which is actually a #define.
    //
    // NOTE: this takes ownership of @modal_dialog_event
    virtual void CreateNewWindow(int route_id,
                                 base::WaitableEvent* modal_dialog_event) = 0;

    // The page is trying to open a new widget (e.g. a select popup). The
    // widget should be created associated with the given route, but it should
    // not be shown yet. That should happen in response to ShowCreatedWidget.
    // If |activatable| is false, the widget cannot be activated or get focus.
    virtual void CreateNewWidget(int route_id, bool activatable) = 0;

    // Show a previously created page with the specified disposition and bounds.
    // The window is identified by the route_id passed to CreateNewWindow.
    //
    // Note: this is not called "ShowWindow" because that will clash with
    // the Windows function which is actually a #define.
    virtual void ShowCreatedWindow(int route_id,
                                   WindowOpenDisposition disposition,
                                   const gfx::Rect& initial_pos,
                                   bool user_gesture,
                                   const GURL& creator_url) = 0;

    // Show the newly created widget with the specified bounds.
    // The widget is identified by the route_id passed to CreateNewWidget.
    virtual void ShowCreatedWidget(int route_id,
                                   const gfx::Rect& initial_pos) = 0;

    // A context menu should be shown, to be built using the context information
    // provided in the supplied params.
    virtual void ShowContextMenu(const ContextMenuParams& params) = 0;

    // The user started dragging content of the specified type within the
    // RenderView. Contextual information about the dragged content is supplied
    // by WebDropData.
    virtual void StartDragging(const WebDropData& drop_data) = 0;

    // The page wants to update the mouse cursor during a drag & drop operation.
    // |is_drop_target| is true if the mouse is over a valid drop target.
    virtual void UpdateDragCursor(bool is_drop_target) = 0;

    // Notification that view for this delegate got the focus.
    virtual void GotFocus() = 0;

    // Callback to inform the browser it should take back focus. If reverse is
    // true, it means the focus was retrieved by doing a Shift-Tab.
    virtual void TakeFocus(bool reverse) = 0;

    // Callback to inform the browser that the renderer did not process the
    // specified events. This gives an opportunity to the browser to process the
    // event (used for keyboard shortcuts).
    virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event) = 0;

    // Notifications about mouse events in this view.  This is useful for
    // implementing global 'on hover' features external to the view.
    virtual void HandleMouseEvent() = 0;
    virtual void HandleMouseLeave() = 0;

    // The content's intrinsic width (prefWidth) changed.
    virtual void UpdatePreferredWidth(int pref_width) = 0;
  };

  // BrowserIntegration --------------------------------------------------------
  // Functions that integrate with other browser services.

  class BrowserIntegration {
   public:
    // Notification the user has made a gesture while focus was on the
    // page. This is used to avoid uninitiated user downloads (aka carpet
    // bombing), see DownloadRequestManager for details.
    virtual void OnUserGesture() = 0;

    // A find operation in the current page completed.
    virtual void OnFindReply(int request_id,
                             int number_of_matches,
                             const gfx::Rect& selection_rect,
                             int active_match_ordinal,
                             bool final_update) = 0;

    // Navigate to the history entry for the given offset from the current
    // position within the NavigationController.  Makes no change if offset is
    // not valid.
    virtual void GoToEntryAtOffset(int offset) = 0;

    // The page requests the size of the back and forward lists
    // within the NavigationController.
    virtual void GetHistoryListCount(int* back_list_count,
                                     int* forward_list_count) = 0;

    // Notification when default plugin updates status of the missing plugin.
    virtual void OnMissingPluginStatus(int status) = 0;

    // Notification from the renderer that a plugin instance has crashed.
    //
    // BrowserIntegration isn't necessarily the best place for this, if you
    // need to implement this function somewhere that doesn't need any other
    // BrowserIntegration callbacks, feel free to move it elsewhere.
    virtual void OnCrashedPlugin(const FilePath& plugin_path) = 0;

    // Notification that a worker process has crashed.
    virtual void OnCrashedWorker() = 0;

    // Notification that a request for install info has completed.
    virtual void OnDidGetApplicationInfo(
        int32 page_id,
        const webkit_glue::WebApplicationInfo& app_info) = 0;
  };

  // Resource ------------------------------------------------------------------
  // Notifications of resource loading events.

  class Resource {
   public:
    // The RenderView is starting a provisional load.
    virtual void DidStartProvisionalLoadForFrame(
        RenderViewHost* render_view_host,
        bool is_main_frame,
        const GURL& url) = 0;

    // Notification by the resource loading system (not the renderer) that it
    // has started receiving a resource response. This is different than
    // DidStartProvisionalLoadForFrame above because this is called for every
    // resource (images, automatically loaded subframes, etc.) and provisional
    // loads are only for user-initiated navigations.
    //
    // The pointer ownership is NOT transferred.
    virtual void DidStartReceivingResourceResponse(
        ResourceRequestDetails* details) = 0;

    // Sent when a provisional load is redirected.
    virtual void DidRedirectProvisionalLoad(int32 page_id,
                                            const GURL& source_url,
                                            const GURL& target_url) = 0;

    // Notification by the resource loading system (not the renderer) that a
    // resource was redirected. This is different than
    // DidRedirectProvisionalLoad above because this is called for every
    // resource (images, automatically loaded subframes, etc.) and provisional
    // loads are only for user-initiated navigations.
    //
    // The pointer ownership is NOT transferred.
    virtual void DidRedirectResource(ResourceRequestDetails* details) = 0;

    // The RenderView loaded a resource from an in-memory cache.
    // |security_info| contains the security info if this resource was
    // originally loaded over a secure connection.
    virtual void DidLoadResourceFromMemoryCache(
        const GURL& url,
        const std::string& frame_origin,
        const std::string& main_frame_origin,
        const std::string& security_info) = 0;

    // The RenderView failed a provisional load with an error.
    virtual void DidFailProvisionalLoadWithError(
        RenderViewHost* render_view_host,
        bool is_main_frame,
        int error_code,
        const GURL& url,
        bool showing_repost_interstitial) = 0;

    // Notification that a document has been loaded in a frame.
    virtual void DocumentLoadedInFrame() = 0;
  };

  // Save ----------------------------------------------------------------------
  // Interface for saving web pages.

  class Save {
   public:
    // Notification that we get when we receive all savable links of
    // sub-resources for the current page, their referrers and list of frames
    // (include main frame and sub frames).
    virtual void OnReceivedSavableResourceLinksForCurrentPage(
        const std::vector<GURL>& resources_list,
        const std::vector<GURL>& referrers_list,
        const std::vector<GURL>& frames_list) = 0;

    // Notification that we get when we receive serialized html content data of
    // a specified web page from render process. The parameter frame_url
    // specifies what frame the data belongs. The parameter data contains the
    // available data for sending. The parameter status indicates the
    // serialization status, See
    // webkit_glue::DomSerializerDelegate::PageSavingSerializationStatus for
    // the detail meaning of status.
    virtual void OnReceivedSerializedHtmlData(const GURL& frame_url,
                                              const std::string& data,
                                              int32 status) = 0;
  };

  // FavIcon -------------------------------------------------------------------
  // Interface for the renderer to supply favicon information.

  class FavIcon {
   public:
    // An image that was requested to be downloaded by DownloadImage has
    // completed.
    //
    // TODO(brettw) this should be renamed DidDownloadFavIcon, and the RVH
    // function, IPC message, and the RenderView function DownloadImage should
    // all be named DownloadFavIcon.
    virtual void DidDownloadFavIcon(RenderViewHost* render_view_host,
                                    int id,
                                    const GURL& image_url,
                                    bool errored,
                                    const SkBitmap& image) = 0;

    // The URL for the FavIcon of a page has changed.
    virtual void UpdateFavIconURL(RenderViewHost* render_view_host,
                                  int32 page_id,
                                  const GURL& icon_url) = 0;
  };

  // ---------------------------------------------------------------------------

  // Returns the current delegate associated with a feature. May return NULL if
  // there is no corresponding delegate.
  virtual View* GetViewDelegate() const;
  virtual BrowserIntegration* GetBrowserIntegrationDelegate() const;
  virtual Resource* GetResourceDelegate() const;
  virtual Save* GetSaveDelegate() const;
  virtual FavIcon* GetFavIconDelegate() const;

  // Gets the URL that is currently being displayed, if there is one.
  virtual const GURL& GetURL() const;

  // Return this object cast to a TabContents, if it is one. If the object is
  // not a TabContents, returns NULL.
  virtual TabContents* GetAsTabContents();

  // The RenderView is being constructed (message sent to the renderer process
  // to construct a RenderView).  Now is a good time to send other setup events
  // to the RenderView.  This precedes any other commands to the RenderView.
  virtual void RenderViewCreated(RenderViewHost* render_view_host) {}

  // The RenderView has been constructed.
  virtual void RenderViewReady(RenderViewHost* render_view_host) {}

  // The RenderView died somehow (crashed or was killed by the user).
  virtual void RenderViewGone(RenderViewHost* render_view_host) {}

  // The RenderView is going to be deleted. This is called when each
  // RenderView is going to be destroyed
  virtual void RenderViewDeleted(RenderViewHost* render_view_host) { }

  // The RenderView was navigated to a different page.
  virtual void DidNavigate(RenderViewHost* render_view_host,
                           const ViewHostMsg_FrameNavigate_Params& params) {}

  // The state for the page changed and should be updated.
  virtual void UpdateState(RenderViewHost* render_view_host,
                           int32 page_id,
                           const std::string& state) {}

  // The page's title was changed and should be updated.
  virtual void UpdateTitle(RenderViewHost* render_view_host,
                           int32 page_id,
                           const std::wstring& title) {}

  // The page's encoding was changed and should be updated.
  virtual void UpdateEncoding(RenderViewHost* render_view_host,
                              const std::wstring& encoding) {}

  // The destination URL has changed should be updated
  virtual void UpdateTargetURL(int32 page_id, const GURL& url) {}

  // The thumbnail representation of the page changed and should be updated.
  virtual void UpdateThumbnail(const GURL& url,
                               const SkBitmap& bitmap,
                               const ThumbnailScore& score) {}

  // Inspector settings were changes and should be persisted.
  virtual void UpdateInspectorSettings(const std::wstring& raw_settings) {}

  // The page is trying to close the RenderView's representation in the client.
  virtual void Close(RenderViewHost* render_view_host) {}

  // The page is trying to move the RenderView's representation in the client.
  virtual void RequestMove(const gfx::Rect& new_bounds) {}

  // The RenderView began loading a new page. This corresponds to WebKit's
  // notion of the throbber starting.
  virtual void DidStartLoading(RenderViewHost* render_view_host) {}

  // The RenderView stopped loading a page. This corresponds to WebKit's
  // notion of the throbber stopping.
  virtual void DidStopLoading(RenderViewHost* render_view_host) {}

  // The page wants to open a URL with the specified disposition.
  virtual void RequestOpenURL(const GURL& url,
                              const GURL& referrer,
                              WindowOpenDisposition disposition) {}

  // A DOM automation operation completed. The result of the operation is
  // expressed in a json string.
  virtual void DomOperationResponse(const std::string& json_string,
                                    int automation_id) {}

  // A message was sent from HTML-based UI.
  // By default we ignore such messages.
  virtual void ProcessDOMUIMessage(const std::string& message,
                                   const std::string& content,
                                   int request_id,
                                   bool has_callback) {}

  // A message for external host. By default we ignore such messages.
  // |receiver| can be a receiving script and |message| is any
  // arbitrary string that makes sense to the receiver.
  virtual void ProcessExternalHostMessage(const std::string& message,
                                          const std::string& origin,
                                          const std::string& target) {}

  // A file chooser should be shown.
  virtual void RunFileChooser(bool multiple_files,
                              const string16& title,
                              const FilePath& default_file) {}

  // A javascript message, confirmation or prompt should be shown.
  virtual void RunJavaScriptMessage(const std::wstring& message,
                                    const std::wstring& default_prompt,
                                    const GURL& frame_url,
                                    const int flags,
                                    IPC::Message* reply_msg,
                                    bool* did_suppress_message) {}

  virtual void RunBeforeUnloadConfirm(const std::wstring& message,
                                      IPC::Message* reply_msg) {}

  virtual void ShowModalHTMLDialog(const GURL& url, int width, int height,
                                   const std::string& json_arguments,
                                   IPC::Message* reply_msg) {}

  // Password forms have been detected in the page.
  virtual void PasswordFormsSeen(
      const std::vector<webkit_glue::PasswordForm>& forms) {}

  // Forms fillable by autofill have been detected in the page.
  virtual void AutofillFormSubmitted(const webkit_glue::AutofillForm& form) {}

  // Called to retrieve a list of suggestions from the web database given
  // the name of the field |field_name| and what the user has already typed in
  // the field |user_text|.  Appeals to the database thead to perform the query.
  // When the database thread is finished, the autofill manager retrieves the
  // calling RenderViewHost and then passes the vector of suggestions to
  // RenderViewHost::AutofillSuggestionsReturned.
  virtual void GetAutofillSuggestions(const std::wstring& field_name,
                                      const std::wstring& user_text,
                                      int64 node_id,
                                      int request_id) {}

  // Called when the user has indicated that she wants to remove the specified
  // autofill suggestion from the database.
  virtual void RemoveAutofillEntry(const std::wstring& field_name,
                                   const std::wstring& value) {}

  // Notification that the page has an OpenSearch description document.
  virtual void PageHasOSDD(RenderViewHost* render_view_host,
                           int32 page_id, const GURL& doc_url,
                           bool autodetected) {}

  // Notification that the render view has calculated the number of printed
  // pages.
  virtual void DidGetPrintedPagesCount(int cookie, int number_pages) {}

  // Notification that the render view is done rendering one printed page. This
  // call is synchronous, the renderer is waiting on us because of the EMF
  // memory mapped data.
  virtual void DidPrintPage(const ViewHostMsg_DidPrintPage_Params& params) {}

  // |url| is assigned to a server that can provide alternate error pages.  If
  // the returned URL is empty, the default error page built into WebKit will
  // be used.
  virtual GURL GetAlternateErrorPageURL() const;

  // Return a dummy RendererPreferences object that will be used by the renderer
  // associated with the owning RenderViewHost.
  virtual RendererPreferences GetRendererPrefs() const;

  // Returns a WebPreferences object that will be used by the renderer
  // associated with the owning render view host.
  virtual WebPreferences GetWebkitPrefs();

  // Notification from the renderer that JS runs out of memory.
  virtual void OnJSOutOfMemory() {}

  // Notification whether we should close the page, after an explicit call to
  // AttemptToClosePage.  This is called before a cross-site request or before
  // a tab/window is closed, to allow the appropriate renderer to approve or
  // deny the request.  |proceed| indicates whether the user chose to proceed.
  virtual void ShouldClosePage(bool proceed) {}

  // Called by ResourceDispatcherHost when a response for a pending cross-site
  // request is received.  The ResourceDispatcherHost will pause the response
  // until the onunload handler of the previous renderer is run.
  virtual void OnCrossSiteResponse(int new_render_process_host_id,
                                   int new_request_id) {}

  // Called the ResourceDispatcherHost's associate CrossSiteRequestHandler
  // when a cross-site navigation has been canceled.
  virtual void OnCrossSiteNavigationCanceled() {}

  // Returns true if this this object can be blurred through a javascript
  // obj.blur() call. ConstrainedWindows shouldn't be able to be blurred, but
  // generally most other windows will be.
  virtual bool CanBlur() const;

  // Return the rect where to display the resize corner, if any, otherwise
  // an empty rect.
  virtual gfx::Rect GetRootWindowResizerRect() const;

  // Notification that the renderer has become unresponsive. The
  // delegate can use this notification to show a warning to the user.
  virtual void RendererUnresponsive(RenderViewHost* render_view_host,
                                    bool is_during_unload) {}

  // Notification that a previously unresponsive renderer has become
  // responsive again. The delegate can use this notification to end the
  // warning shown to the user.
  virtual void RendererResponsive(RenderViewHost* render_view_host) {}

  // Notification that the RenderViewHost's load state changed.
  virtual void LoadStateChanged(const GURL& url, net::LoadState load_state) {}

  // Returns true if this view is used to host an external tab container.
  virtual bool IsExternalTabContainer() const;

  // The RenderView has inserted one css file into page.
  virtual void DidInsertCSS() {}
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_H_
