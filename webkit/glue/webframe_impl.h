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

class ChromePrintContext;
class WebDataSourceImpl;
class WebPluginDelegate;
class WebView;
class WebViewImpl;
class WebTextInput;
class WebTextInputImpl;

namespace gfx {
class BitmapPlatformDevice;
}

namespace WebCore {
class Frame;
class FrameView;
class HistoryItem;
class KURL;
class Node;
class Range;
class SubstituteData;
struct WindowFeatures;
}

namespace webkit_glue {
class AltErrorPageResourceFetcher;
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
  virtual void Reload();
  virtual void LoadRequest(const WebKit::WebURLRequest& request);
  virtual void LoadHistoryItem(const WebKit::WebHistoryItem& item);
  virtual void LoadData(
      const WebKit::WebData& data,
      const WebKit::WebString& mime_type,
      const WebKit::WebString& text_encoding,
      const WebKit::WebURL& base_url,
      const WebKit::WebURL& unreachable_url = WebKit::WebURL(),
      bool replace = false);
  virtual void LoadHTMLString(
      const WebKit::WebData& data,
      const WebKit::WebURL& base_url,
      const WebKit::WebURL& unreachable_url = WebKit::WebURL(),
      bool replace = false);
  virtual void LoadAlternateHTMLErrorPage(
      const WebKit::WebURLRequest& request,
      const WebKit::WebURLError& error,
      const GURL& error_page_url,
      bool replace,
      const GURL& fake_url);
  virtual void DispatchWillSendRequest(WebKit::WebURLRequest* request);
  virtual void ExecuteScript(const WebKit::WebScriptSource& source);
  virtual void ExecuteScriptInNewContext(
      const WebKit::WebScriptSource* sources, int num_sources);
  virtual bool InsertCSSStyles(const std::string& css);
  virtual WebKit::WebHistoryItem GetPreviousHistoryItem() const;
  virtual WebKit::WebHistoryItem GetCurrentHistoryItem() const;
  virtual GURL GetURL() const;
  virtual GURL GetFavIconURL() const;
  virtual GURL GetOSDDURL() const;
  virtual int GetContentsPreferredWidth() const;
  virtual WebKit::WebDataSource* GetDataSource() const;
  virtual WebKit::WebDataSource* GetProvisionalDataSource() const;
  virtual void StopLoading();
  virtual WebFrame* GetOpener() const;
  virtual WebFrame* GetParent() const;
  virtual WebFrame* GetTop() const;
  virtual WebFrame* GetChildFrame(const std::wstring& xpath) const;
  virtual WebView* GetView() const;
  virtual void GetForms(std::vector<WebKit::WebForm>* forms) const;
  virtual std::string GetSecurityOrigin() const;

  // This method calls createRuntimeObject (in KJS::Bindings::Instance), which
  // increments the refcount of the NPObject passed in.
  virtual void BindToWindowObject(const std::wstring& name, NPObject* object);
  virtual void CallJSGC();

  virtual void GrantUniversalAccess();

  virtual NPObject* GetWindowNPObject();

#if USE(V8)
  // Returns the V8 context for this frame, or an empty handle if there is
  // none.
  virtual v8::Local<v8::Context> GetScriptContext();
#endif

  virtual void GetContentAsPlainText(int max_chars, std::wstring* text) const;
  virtual bool Find(
      int request_id,
      const string16& search_text,
      const WebKit::WebFindOptions& options,
      bool wrap_within_frame,
      WebKit::WebRect* selection_rect);
  virtual void StopFinding(bool clear_selection);
  virtual void ScopeStringMatches(
      int request_id,
      const string16& search_text,
      const WebKit::WebFindOptions& options,
      bool reset);
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
  virtual bool HasSelection();
  virtual std::string GetSelection(bool as_html);
  virtual std::string GetFullPageHtml();

  virtual void SetInViewSourceMode(bool enable);

  virtual bool GetInViewSourceMode() const;

  virtual void DidReceiveData(WebCore::DocumentLoader* loader,
                              const char* data,
                              int length);
  virtual void DidFail(const WebCore::ResourceError&, bool was_provisional);

  virtual std::wstring GetName();

  virtual WebTextInput* GetTextInput();

  virtual bool ExecuteEditCommandByName(const std::string& name,
                                        const std::string& value);
  virtual bool IsEditCommandEnabled(const std::string& name);

  virtual void AddMessageToConsole(const WebKit::WebConsoleMessage&);

  virtual void ClosePage();

  virtual WebKit::WebSize ScrollOffset() const;

  virtual int PrintBegin(const WebKit::WebSize& page_size);
  virtual float PrintPage(int page, WebKit::WebCanvas* canvas);
  virtual void PrintEnd();

  PassRefPtr<WebCore::Frame> CreateChildFrame(
      const WebCore::FrameLoadRequest&,
      WebCore::HTMLFrameOwnerElement* owner_element);

  // WebFrameImpl
  void Layout();
  void Paint(skia::PlatformCanvas* canvas, const WebKit::WebRect& rect);

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

  WebViewImpl* GetWebViewImpl() const;

  WebCore::FrameView* frameview() const {
    return frame_ ? frame_->view() : NULL;
  }

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
  // Otherwise, disallow scrolling.
  void SetAllowsScrolling(bool flag);

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

  // See WebFrame.h for details.
  virtual void IncreaseMatchCount(int count, int request_id);
  virtual void ReportFindInPageSelection(const WebKit::WebRect& selection_rect,
                                         int active_match_ordinal,
                                         int request_id);

  // Resource fetcher for downloading an alternate DNS error page.
  scoped_ptr<webkit_glue::AltErrorPageResourceFetcher> alt_error_page_fetcher_;

  // Used to check for leaks of this object.
  static int live_object_count_;

  WebFrameLoaderClient frame_loader_client_;

  // This is a factory for creating cancelable tasks for this frame that run
  // asynchronously in order to scope string matches during a find operation.
  ScopedRunnableMethodFactory<WebFrameImpl> scope_matches_factory_;

  // This is a weak pointer to our corresponding WebCore frame.  A reference to
  // ourselves is held while frame_ is valid.  See our Closing method.
  WebCore::Frame* frame_;

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
  string16 last_search_string_;

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
  void AddMarker(WebCore::Range* range, bool active_match);

  // Sets the markers within a range as active or inactive.
  void SetMarkerActive(WebCore::Range* range, bool active);

  // Returns the ordinal of the first match in the frame specified. This
  // function enumerates the frames, starting with the main frame and up to (but
  // not including) the frame passed in as a parameter and counts how many
  // matches have been found.
  int OrdinalOfFirstMatchForFrame(WebFrameImpl* frame) const;

  // Determines whether the scoping effort is required for a particular frame.
  // It is not necessary if the frame is invisible, for example, or if this
  // is a repeat search that already returned nothing last time the same prefix
  // was searched.
  bool ShouldScopeMatches(const string16& search_text);

  // Only for test_shell
  int PendingFrameUnloadEventCount() const;

  // Determines whether to invalidate the content area and scrollbar.
  void InvalidateIfNecessary();

  // Clears the map of password listeners.
  void ClearPasswordListeners();

  void LoadJavaScriptURL(const WebCore::KURL& url);

  // Valid between calls to BeginPrint() and EndPrint(). Containts the print
  // information. Is used by PrintPage().
  scoped_ptr<ChromePrintContext> print_context_;

  // The input fields that are interested in edit events and their associated
  // listeners.
  typedef HashMap<RefPtr<WebCore::HTMLInputElement>,
      webkit_glue::PasswordAutocompleteListener*> PasswordListenerMap;
  PasswordListenerMap password_listeners_;

  DISALLOW_COPY_AND_ASSIGN(WebFrameImpl);
};

#endif  // WEBKIT_GLUE_WEBFRAME_IMPL_H_
