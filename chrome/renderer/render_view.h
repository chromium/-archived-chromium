// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDER_VIEW_H_
#define CHROME_RENDERER_RENDER_VIEW_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_handle.h"
#include "base/gfx/point.h"
#include "base/gfx/rect.h"
#include "base/timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/page_zoom.h"
#include "chrome/common/resource_dispatcher.h"
#ifdef CHROME_PERSONALIZATION
#include "chrome/personalization/personalization.h"
#endif
#include "chrome/renderer/automation/dom_automation_controller.h"
#include "chrome/renderer/dom_ui_bindings.h"
#include "chrome/renderer/external_host_bindings.h"
#include "chrome/renderer/external_js_object.h"
#include "chrome/renderer/render_process.h"
#include "chrome/renderer/render_widget.h"
#include "testing/gtest/include/gtest/gtest_prod.h"
#include "webkit/glue/console_message_level.h"
#include "webkit/glue/dom_serializer_delegate.h"
#include "webkit/glue/glue_accessibility.h"
#include "webkit/glue/webview_delegate.h"
#include "webkit/glue/weburlrequest.h"
#include "webkit/glue/webview.h"

#if defined(OS_WIN)
// RenderView is a diamond-shaped hierarchy, with WebWidgetDelegate at the root.
// VS warns when we inherit the WebWidgetDelegate method implementations from
// RenderWidget.  It's safe to ignore that warning.
#pragma warning(disable: 4250)
#endif

class DebugMessageHandler;
class GURL;
class RenderThread;
class SkBitmap;
struct ThumbnailScore;
class WebError;
class WebFrame;
class WebPluginDelegate;
class WebPluginDelegateProxy;

namespace base {
class WaitableEvent;
}

namespace webkit_glue {
struct FileUploadData;
}

// We need to prevent a page from trying to create infinite popups. It is not
// as simple as keeping a count of the number of immediate children
// popups. Having an html file that window.open()s itself would create
// an unlimited chain of RenderViews who only have one RenderView child.
//
// Therefore, each new top level RenderView creates a new counter and shares it
// with all its children and grandchildren popup RenderViews created with
// CreateWebView() to have a sort of global limit for the page so no more than
// kMaximumNumberOfPopups popups are created.
//
// This is a RefCounted holder of an int because I can't say
// scoped_refptr<int>.
typedef base::RefCountedData<int> SharedRenderViewCounter;

//
// RenderView is an object that manages a WebView object, and provides a
// communication interface with an embedding application process
//
class RenderView : public RenderWidget,
                   public WebViewDelegate,
                   public webkit_glue::DomSerializerDelegate {
 public:
  // Creates a new RenderView.  The parent_hwnd specifies a HWND to use as the
  // parent of the WebView HWND that will be created.  The modal_dialog_event
  // is set by the RenderView whenever a modal dialog alert is shown, so that
  // the renderer and plugin processes know to pump window messages.  If this
  // is a constrained popup or as a new tab, opener_id is the routing ID of the
  // RenderView responsible for creating this RenderView (corresponding to the
  // parent_hwnd). |counter| is either a currently initialized counter, or NULL
  // (in which case we treat this RenderView as a top level window).
  static RenderView* Create(
      RenderThreadBase* render_thread,
      HWND parent_hwnd,
      base::WaitableEvent* modal_dialog_event,  // takes ownership
      int32 opener_id,
      const WebPreferences& webkit_prefs,
      SharedRenderViewCounter* counter,
      int32 routing_id);

  // Sets the "next page id" counter.
  static void SetNextPageID(int32 next_page_id);

  // The resource dispatcher used to fetch resources for this view.
  ResourceDispatcher* resource_dispatcher() {
    return resource_dispatcher_;
  }

  // May return NULL when the view is closing.
  WebView* webview() const {
    return static_cast<WebView*>(webwidget());
  }

  HWND host_window() const {
    return host_window_;
  }

  base::WaitableEvent* modal_dialog_event() {
    return modal_dialog_event_.get();
  }

  // IPC::Channel::Listener
  virtual void OnMessageReceived(const IPC::Message& msg);

  // WebViewDelegate
  virtual void ShowModalHTMLDialog(const GURL& url, int width, int height,
                                   const std::string& json_arguments,
                                   std::string* json_retval);
  virtual void RunJavaScriptAlert(WebView* webview,
                                  const std::wstring& message);
  virtual bool RunJavaScriptConfirm(WebView* webview,
                                    const std::wstring& message);
  virtual bool RunJavaScriptPrompt(WebView* webview,
                                   const std::wstring& message,
                                   const std::wstring& default_value,
                                   std::wstring* result);
  virtual bool RunBeforeUnloadConfirm(WebView* webview,
                                      const std::wstring& message);
  virtual void EnableSuddenTermination();
  virtual void DisableSuddenTermination();
  virtual void QueryFormFieldAutofill(const std::wstring& field_name,
                                      const std::wstring& text,
                                      int64 node_id);
  virtual void UpdateTargetURL(WebView* webview,
                               const GURL& url);
  virtual void RunFileChooser(bool multi_select,
                              const std::wstring& title,
                              const std::wstring& initial_filename,
                              const std::wstring& filter,
                              WebFileChooserCallback* file_chooser);
  virtual void AddMessageToConsole(WebView* webview,
                                   const std::wstring& message,
                                   unsigned int line_no,
                                   const std::wstring& source_id);

  virtual void DebuggerOutput(const std::wstring& out);

  virtual void DidStartLoading(WebView* webview);
  virtual void DidStopLoading(WebView* webview);
  virtual void DidStartProvisionalLoadForFrame(
      WebView* webview,
      WebFrame* frame,
      NavigationGesture gesture);
  virtual void DidReceiveProvisionalLoadServerRedirect(WebView* webview,
                                                       WebFrame* frame);
  virtual void DidFailProvisionalLoadWithError(WebView* webview,
                                               const WebError& error,
                                               WebFrame* frame);
  virtual void LoadNavigationErrorPage(WebFrame* frame,
                                       const WebRequest* failed_request,
                                       const WebError& error,
                                       const std::string& html,
                                       bool replace);
  virtual void DidCommitLoadForFrame(WebView* webview, WebFrame* frame,
                                     bool is_new_navigation);
  virtual void DidReceiveTitle(WebView* webview,
                               const std::wstring& title,
                               WebFrame* frame);
  virtual void DidFinishLoadForFrame(WebView* webview,
                                     WebFrame* frame);
  virtual void DidFailLoadWithError(WebView* webview,
                                    const WebError& error,
                                    WebFrame* forFrame);
  virtual void DidFinishDocumentLoadForFrame(WebView* webview, WebFrame* frame);
  virtual bool DidLoadResourceFromMemoryCache(WebView* webview,
                                              const WebRequest& request,
                                              const WebResponse& response,
                                              WebFrame* frame);
  virtual void DidHandleOnloadEventsForFrame(WebView* webview, WebFrame* frame);
  virtual void DidChangeLocationWithinPageForFrame(WebView* webview,
                                                   WebFrame* frame,
                                                   bool is_new_navigation);
  virtual void DidReceiveIconForFrame(WebView* webview, WebFrame* frame);

  virtual void WillPerformClientRedirect(WebView* webview,
                                         WebFrame* frame,
                                         const GURL& src_url,
                                         const GURL& dest_url,
                                         unsigned int delay_seconds,
                                         unsigned int fire_date);
  virtual void DidCancelClientRedirect(WebView* webview,
                                       WebFrame* frame);
  virtual void DidCompleteClientRedirect(WebView* webview,
                                         WebFrame* frame,
                                         const GURL& source);

  virtual void WindowObjectCleared(WebFrame* webframe);
  virtual WindowOpenDisposition DispositionForNavigationAction(
      WebView* webview,
      WebFrame* frame,
      const WebRequest* request,
      WebNavigationType type,
      WindowOpenDisposition disposition,
      bool is_redirect);

  virtual WebView* CreateWebView(WebView* webview, bool user_gesture);
  virtual WebWidget* CreatePopupWidget(WebView* webview, bool activatable);
  virtual WebPluginDelegate* CreatePluginDelegate(
      WebView* webview,
      const GURL& url,
      const std::string& mime_type,
      const std::string& clsid,
      std::string* actual_mime_type);
  virtual webkit_glue::WebMediaPlayerDelegate* CreateMediaPlayerDelegate();
  virtual void OnMissingPluginStatus(WebPluginDelegate* delegate, int status);
  virtual void OpenURL(WebView* webview, const GURL& url,
                       const GURL& referrer,
                       WindowOpenDisposition disposition);
  virtual void DidDownloadImage(int id,
                                const GURL& image_url,
                                bool errored,
                                const SkBitmap& image);
  virtual GURL GetAlternateErrorPageURL(const GURL& failedURL,
                                        ErrorPageType error_type);

  virtual void ShowContextMenu(WebView* webview,
                               ContextNode::Type type,
                               int x,
                               int y,
                               const GURL& link_url,
                               const GURL& image_url,
                               const GURL& page_url,
                               const GURL& frame_url,
                               const std::wstring& selection_text,
                               const std::wstring& misspelled_word,
                               int edit_flags,
                               const std::string& security_info);
  virtual void StartDragging(WebView* webview,
                             const WebDropData& drag_data);

  virtual void TakeFocus(WebView* webview, bool reverse);
  virtual void JSOutOfMemory();

  virtual WebHistoryItem* GetHistoryEntryAtOffset(int offset);
  virtual int GetHistoryBackListCount();
  virtual int GetHistoryForwardListCount();
  virtual void OnNavStateChanged(WebView* webview);
  virtual void SetTooltipText(WebView* webview,
                              const std::wstring& tooltip_text);

  virtual void DownloadUrl(const GURL& url, const GURL& referrer);

  virtual void OnPasswordFormsSeen(WebView* webview,
                                   const std::vector<PasswordForm>& forms);

  virtual void OnAutofillFormSubmitted(WebView* webview,
                                       const AutofillForm& form);

  virtual void ReportFindInPageMatchCount(int count, int request_id,
                                          bool final_update);
  virtual void ReportFindInPageSelection(int request_id,
                                         int active_match_ordinal,
                                         const gfx::Rect& selection_rect);
  virtual bool WasOpenedByUserGesture(WebView* webview) const;
  virtual void SpellCheck(const std::wstring& word, int& misspell_location,
                          int& misspell_length);
  virtual void SetInputMethodState(bool enabled);
  virtual void ScriptedPrint(WebFrame* frame);
  virtual void WebInspectorOpened(int num_resources);
  virtual void UserMetricsRecordAction(const std::wstring& action);
  virtual void DnsPrefetch(const std::vector<std::string>& host_names);

  // DomSerializerDelegate
  virtual void DidSerializeDataForFrame(const GURL& frame_url,
      const std::string& data, PageSavingSerializationStatus status);

  // WebWidgetDelegate
  // Most methods are handled by RenderWidget.
  virtual void Show(WebWidget* webwidget, WindowOpenDisposition disposition);
  virtual void CloseWidgetSoon(WebWidget* webwidget);
  virtual void RunModal(WebWidget* webwidget);

  // Do not delete directly.  This class is reference counted.
  virtual ~RenderView();

  // Called when a plugin is destroyed.
  void PluginDestroyed(WebPluginDelegateProxy* proxy);

  // Called when a plugin is crashed.
  void PluginCrashed(const FilePath& plugin_path);

  // Called from JavaScript window.external.AddSearchProvider() to add a
  // keyword for a provider described in the given OpenSearch document.
  void AddSearchProvider(const std::string& url);

  // Asks the browser for the CPBrowsingContext associated with this renderer.
  uint32 GetCPBrowsingContext();

  // Dispatches the current navigation state to the browser. Called on a
  // periodic timer so we don't send too many messages.
  void SyncNavigationState();

  // Evaluates a string of JavaScript in a particular frame.
  void EvaluateScript(const std::wstring& frame_xpath,
                      const std::wstring& jscript);

  // Called when the Javascript debugger is no longer attached.
  // This is called from within the renderer, not via an IPC message.
  void OnDebugDetach();

  int delay_seconds_for_form_state_sync() const {
    return delay_seconds_for_form_state_sync_;
  }
  void set_delay_seconds_for_form_state_sync(int delay_in_seconds) {
    delay_seconds_for_form_state_sync_ = delay_in_seconds;
  }

 private:
  FRIEND_TEST(RenderViewTest, OnLoadAlternateHTMLText);
  FRIEND_TEST(RenderViewTest, OnNavStateChanged);

  explicit RenderView(RenderThreadBase* render_thread);

  // Initializes this view with the given parent and ID. The |routing_id| can be
  // set to 'MSG_ROUTING_NONE' if the true ID is not yet known. In this case,
  // CompleteInit must be called later with the true ID.
  void Init(HWND parent,
            base::WaitableEvent* modal_dialog_event,  // takes ownership
            int32 opener_id,
            const WebPreferences& webkit_prefs,
            SharedRenderViewCounter* counter,
            int32 routing_id);

  void UpdateURL(WebFrame* frame);
  void UpdateTitle(WebFrame* frame, const std::wstring& title);
  void UpdateSessionHistory(WebFrame* frame);

  // Update current main frame's encoding and send it to browser window.
  // Since we want to let users see the right encoding info from menu
  // before finishing loading, we call the UpdateEncoding in
  // a) function:DidCommitLoadForFrame. When this function is called,
  // that means we have got first data. In here we try to get encoding
  // of page if it has been specified in http header.
  // b) function:DidReceiveTitle. When this function is called,
  // that means we have got specified title. Because in most of webpages,
  // title tags will follow meta tags. In here we try to get encoding of
  // page if it has been specified in meta tag.
  // c) function:DidFinishDocumentLoadForFrame. When this function is
  // called, that means we have got whole html page. In here we should
  // finally get right encoding of page.
  void UpdateEncoding(WebFrame* frame, const std::wstring& encoding_name);

  // Captures the thumbnail and text contents for indexing for the given load
  // ID. If the view's load ID is different than the parameter, this call is
  // a NOP. Typically called on a timer, so the load ID may have changed in the
  // meantime.
  void CapturePageInfo(int load_id, bool preliminary_capture);

  // Called to retrieve the text from the given frame contents, the page text
  // up to the maximum amount will be placed into the given buffer
  void CaptureText(WebFrame* frame, std::wstring* contents);

  // Creates a thumbnail of |frame|'s contents resized to (|w|, |h|)
  // and puts that in |thumbnail|. Thumbnail metadata goes in |score|.
  bool CaptureThumbnail(WebFrame* frame, int w, int h,
                        SkBitmap* thumbnail,
                        ThumbnailScore* score);

  // Calculates how "boring" a thumbnail is. The boring score is the
  // 0,1 ranged percentage of pixels that are the most common
  // luma. Higher boring scores indicate that a higher percentage of a
  // bitmap are all the same brightness.
  double CalculateBoringScore(SkBitmap* bitmap);

  bool RunJavaScriptMessage(int type,
                            const std::wstring& message,
                            const std::wstring& default_value,
                            std::wstring* result);

  // Adds search provider from the given OpenSearch description URL as a
  // keyword search.
  void AddGURLSearchProvider(const GURL& osd_url, bool autodetected);

  // Tells the browser process to navigate to a back/forward entry at the given
  // offset from current.
  void GoToEntryAtOffset(int offset);

  // RenderView IPC message handlers
#if defined(OS_WIN)
  void OnCreatingNewAck(HWND parent);
#else
  void OnCreatingNewAck();
#endif
  void SendThumbnail();
  void OnPrintPage(const ViewMsg_PrintPage_Params& params);
  void OnGetPrintedPagesCount(const ViewMsg_Print_Params& params);
  void OnPrintPages(const ViewMsg_PrintPages_Params& params);
  void OnNavigate(const ViewMsg_Navigate_Params& params);
  void OnStop();
  void OnLoadAlternateHTMLText(const std::string& html_contents,
                               bool new_navigation,
                               const GURL& display_url,
                               const std::string& security_info);
  void OnStopFinding(bool clear_selection);
  void OnFindReplyAck();
  void OnUpdateTargetURLAck();
  void OnUndo();
  void OnRedo();
  void OnCut();
  void OnCopy();
  void OnPaste();
  void OnReplace(const std::wstring& text);
  void OnToggleSpellCheck();
  void OnDelete();
  void OnSelectAll();
  void OnCopyImageAt(int x, int y);
  void OnInspectElement(int x, int y);
  void OnShowJavaScriptConsole();
  void OnCancelDownload(int32 download_id);
  void OnFind(const FindInPageRequest& request);
  void OnZoom(int function);
  void OnSetPageEncoding(const std::wstring& encoding_name);
  void OnGetAllSavableResourceLinksForCurrentPage(const GURL& page_url);
  void OnGetSerializedHtmlDataForCurrentPageWithLocalLinks(
      const std::vector<std::wstring>& links,
      const std::vector<std::wstring>& local_paths,
      const std::wstring& local_directory_name);
  void OnUploadFileRequest(const ViewMsg_UploadFile_Params& p);
  void OnFormFill(const FormData& form);
  void OnFillPasswordForm(const PasswordFormDomManager::FillData& form_data);
  void OnDragTargetDragEnter(const WebDropData& drop_data,
                             const gfx::Point& client_pt,
                             const gfx::Point& screen_pt);
  void OnDragTargetDragOver(const gfx::Point& client_pt,
                            const gfx::Point& screen_pt);
  void OnDragTargetDragLeave();
  void OnDragTargetDrop(const gfx::Point& client_pt,
                        const gfx::Point& screen_pt);
  void OnAllowDomAutomationBindings(bool allow_binding);
  void OnAllowBindings(bool enable_dom_ui_bindings,
                       bool enable_external_host_bindings);
  void OnSetDOMUIProperty(const std::string& name, const std::string& value);
  void OnSetInitialFocus(bool reverse);
  void OnUpdateWebPreferences(const WebPreferences& prefs);
  void OnSetAltErrorPageURL(const GURL& gurl);

  void OnDownloadImage(int id, const GURL& image_url, int image_size);

  void OnGetApplicationInfo(int page_id);

  void OnScriptEvalRequest(const std::wstring& frame_xpath,
                           const std::wstring& jscript);
  void OnAddMessageToConsole(const std::wstring& frame_xpath,
                             const std::wstring& msg,
                             ConsoleMessageLevel level);
  void OnDebugAttach();

  void OnReservePageIDRange(int size_of_range);

  void OnDragSourceEndedOrMoved(
      int client_x, int client_y, int screen_x, int screen_y, bool ended);
  void OnDragSourceSystemDragEnded();
  void OnInstallMissingPlugin();
  void OnFileChooserResponse(const std::vector<std::wstring>& file_names);
  void OnEnableViewSourceMode();
  void OnUpdateBackForwardListCount(int back_list_count,
                                    int forward_list_count);
  void OnGetAccessibilityInfo(const ViewMsg_Accessibility_In_Params& in_params,
                              ViewHostMsg_Accessibility_Out_Params* out_params);
  void OnClearAccessibilityInfo(int iaccessible_id, bool clear_all);

  // Checks if the RenderView should close, runs the beforeunload handler and
  // sends ViewMsg_ShouldClose to the browser.
  void OnMsgShouldClose();

  // Runs the onunload handler and closes the page, replying with ClosePage_ACK
  // (with the given RPH and request IDs, to help track the request).
  void OnClosePage(int new_render_process_host_id, int new_request_id);

  // Notification about ui theme changes.
  void OnThemeChanged();

  // Notification that we have received autofill suggestion.
  void OnReceivedAutofillSuggestions(
      int64 node_id,
      int request_id,
      const std::vector<std::wstring>& suggestions,
      int default_suggestions_index);

  // Message that the popup notification has been shown or hidden.
  void OnPopupNotificationVisiblityChanged(bool visible);

#ifdef CHROME_PERSONALIZATION
  void OnPersonalizationEvent(std::string event_name, std::string event_args);
#endif

  // Handles messages posted from automation.
  void OnMessageFromExternalHost(const std::string& target,
                                 const std::string& message);

  // Message that we should no longer be part of the current popup window
  // grouping, and should form our own grouping.
  void OnDisassociateFromPopupCount();

  // Switches the frame's CSS media type to "print" and calculate the number of
  // printed pages that are to be expected. |frame| will be used to calculate
  // the number of expected pages for this frame only.
  int SwitchFrameToPrintMediaType(const ViewMsg_Print_Params& params,
                                  WebFrame* frame);

  // Switches the frame's CSS media type to "display".
  void SwitchFrameToDisplayMediaType(WebFrame* frame);

  // Prints the page listed in |params|.
  void PrintPage(const ViewMsg_PrintPage_Params& params, WebFrame* frame);

  // Prints all the pages listed in |params|.
  void PrintPages(const ViewMsg_PrintPages_Params& params, WebFrame* frame);

  // Attempt to upload the file that we are trying to process if any.
  // Reset the pending file upload data if the form was successfully
  // posted.
  void ProcessPendingUpload();

  // Reset the pending file upload.
  void ResetPendingUpload();

  // Exposes the DOMAutomationController object that allows JS to send
  // information to the browser process.
  void BindDOMAutomationController(WebFrame* webframe);

  void set_opened_by_user_gesture(bool value) {
    opened_by_user_gesture_ = value;
  }

  // Called by RenderWidget after it paints.
  virtual void DidPaint();

  // Locates a sub frame with given xpath
  WebFrame* GetChildFrame(const std::wstring& frame_xpath) const;

  std::string GetAltHTMLForTemplate(const DictionaryValue& error_strings,
                                    int template_resource_id) const;

  virtual void TransitionToCommittedForNewPage();

  virtual void DidAddHistoryItem();

  // A helper method used by WasOpenedByUserGesture.
  bool WasOpenedByUserGestureHelper() const;

  void set_waiting_for_create_window_ack(bool wait) {
    waiting_for_create_window_ack_ = wait;
  }

  // Handles resource loads for this view.
  scoped_refptr<ResourceDispatcher> resource_dispatcher_;

  // DOM Automation Controller CppBoundClass.
  bool enable_dom_automation_;
  DomAutomationController dom_automation_controller_;

  // Chrome page<->browser messaging CppBoundClass.
  bool enable_dom_ui_bindings_;
  DOMUIBindings dom_ui_bindings_;

#ifdef CHROME_PERSONALIZATION
  RendererPersonalization personalization_;
#endif

  // window.external object for "built-in" JS extensions
  ExternalJSObject external_js_object_;

  // External host exposed through automation controller.
  bool enable_external_host_bindings_;
  ExternalHostBindings external_host_bindings_;

  // The last gotten main frame's encoding.
  std::wstring last_encoding_name_;

  // The URL we think the user's mouse is hovering over. We use this to
  // determine if we want to send a new one (we do not need to send
  // duplicates).
  GURL target_url_;

  // The state of our target_url transmissions. When we receive a request to
  // send a URL to the browser, we set this to TARGET_INFLIGHT until an ACK
  // comes back - if a new request comes in before the ACK, we store the new
  // URL in pending_target_url_ and set the status to TARGET_PENDING. If an
  // ACK comes back and we are in TARGET_PENDING, we send the stored URL and
  // revert to TARGET_INFLIGHT.
  //
  // We don't need a queue of URLs to send, as only the latest is useful.
  enum {
    TARGET_NONE,
    TARGET_INFLIGHT,  // We have a request in-flight, waiting for an ACK
    TARGET_PENDING    // INFLIGHT + we have a URL waiting to be sent
  } target_url_status_;

  // The next target URL we want to send to the browser.
  GURL pending_target_url_;

  // Are we loading our top level frame
  bool is_loading_;

  // If we are handling a top-level client-side redirect, this tracks the URL
  // of the page that initiated it. Specifically, when a load is committed this
  // is used to determine if that load originated from a client-side redirect.
  // It is empty if there is no top-level client-side redirect.
  GURL completed_client_redirect_src_;

  // The gesture that initiated the current navigation.
  NavigationGesture navigation_gesture_;

  // Unique id to identify the current page between browser and renderer.
  //
  // Note that this is NOT updated for every main frame navigation, only for
  // "regular" navigations that go into session history. In particular, client
  // redirects, like the page cycler uses (document.location.href="foo") do not
  // count as regular navigations and do not increment the page id.
  int32 page_id_;

  // Indicates the ID of the last page that we sent a FrameNavigate to the
  // browser for. This is used to determine if the most recent transition
  // generated a history entry (less than page_id_), or not (equal to or
  // greater than). Note that this will be greater than page_id_ if the user
  // goes back.
  int32 last_page_id_sent_to_browser_;

  // Page_id from the last page we indexed. This prevents us from indexing the
  // same page twice in a row.
  int32 last_indexed_page_id_;

  // Used for popups.
  bool opened_by_user_gesture_;

  // The alternate error page URL, if one exists.
  GURL alternate_error_page_url_;

  // The pending file upload.
  scoped_ptr<webkit_glue::FileUploadData> pending_upload_data_;

  ScopedRunnableMethodFactory<RenderView> method_factory_;

  // Timer used to delay the updating of nav state (see SyncNavigationState).
  base::OneShotTimer<RenderView> nav_state_sync_timer_;

  typedef std::vector<WebPluginDelegateProxy*> PluginDelegateList;
  PluginDelegateList plugin_delegates_;

  // Remember the first uninstalled plugin, so that we can ask the plugin
  // to install itself when user clicks on the info bar.
  WebPluginDelegate* first_default_plugin_;

  // If the browser hasn't sent us an ACK for the last FindReply we sent
  // to it, then we need to queue up the message (keeping only the most
  // recent message if new ones come in).
  scoped_ptr<IPC::Message> queued_find_reply_message_;

  // Handle to an event that's set when the page is showing a modal dialog (or
  // equivalent constrained window).  The renderer and any plugin processes
  // check this to know if they should pump messages/tasks then.
  scoped_ptr<base::WaitableEvent> modal_dialog_event_;

  // Document width when in print CSS media type. 0 otherwise.
  int printed_document_width_;

  // Backup the view size before printing since it needs to be overriden. This
  // value is set to restore the view size when printing is done.
  gfx::Size printing_view_size_;

  scoped_refptr<DebugMessageHandler> debug_message_handler_;

  scoped_ptr<WebFileChooserCallback> file_chooser_;

  int history_back_list_count_;
  int history_forward_list_count_;

  // True if pop-up blocking is disabled.  False by default.
  bool disable_popup_blocking_;

  // True if the page has any frame-level unload or beforeunload listeners.
  bool has_unload_listener_;

  // The total number of unrequested popups that exist and can be followed back
  // to a common opener. This count is shared among all RenderViews created
  // with CreateWebView(). All popups are treated as unrequested until
  // specifically instructed otherwise by the Browser process.
  scoped_refptr<SharedRenderViewCounter> shared_popup_counter_;

  // Whether this is a top level window (instead of a popup). Top level windows
  // shouldn't count against their own |shared_popup_counter_|.
  bool decrement_shared_popup_at_destruction_;

  // Handles accessibility requests into the renderer side, as well as
  // maintains the cache and other features of the accessibility tree.
  scoped_ptr<GlueAccessibility> glue_accessibility_;

  // True if user scripts are enabled in this process.
  bool user_scripts_enabled_;

  // Resource message queue. Used to queue up resource IPCs if we need
  // to wait for an ACK from the browser before proceeding.
  std::queue<IPC::Message*> queued_resource_messages_;

  // Set if we are waiting for an ack for ViewHostMsg_CreateWindow
  bool waiting_for_create_window_ack_;

  // The id of the last request sent for form field autofill.  Used to ignore
  // out of date responses.
  int form_field_autofill_request_id_;

  // A cached WebHistoryItem used for back/forward navigations initiated by
  // WebCore (via the window.history.go API).  We only have one such navigation
  // pending at a time.
  scoped_refptr<WebHistoryItem> history_navigation_item_;

  // We need to prevent windows from closing themselves with a window.close()
  // call while a blocked popup notification is being displayed. We cannot
  // synchronously querry the Browser process. We cannot wait for the Browser
  // process to send a message to us saying that a blocked popup notification
  // is being displayed. We instead assume that when we create a window off
  // this RenderView, that it is going to be blocked until we get a message
  // from the Browser process telling us otherwise.
  bool popup_notification_visible_;

  // Time in seconds of the delay between syncing page state such as form
  // elements and scroll position. This timeout allows us to avoid spamming the
  // browser process with every little thing that changes. This normally doesn't
  // change but is overridden by tests.
  int delay_seconds_for_form_state_sync_;

  DISALLOW_COPY_AND_ASSIGN(RenderView);
};

#endif  // CHROME_RENDERER_RENDER_VIEW_H_
