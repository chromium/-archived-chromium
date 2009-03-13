/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WEBKIT_GLUE_WEBFRAME_IMPL_H_
#define WEBKIT_GLUE_WEBFRAME_IMPL_H_

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "skia/ext/platform_canvas.h"
#include "webkit/glue/password_autocomplete_listener.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webframeloaderclient_impl.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "ResourceHandleClient.h"
#include "Frame.h"
#include "PlatformString.h"
MSVC_POP_WARNING();

class AltErrorPageResourceFetcher;
class WebDataSourceImpl;
class WebErrorImpl;
class WebHistoryItemImpl;
class WebPluginDelegate;
class WebRequest;
class WebView;
class WebViewImpl;
class WebTextInput;
class WebTextInputImpl;

namespace WebCore {
class Frame;
class FrameView;
class HistoryItem;
class Node;
class Range;
class SubstituteData;
struct WindowFeatures;
}

namespace gfx {
class BitmapPlatformDeviceWin;
}

// Implementation of WebFrame, note that this is a reference counted object.
class WebFrameImpl : public WebFrame, public base::RefCounted<WebFrameImpl> {
 public:
  WebFrameImpl();
  ~WebFrameImpl();

  static int live_object_count() {
    return live_object_count_;
  }

  // Called by the WebViewImpl to initialize its main frame:
  void InitMainFrame(WebViewImpl* webview_impl);

  // WebFrame
  virtual void LoadRequest(WebRequest* request);
  virtual void LoadHTMLString(const std::string& html_text,
                              const GURL& base_url);
  virtual void LoadAlternateHTMLString(const WebRequest* request,
                                       const std::string& html_text,
                                       const GURL& display_url,
                                       bool replace);
  virtual void LoadAlternateHTMLErrorPage(const WebRequest* request,
                                          const WebError& error,
                                          const GURL& error_page_url,
                                          bool replace,
                                          const GURL& fake_url);
  virtual void ExecuteJavaScript(const std::string& js_code,
                                 const GURL& script_url,
                                 int start_line);
  virtual bool GetPreviousHistoryState(std::string* history_state) const;
  virtual bool GetCurrentHistoryState(std::string* history_state) const;
  virtual bool HasCurrentHistoryState() const;
  virtual GURL GetURL() const;
  virtual GURL GetFavIconURL() const;
  virtual GURL GetOSDDURL() const;
  virtual scoped_refptr<class FeedList> GetFeedList() const;
  virtual WebDataSource* GetDataSource() const;
  virtual WebDataSource* GetProvisionalDataSource() const;
  virtual void StopLoading();
  virtual WebFrame* GetOpener() const;
  virtual WebFrame* GetParent() const;
  virtual WebFrame* GetChildFrame(const std::wstring& xpath) const;
  virtual WebView* GetView() const;
  virtual bool CaptureImage(scoped_ptr<skia::BitmapPlatformDevice>* image,
                            bool scroll_to_zero);

  // This method calls createRuntimeObject (in KJS::Bindings::Instance), which
  // increments the refcount of the NPObject passed in.
  virtual void BindToWindowObject(const std::wstring& name, NPObject* object);
  virtual void CallJSGC();

  virtual void* GetFrameImplementation() { return frame(); }

  virtual NPObject* GetWindowNPObject();

  virtual void GetContentAsPlainText(int max_chars, std::wstring* text) const;
  virtual bool Find(const FindInPageRequest& request,
                    bool wrap_within_frame,
                    gfx::Rect* selection_rect);
  virtual void StopFinding(bool clear_selection);
  virtual void ScopeStringMatches(FindInPageRequest request, bool reset);
  virtual void CancelPendingScopingEffort();
  virtual void ResetMatchCount();
  virtual bool Visible();
  virtual void SelectAll();
  virtual void Copy();
  virtual void Cut();
  virtual void Paste();
  virtual void Replace(const std::wstring& text);
  virtual void ToggleSpellCheck();
  virtual bool SpellCheckEnabled();
  virtual void Delete();
  virtual void Undo();
  virtual void Redo();
  virtual void ClearSelection();
  virtual std::string GetSelection(bool as_html);

  virtual void SetInViewSourceMode(bool enable);

  virtual bool GetInViewSourceMode() const;

  virtual void DidReceiveData(WebCore::DocumentLoader* loader,
                              const char* data,
                              int length);
  virtual void DidFail(const WebCore::ResourceError&, bool was_provisional);

  virtual std::wstring GetName();

  virtual WebTextInput* GetTextInput();

  virtual bool ExecuteCoreCommandByName(const std::string& name, const std::string& value);
  virtual bool IsCoreCommandEnabled(const std::string& name);

  virtual void AddMessageToConsole(const std::wstring& msg,
                                   ConsoleMessageLevel level);

  virtual void ClosePage();

  virtual gfx::Size ScrollOffset() const;

  virtual bool SetPrintingMode(bool printing,
                               float page_width_min,
                               float page_width_max,
                               int* width);
  virtual int ComputePageRects(const gfx::Size& page_size_px);
  virtual void GetPageRect(int page, gfx::Rect* page_size) const;
  virtual bool SpoolPage(int page, skia::PlatformCanvas* canvas);

  // Reformats this frame for printing or for screen display, depending on
  // |printing| flag. Acts recursively on inner frames.
  // Note: It fails if the main frame failed to load. It will succeed even if a
  // child frame failed to load.
  void SetPrinting(bool printing, float page_width_min, float page_width_max);

  PassRefPtr<WebCore::Frame> CreateChildFrame(
      const WebCore::FrameLoadRequest&,
      WebCore::HTMLFrameOwnerElement* owner_element);

  // WebFrameImpl
  void Layout();
  void Paint(skia::PlatformCanvas* canvas, const gfx::Rect& rect);

  bool IsLoading();

  void CreateFrameView();

  // The plugin delegate is used to get notifications when downloads complete.
  // This is used by the NPAPI method getURLNotify.  plugin_delegate() may
  // return NULL.  TODO(darin): how come there is only one per frame?!?
  WebPluginDelegate* plugin_delegate() const {
    return plugin_delegate_;
  }
  void set_plugin_delegate(WebPluginDelegate* plugin_delegate) {
    plugin_delegate_ = plugin_delegate;
  }

  WebCore::Frame* frame() const {
    return frame_;
  }

  static WebFrameImpl* FromFrame(WebCore::Frame* frame);

  WebViewImpl* webview_impl() const {
    return webview_impl_;
  }

  WebCore::FrameView* frameview() const {
    return frame_ ? frame_->view() : NULL;
  }

  // Update the given datasource with currently_loading_request's info.
  // If currently_loading_request is NULL, does nothing.
  void CacheCurrentRequestInfo(WebDataSourceImpl* datasource);

  void set_currently_loading_history_item(WebHistoryItemImpl* item);

  // Getters for the impls corresponding to Get(Provisional)DataSource. They
  // may return NULL if there is no corresponding data source.
  WebDataSourceImpl* GetDataSourceImpl() const;
  WebDataSourceImpl* GetProvisionalDataSourceImpl() const;

  // Returns which frame has an active match. This function should only be
  // called on the main frame, as it is the only frame keeping track. Returned
  // value can be NULL if no frame has an active match.
  const WebFrameImpl* active_match_frame() const {
    return active_match_frame_;
  }

  // When a Find operation ends, we want to set the selection to what was active
  // and set focus to the first focusable node we find (starting with the first
  // node in the matched range and going up the inheritance chain). If we find
  // nothing to focus we focus the first focusable node in the range. This
  // allows us to set focus to a link (when we find text inside a link), which
  // allows us to navigate by pressing Enter after closing the Find box.
  void SetFindEndstateFocusAndSelection();

  // Sets whether the WebFrameImpl allows its document to be scrolled.
  // If the parameter is true, allow the document to be scrolled.
  // Otherwise, disallow scrolling
  void SetAllowsScrolling(bool flag);

  // Returns true if the frame CSS is in "printing" mode.
  bool printing() const { return printing_; }

  // Registers a listener for the specified user name input element.  The
  // listener will receive notifications for blur and when autocomplete should
  // be triggered.
  // The WebFrameImpl becomes the owner of the passed listener.
  void RegisterPasswordListener(
      PassRefPtr<WebCore::HTMLInputElement> user_name_input_element,
      webkit_glue::PasswordAutocompleteListener* listener);

  // Returns the password autocomplete listener associated with the passed
  // user name input element, or NULL if none available.
  // Note that the returned listener is owner by the WebFrameImpl and should not
  // be kept around as it is deleted when the page goes away.
  webkit_glue::PasswordAutocompleteListener* GetPasswordListener(
      WebCore::HTMLInputElement* user_name_input_element);

 protected:
  friend class WebFrameLoaderClient;

  // Informs the WebFrame that the Frame is being closed, called by the
  // WebFrameLoaderClient
  void Closing();

  // A helper function for loading some document, given all of its data, into
  // this frame.  The charset may be empty if unknown, but a mime type must be
  // specified.  TODO(darin): Add option for storing this in session history.
  void LoadDocumentData(const WebCore::KURL& base_url,
                        const WebCore::String& data,
                        const WebCore::String& mime_type,
                        const WebCore::String& charset);

  // See WebFrame.h for details.
  virtual void IncreaseMatchCount(int count, int request_id);
  virtual void ReportFindInPageSelection(const gfx::Rect& selection_rect,
                                         int active_match_ordinal,
                                         int request_id);

  // Resource fetcher for downloading an alternate DNS error page.
  scoped_ptr<AltErrorPageResourceFetcher> alt_error_page_fetcher_;

  // Used to check for leaks of this object.
  static int live_object_count_;

  WebFrameLoaderClient frame_loader_client_;

  // This is a factory for creating cancelable tasks for this frame that run
  // asynchronously in order to scope string matches during a find operation.
  ScopedRunnableMethodFactory<WebFrameImpl> scope_matches_factory_;

  // This is a weak pointer to our containing WebViewImpl.
  WebViewImpl* webview_impl_;

  // This is a weak pointer to our corresponding WebCore frame.  A reference to
  // ourselves is held while frame_ is valid.  See our Closing method.
  WebCore::Frame* frame_;

  // This holds the request passed to LoadRequest, for access by the
  // WebFrameLoaderClient.  Unfortunately we have no other way to pass this
  // information to him.  Only non-NULL during a call to LoadRequest.
  const WebRequest* currently_loading_request_;

  // Similar to currently_loading_request_, except this will be set when
  // WebCore initiates a history navigation (probably via javascript).
  scoped_refptr<WebHistoryItemImpl> currently_loading_history_item_;

  // Plugins sometimes need to be notified when loads are complete so we keep
  // a pointer back to the appropriate plugin.
  WebPluginDelegate* plugin_delegate_;

  // Handling requests from TextInputController on this frame.
  scoped_ptr<WebTextInputImpl> webtextinput_impl_;

  // A way for the main frame to keep track of which frame has an active
  // match. Should be NULL for all other frames.
  WebFrameImpl* active_match_frame_;

  // The range of the active match for the current frame.
  RefPtr<WebCore::Range> active_match_;

  // The index of the active match.
  int active_match_index_;

  // This flag is used by the scoping effort to determine if we need to figure
  // out which rectangle is the active match. Once we find the active
  // rectangle we clear this flag.
  bool locating_active_rect_;

  // The scoping effort can time out and we need to keep track of where we
  // ended our last search so we can continue from where we left of.
  RefPtr<WebCore::Range> resume_scoping_from_range_;

  // Keeps track of the last string this frame searched for. This is used for
  // short-circuiting searches in the following scenarios: When a frame has
  // been searched and returned 0 results, we don't need to search that frame
  // again if the user is just adding to the search (making it more specific).
  std::wstring last_search_string_;

  // Keeps track of how many matches this frame has found so far, so that we
  // don't loose count between scoping efforts, and is also used (in conjunction
  // with last_search_string_ and scoping_complete_) to figure out if we need to
  // search the frame again.
  int last_match_count_;

  // This variable keeps a cumulative total of matches found so far for ALL the
  // frames on the page, and is only incremented by calling IncreaseMatchCount
  // (on the main frame only). It should be -1 for all other frames.
  size_t total_matchcount_;

  // This variable keeps a cumulative total of how many frames are currently
  // scoping, and is incremented/decremented on the main frame only.
  // It should be -1 for all other frames.
  int frames_scoping_count_;

  // Keeps track of whether the scoping effort was completed (the user may
  // interrupt it before it completes by submitting a new search).
  bool scoping_complete_;

  // Keeps track of when the scoping effort should next invalidate the scrollbar
  // and the frame area.
  int next_invalidate_after_;

 private:
  // A bit mask specifying area of the frame to invalidate.
  enum AreaToInvalidate {
    INVALIDATE_NOTHING      = 0,
    INVALIDATE_CONTENT_AREA = 1,
    INVALIDATE_SCROLLBAR    = 2,  // vertical scrollbar only.
    INVALIDATE_ALL          = 3   // both content area and the scrollbar.
  };

  // Invalidates a certain area within the frame.
  void InvalidateArea(AreaToInvalidate area);

  // Add a WebKit TextMatch-highlight marker to nodes in a range.
  void AddMarker(WebCore::Range* range);

  // Returns the ordinal of the first match in the frame specified. This
  // function enumerates the frames, starting with the main frame and up to (but
  // not including) the frame passed in as a parameter and counts how many
  // matches have been found.
  int OrdinalOfFirstMatchForFrame(WebFrameImpl* frame) const;

  // Determines whether the scoping effort is required for a particular frame.
  // It is not necessary if the frame is invisible, for example, or if this
  // is a repeat search that already returned nothing last time the same prefix
  // was searched.
  bool ShouldScopeMatches(FindInPageRequest request);

  // Only for test_shell
  int PendingFrameUnloadEventCount() const;

  // Determines whether to invalidate the content area and scrollbar.
  void InvalidateIfNecessary();

  void InternalLoadRequest(const WebRequest* request,
                           const WebCore::SubstituteData& data,
                           bool replace);

  // Clears the map of password listeners.
  void ClearPasswordListeners();

  // In "printing" mode. Used as a state check.
  bool printing_;

  // For each printed page, the view of the document in pixels.
  Vector<WebCore::IntRect> pages_;

  // The input fields that are interested in edit events and their associated
  // listeners.
  typedef HashMap<RefPtr<WebCore::HTMLInputElement>,
      webkit_glue::PasswordAutocompleteListener*> PasswordListenerMap;
  PasswordListenerMap password_listeners_;

  DISALLOW_COPY_AND_ASSIGN(WebFrameImpl);
};

#endif  // WEBKIT_GLUE_WEBFRAME_IMPL_H_
