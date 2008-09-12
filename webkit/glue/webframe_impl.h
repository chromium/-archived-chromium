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

#include <string>

#include "base/basictypes.h"
#include "base/gfx/platform_canvas.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "webkit/glue/webdatasource_impl.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webframeloaderclient_impl.h"
#include "webkit/glue/webplugin_delegate.h"
#include "webkit/glue/webview_delegate.h"

#pragma warning(push, 0)
#include "ResourceHandleClient.h"
#include "Frame.h"
#include "PlatformString.h"
#pragma warning(pop)

class AltErrorPageResourceFetcher;
class WebErrorImpl;
class WebHistoryItemImpl;
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
class WebFrameImpl : public WebFrame {
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
  virtual bool GetPreviousState(GURL* url, std::wstring* title,
                                std::string* history_state) const;
  virtual bool GetCurrentState(GURL* url, std::wstring* title,
                               std::string* history_state) const;
  virtual bool HasCurrentState() const;
  virtual GURL GetURL() const;
  virtual GURL GetFavIconURL() const;
  virtual GURL GetOSDDURL() const;
  virtual WebDataSource* GetDataSource() const;
  virtual WebDataSource* GetProvisionalDataSource() const;
  virtual void StopLoading();
  virtual WebFrame* GetOpener() const;
  virtual WebFrame* GetParent() const;
  virtual WebFrame* GetChildFrame(const std::wstring& xpath) const;
  virtual WebView* GetView() const;
  virtual gfx::BitmapPlatformDevice CaptureImage(bool scroll_to_zero);

  // This method calls createRuntimeObject (in KJS::Bindings::Instance), which
  // increments the refcount of the NPObject passed in.
  virtual void BindToWindowObject(const std::wstring& name, NPObject* object);
  virtual void CallJSGC();

  virtual void* GetFrameImplementation() { return frame(); }

  virtual void GetContentAsPlainText(int max_chars, std::wstring* text) const;
  virtual bool Find(const FindInPageRequest& request,
                    bool wrap_within_frame,
                    gfx::Rect* selection_rect);
  virtual bool FindNext(const FindInPageRequest& request,
                        bool wrap_within_frame);
  virtual void StopFinding();
  virtual void ScopeStringMatches(FindInPageRequest request, bool reset);
  virtual void CancelPendingScopingEffort();
  virtual void ResetMatchCount();
  virtual bool Visible();
  virtual void SelectAll();
  virtual void Copy();
  virtual void Cut();
  virtual void Paste();
  virtual void Replace(const std::wstring& text);
  virtual void Delete();
  virtual void Undo();
  virtual void Redo();
  virtual void ClearSelection();

  virtual void SetInViewSourceMode(bool enable);

  virtual bool GetInViewSourceMode() const;

  virtual void DidReceiveData(WebCore::DocumentLoader* loader,
                              const char* data,
                              int length);
  virtual void DidFail(const WebCore::ResourceError&, bool was_provisional);

  virtual std::wstring GetName();

  virtual WebTextInput* GetTextInput();

  virtual bool ExecuteCoreCommandByName(const std::string& name, const std::string& value);

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
  virtual bool SpoolPage(int page,
                         PlatformContextSkia* context);

  // Reformats this frame for printing or for screen display, depending on
  // |printing| flag. Acts recursively on inner frames.
  // Note: It fails if the main frame failed to load. It will succeed even if a
  // child frame failed to load.
  void SetPrinting(bool printing, float page_width_min, float page_width_max);

  void CreateChildFrame(const WebCore::FrameLoadRequest&,
                        WebCore::HTMLFrameOwnerElement* owner_element,
                        bool allows_scrolling, int margin_width,
                        int margin_height, WebCore::Frame*& new_frame);

  // WebFrameImpl
  void Layout();
  void Paint(gfx::PlatformCanvas* canvas, const gfx::Rect& rect);

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
    return frame_.get();
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

  // Gets the tickmarks for drawing on the scrollbars of a particular frame.
  const Vector<RefPtr<WebCore::Range> >& tickmarks() const {
    return tickmarks_;
  }

  // Returns whether a range representing a tickmark should be highlighted.
  // We use this to avoid highlighting ranges that are currently hidden.
  static bool RangeShouldBeHighlighted(WebCore::Range* range);

  const WebCore::Node* inspected_node() const {
    return inspected_node_;
  }

  void selectNodeFromInspector(WebCore::Node* node);

  // Returns which frame has an active tickmark. This function should only be
  // called on the main frame, as it is the only frame keeping track. Returned
  // value can be NULL if no frame has an active tickmark.
  const WebFrameImpl* active_tickmark_frame() const {
    return active_tickmark_frame_;
  }

  // Returns the index of the active tickmark for this frame.
  size_t active_tickmark_index() const {
    return active_tickmark_;
  }

  // Sets whether the WebFrameImpl allows its document to be scrolled.
  // If the parameter is true, allow the document to be scrolled.
  // Otherwise, disallow scrolling
  void SetAllowsScrolling(bool flag);

  // Returns true if the frame CSS is in "printing" mode.
  bool printing() const { return printing_; }

  virtual bool HasUnloadListener();
  virtual bool IsReloadAllowingStaleData() const;

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

  // Holding a reference back to the WebViewImpl is necessary to ensure that
  // its HWND is not destroyed before all of the WebCore::Widgets, which refer
  // to the WebViewImpl's HWND as their containingWindow.  However, this ref
  // creates a cycle between the WebViewImpl and the top-most WebFrameImpl.  We
  // break this cycle in our Closing method.
  scoped_refptr<WebViewImpl> webview_impl_;

  // The WebCore frame associated with this frame.  MAY BE NULL if the frame
  // has been detached from the DOM.
  WTF::RefPtr<WebCore::Frame> frame_;

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

  // Frame construction parameters
  bool allows_scrolling_;
  int margin_width_;
  int margin_height_;

  // Handling requests from TextInputController on this frame.
  scoped_ptr<WebTextInputImpl> webtextinput_impl_;

  // This vector maintains a list of Ranges representing locations for search
  // string matches that were found in the frame during a FindInPage operation.
  Vector<RefPtr<WebCore::Range> > tickmarks_;

  // The node selected in the web inspector. Used for highlighting it on the page.
  WebCore::Node* inspected_node_;

  // The index of the active tickmark for the current frame.
  size_t active_tickmark_;

  // This flag is used by the scoping effort to determine if we need to figure
  // out which rectangle is the active tickmark. Once we find the active
  // rectangle we clear this flag.
  bool locating_active_rect_;

  // This rectangle is used during the scoping effort to figure out what rect
  // got selected during the Find operation. In other words, first the Find
  // operation iterates to the next match and then scoping will happen for all
  // matches. When we encounter this rectangle during scoping we mark that
  // tickmark as active (see active_tickmark_). This avoids having to iterate
  // through a potentially very large tickmark vector to see which hit is
  // active. An empty rect means that we don't know the rectangle for the
  // selection (ie. because the selection controller couldn't tell us what the
  // bounding box for it is) and the scoping effort should mark the first
  // match it finds as the active rectangle.
  WebCore::IntRect active_selection_rect_;

  // This range represents the range that got selected during the Find or
  // FindNext operation. We will set this range as the selection (unless the
  // user selects something between Find/FindNext operations) so that we can
  // continue from where we left off.
  RefPtr<WebCore::Range> last_active_range_;

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
  int total_matchcount_;

  // A way for the main frame to keep track of which frame has an active
  // tickmark. Should be NULL for all other frames.
  WebFrameImpl* active_tickmark_frame_;

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

  // This is a factory for creating cancelable tasks for this frame that run
  // asynchronously in order to scope string matches during a find operation.
  ScopedRunnableMethodFactory<WebFrameImpl> scope_matches_factory_;

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

  // Invalidates the tickmark area represented by the range passed in.
  void InvalidateTickmark(RefPtr<WebCore::Range> tickmark);

  // Returns the ordinal of the first match in the frame specified. This
  // function enumerates the frames, starting with the main frame and up to (but
  // not including) the frame passed in as a parameter and counts how many
  // tickmarks there are.
  int OrdinalOfFirstMatchForFrame(WebFrameImpl* frame) const;

  // Determines whether the scoping effort is required for a particular frame.
  // It is not necessary if the frame is invisible, for example, or if this
  // is a repeat search that already returned nothing last time the same prefix
  // was searched.
  bool ShouldScopeMatches(FindInPageRequest request);

  // Determines whether to invalidate the content area and scrollbar.
  void InvalidateIfNecessary();

  void InternalLoadRequest(const WebRequest* request,
                           const WebCore::SubstituteData& data,
                           bool replace);

  // In "printing" mode. Used as a state check.
  bool printing_;

  // For each printed page, the view of the document in pixels.
  Vector<WebCore::IntRect> pages_;

  DISALLOW_COPY_AND_ASSIGN(WebFrameImpl);
};

#endif  // WEBKIT_GLUE_WEBFRAME_IMPL_H_
