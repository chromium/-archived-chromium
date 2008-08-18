/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
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

// How ownership works
// -------------------
//
// Big oh represents a refcounted relationship: owner O--- ownee
//
// WebView (for the toplevel frame only)
//    O
//    |
//    O
// WebFrame <-------------------------- FrameLoader
//        O  (via WebFrameLoaderClient)     ||
//        |                                 ||
//        +------------------------------+  ||
//                                       |  ||
// FrameView O-------------------------O Frame
//
// FrameLoader and Frame are formerly one object that was split apart because
// it got too big. They basically have the same lifetime, hence the double line.
//
// WebFrame is refcounted and has one ref on behalf of the FrameLoader/Frame
// and, in the case of the toplevel frame, one more for the WebView. This is
// not a normal reference counted pointer because that would require changing
// WebKit code that we don't control. Instead, it is created with this ref
// initially and it is removed when the FrameLoader is getting destroyed.
//
// WebFrames are created in two places, first in WebViewImpl when the root
// frame is created, and second in WebFrame::CreateChildFrame when sub-frames
// are created. WebKit will hook up this object to the FrameLoader/Frame
// and the refcount will be correct.
//
// How frames are destroyed
// ------------------------
//
// The main frame is never destroyed and is re-used. The FrameLoader is
// re-used and a reference is also kept by the WebView, so the root frame will
// generally have a refcount of 2.
//
// When frame content is replaced, all subframes are destroyed. This happens
// in FrameLoader::detachFromParent for each suframe. Here, we first clear
// the view in the Frame, breaking the circular cycle between Frame and
// FrameView. Then it calls detachedFromParent4 on the FrameLoaderClient.
//
// The FrameLoaderClient is implemented by WebFrameLoaderClient, which is
// an object owned by WebFrame. It calls WebFrame::Closing which causes
// WebFrame to release its references to Frame, generally releasing it.
//
// Frame going away causes the FrameLoader to get deleted. In FrameLoader's
// destructor it notifies its client with frameLoaderDestroyed. This derefs
// WebView and will cause it to be deleted (unless an external someone is also
// holding a reference).

#include "config.h"

#include <algorithm>
#include <string>

#pragma warning(push, 0)
#include "HTMLFormElement.h"  // need this before Document.h
#include "Chrome.h"
#include "Document.h"
#include "DocumentFragment.h"  // Only needed for ReplaceSelectionCommand.h :(
#include "DocumentLoader.h"
#include "Editor.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoadRequest.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "FrameWin.h"
#include "GraphicsContext.h"
#include "HTMLHeadElement.h"
#include "HTMLLinkElement.h"
#include "HTMLNames.h"
#include "HistoryItem.h"
#include "markup.h"
#include "Page.h"
#include "PlatformScrollBar.h"
#include "RenderFrame.h"
#include "RenderWidget.h"
#include "ReplaceSelectionCommand.h"
#include "ResourceHandle.h"
#include "ResourceHandleWin.h"
#include "ResourceRequest.h"
#include "SelectionController.h"
#include "Settings.h"
#include "SkiaUtils.h"
#include "SubstituteData.h"
#include "TextIterator.h"
#include "TextAffinity.h"
#include "XPathResult.h"

#pragma warning(pop)

#undef LOG
#include "base/gfx/bitmap_platform_device_win.h"
#include "base/gfx/rect.h"
#include "base/gfx/platform_canvas_win.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "base/time.h"
#include "net/base/net_errors.h"
#include "webkit/glue/dom_operations.h"
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/alt_error_page_resource_fetcher.h"
#include "webkit/glue/webdocumentloader_impl.h"
#include "webkit/glue/weberror_impl.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webhistoryitem_impl.h"
#include "webkit/glue/webtextinput_impl.h"
#include "webkit/glue/webview_impl.h"
#include "webkit/port/page/ChromeClientWin.h"
#include "webkit/port/platform/WidgetClientWin.h"

using WebCore::ChromeClientWin;
using WebCore::Color;
using WebCore::DeprecatedString;
using WebCore::Document;
using WebCore::DocumentLoader;
using WebCore::ExceptionCode;
using WebCore::GraphicsContext;
using WebCore::HTMLFrameOwnerElement;
using WebCore::Frame;
using WebCore::FrameLoader;
using WebCore::FrameLoadRequest;
using WebCore::FrameLoadType;
using WebCore::FrameTree;
using WebCore::FrameView;
using WebCore::HistoryItem;
using WebCore::HTMLFrameElementBase;
using WebCore::IntRect;
using WebCore::KURL;
using WebCore::Node;
using WebCore::PlatformScrollbar;
using WebCore::Range;
using WebCore::ReloadIgnoringCacheData;
using WebCore::RenderObject;
using WebCore::ResourceError;
using WebCore::ResourceHandle;
using WebCore::ResourceRequest;
using WebCore::Selection;
using WebCore::SharedBuffer;
using WebCore::String;
using WebCore::SubstituteData;
using WebCore::TextIterator;
using WebCore::VisiblePosition;
using WebCore::WidgetClientWin;
using WebCore::XPathResult;

static const wchar_t* const kWebFrameActiveCount = L"WebFrameActiveCount";

static const char* const kOSDType = "application/opensearchdescription+xml";
static const char* const kOSDRel = "search";

// The separator between frames when the frames are converted to plain text.
static const wchar_t kFrameSeparator[] = L"\n\n";
static const int kFrameSeparatorLen = arraysize(kFrameSeparator) - 1;

// Backend for GetContentAsPlainText, this is a recursive function that gets
// the text for the current frame and all of its subframes. It will append
// the text of each frame in turn to the |output| up to |max_chars| length.
//
// The |frame| must be non-NULL.
static void FrameContentAsPlainText(int max_chars, Frame* frame,
                                    std::wstring* output) {
  Document* doc = frame->document();
  if (!doc)
    return;

  // Select the document body.
  RefPtr<Range> range(doc->createRange());
  ExceptionCode exception = 0;
  range->selectNodeContents(doc->body(), exception);

  if (exception == 0) {
    // The text iterator will walk nodes giving us text. This is similar to
    // the plainText() function in TextIterator.h, but we implement the maximum
    // size and also copy the results directly into a wstring, avoiding the
    // string conversion.
    for (TextIterator it(range.get()); !it.atEnd(); it.advance()) {
      const wchar_t* chars = reinterpret_cast<const wchar_t*>(it.characters());
      if (!chars && it.length() != 0) {
        // It appears from crash reports that an iterator can get into a state
        // where the character count is nonempty but the character pointer is
        // NULL. advance()ing it will then just add that many to the NULL
        // pointer which won't be caught in a NULL check but will crash.
        //
        // A NULL pointer and 0 length is common for some nodes.
        //
        // IF YOU CATCH THIS IN A DEBUGGER please let brettw know. We don't
        // currently understand the conditions for this to occur. Ideally, the
        // iterators would never get into the condition so we should fix them
        // if we can.
        NOTREACHED();
        break;
      }
      int to_append = std::min(it.length(),
                               max_chars - static_cast<int>(output->size()));
      output->append(chars, to_append);
      if (output->size() >= static_cast<size_t>(max_chars))
        return;  // Filled up the buffer.
    }
  }

  // Recursively walk the children.
  FrameTree* frame_tree = frame->tree();
  Frame* cur_child = frame_tree->firstChild();
  for (Frame* cur_child = frame_tree->firstChild(); cur_child;
       cur_child = cur_child->tree()->nextSibling()) {
    // Make sure the frame separator won't fill up the buffer, and give up if
    // it will. The danger is if the separator will make the buffer longer than
    // max_chars. This will cause the computation above:
    //   max_chars - output->size()
    // to be a negative number which will crash when the subframe is added.
    if (static_cast<int>(output->size()) >= max_chars - kFrameSeparatorLen)
      return;

    output->append(kFrameSeparator, kFrameSeparatorLen);
    FrameContentAsPlainText(max_chars, cur_child, output);
    if (output->size() >= static_cast<size_t>(max_chars))
      return;  // Filled up the buffer.
  }
}

// WebFrameImpl ----------------------------------------------------------------

int WebFrameImpl::live_object_count_ = 0;

WebFrameImpl::WebFrameImpl()
// Don't complain about using "this" in initializer list.
#pragma warning(disable: 4355)
  : frame_loader_client_(this),
    scope_matches_factory_(this),
#pragma warning(default: 4355)
    currently_loading_request_(NULL),
    plugin_delegate_(NULL),
    allows_scrolling_(true),
    margin_width_(-1),
    margin_height_(-1),
    last_match_count_(-1),
    total_matchcount_(-1),
    inspected_node_(NULL),
    active_tickmark_frame_(NULL),
    active_tickmark_(WidgetClientWin::kNoTickmark),
    last_active_range_(NULL),
    frames_scoping_count_(-1),
    scoping_complete_(false),
    next_invalidate_after_(0),
    printing_(false) {
  StatsCounter(kWebFrameActiveCount).Increment();
  live_object_count_++;
}

WebFrameImpl::~WebFrameImpl() {
  StatsCounter(kWebFrameActiveCount).Decrement();
  live_object_count_--;

  CancelPendingScopingEffort();
}

// WebFrame -------------------------------------------------------------------

void WebFrameImpl::InitMainFrame(WebViewImpl* webview_impl) {
  webview_impl_ = webview_impl;  // owning ref

  Frame* frame = new Frame(webview_impl_->page(), 0, &frame_loader_client_);

  // Add reference on behalf of FrameLoader.  See comments in
  // WebFrameLoaderClient::frameLoaderDestroyed for more info.
  AddRef();

  frame_ = frame;

  // We must call init() after frame_ is assigned because it is referenced
  // during init().
  frame_->init();
}

void WebFrameImpl::LoadRequest(WebRequest* request) {
  SubstituteData data;
  InternalLoadRequest(request, data, false);
}

void WebFrameImpl::InternalLoadRequest(const WebRequest* request,
                                       const SubstituteData& data,
                                       bool replace) {
  const WebRequestImpl* request_impl =
      static_cast<const WebRequestImpl*>(request);

  const ResourceRequest& resource_request =
      request_impl->frame_load_request().resourceRequest();

  // Special-case javascript URLs.  Do not interrupt the existing load when
  // asked to load a javascript URL unless the script generates a result.
  // We can't just use FrameLoader::executeIfJavaScriptURL because it doesn't
  // handle redirects properly.
  const KURL& kurl = resource_request.url();
  if (!data.isValid() && kurl.protocol() == "javascript") {
    // Don't attempt to reload javascript URLs.
    if (resource_request.cachePolicy() == ReloadIgnoringCacheData)
      return;

    // We can't load a javascript: URL if there is no Document!
    if (!frame_->document())
      return;

    // TODO(darin): Is this the best API to use here?  It works and seems good,
    // but will it change out from under us?
    DeprecatedString script =
        KURL::decode_string(kurl.deprecatedString().mid(sizeof("javascript:")-1));
    bool succ = false;
    WebCore::String value =
        frame_->loader()->executeScript(script, &succ, true);
    if (succ && !frame_->loader()->isScheduledLocationChangePending()) {
      // TODO(darin): We need to figure out how to represent this in session
      // history.  Hint: don't re-eval script when the user or script navigates
      // back-n-forth (instead store the script result somewhere).
      LoadDocumentData(kurl, value, String("text/html"), String());
    }
    return;
  }

  StopLoading();  // make sure existing activity stops

  // Keep track of the request temporarily.  This is effectively a way of
  // passing the request to callbacks that may need it.  See
  // WebFrameLoaderClient::createDocumentLoader.
  currently_loading_request_ = request;

  // If we have a current datasource, save the request info on it immediately.
  // This is because WebCore may not actually initiate a load on the toplevel
  // frame for some subframe navigations, so we want to update its request.
  WebDataSourceImpl* datasource = GetDataSourceImpl();
  if (datasource)
    CacheCurrentRequestInfo(datasource);

  if (data.isValid()) {
    frame_->loader()->load(resource_request, data);
    if (replace) {
      // Do this to force WebKit to treat the load as replacing the currently
      // loaded page.
      frame_->loader()->setReplacing();
    }
  } else if (request_impl->history_item()) {
    // Use the history item if we have one, otherwise fall back to standard
    // load.
    RefPtr<HistoryItem> current_item = frame_->loader()->currentHistoryItem();

    // If there is no current_item, which happens when we are navigating in
    // session history after a crash, we need to manufacture one otherwise
    // WebKit hoarks. This is probably the wrong thing to do, but it seems to
    // work.
    if (!current_item) {
      current_item = new HistoryItem(KURL("about:blank"), "");
      frame_->loader()->setCurrentHistoryItem(current_item);
      frame_->page()->backForwardList()->setCurrentItem(current_item.get());

      // Mark the item as fake, so that we don't attempt to save its state and
      // end up with about:blank in the navigation history.
      frame_->page()->backForwardList()->setCurrentItemFake(true);
    }

    frame_->loader()->goToItem(request_impl->history_item().get(),
                               WebCore::FrameLoadTypeIndexedBackForward);
  } else if (resource_request.cachePolicy() == ReloadIgnoringCacheData) {
    frame_->loader()->reload();
  } else {
    frame_->loader()->load(resource_request);
  }

  currently_loading_request_ = NULL;
}

void WebFrameImpl::LoadHTMLString(const std::string& html_text,
                                  const GURL& base_url) {
  WebRequestImpl request(base_url);
  LoadAlternateHTMLString(&request, html_text, GURL(), false);
}

void WebFrameImpl::LoadAlternateHTMLString(const WebRequest* request,
                                           const std::string& html_text,
                                           const GURL& display_url,
                                           bool replace) {
  int len = static_cast<int>(html_text.size());
  RefPtr<SharedBuffer> buf(new SharedBuffer(html_text.data(), len));

  SubstituteData subst_data(
      buf, String("text/html"), String("UTF-8"),
      webkit_glue::GURLToKURL(display_url));
  DCHECK(subst_data.isValid());

  InternalLoadRequest(request, subst_data, replace);
}

GURL WebFrameImpl::GetURL() const {
  const WebDataSource* ds = GetDataSource();
  if (!ds)
    return GURL();
  return ds->GetRequest().GetURL();
}

GURL WebFrameImpl::GetFavIconURL() const {
  WebCore::FrameLoader* frame_loader = frame_->loader();
  // The URL to the favicon may be in the header. As such, only
  // ask the loader for the favicon if it's finished loading.
  if (frame_loader->state() == WebCore::FrameStateComplete) {
    const KURL& url = frame_loader->iconURL();
    if (!url.isEmpty()) {
      return webkit_glue::KURLToGURL(url);
    }
  }
  return GURL();
}

GURL WebFrameImpl::GetOSDDURL() const {
  WebCore::FrameLoader* frame_loader = frame_->loader();
  if (frame_loader->state() == WebCore::FrameStateComplete &&
      frame_->document() && frame_->document()->head() &&
      !frame_->tree()->parent()) {
    WebCore::HTMLHeadElement* head = frame_->document()->head();
    if (head) {
      RefPtr<WebCore::HTMLCollection> children = head->children();
      for (Node* child = children->firstItem(); child != NULL;
           child = children->nextItem()) {
        WebCore::HTMLLinkElement* link_element =
            webkit_glue::CastHTMLElement<WebCore::HTMLLinkElement>(
                child, WebCore::HTMLNames::linkTag);
        if (link_element && link_element->type() == kOSDType &&
            link_element->rel() == kOSDRel && !link_element->href().isEmpty()) {
          return GURL(link_element->href().charactersWithNullTermination());
        }
      }
    }
  }
  return GURL();
}

bool WebFrameImpl::GetPreviousState(GURL* url, std::wstring* title,
                                    std::string* history_state) const {
  // We use the previous item here because documentState (filled-out forms)
  // only get saved to history when it becomes the previous item.  The caller
  // is expected to query the history state after a navigation occurs, after
  // the desired history item has become the previous entry.
  if (frame_->page()->backForwardList()->isPreviousItemFake())
    return false;

  RefPtr<HistoryItem> item = frame_->page()->backForwardList()->previousItem();
  if (!item)
    return false;

  static StatsCounterTimer history_timer(L"GetHistoryTimer");
  StatsScope<StatsCounterTimer> history_scope(history_timer);

  webkit_glue::HistoryItemToString(item, history_state);
  *url = webkit_glue::KURLToGURL(item->url());
  *title = webkit_glue::StringToStdWString(item->title());

  return true;
}

bool WebFrameImpl::GetCurrentState(GURL* url, std::wstring* title,
                                   std::string* state) const {
  if (frame_->loader())
    frame_->loader()->saveDocumentAndScrollState();
  RefPtr<HistoryItem> item = frame_->page()->backForwardList()->currentItem();
  if (!item)
    return false;

  webkit_glue::HistoryItemToString(item, state);
  *url = webkit_glue::KURLToGURL(item->url());
  *title = webkit_glue::StringToStdWString(item->title());

  return true;
}

bool WebFrameImpl::HasCurrentState() const {
  return frame_->page()->backForwardList()->currentItem() != NULL;
}

void WebFrameImpl::LoadDocumentData(const KURL& base_url,
                                    const String& data,
                                    const String& mime_type,
                                    const String& charset) {
  // TODO(darin): This is wrong.  We need to re-cast this in terms of a call to
  // one of the FrameLoader::load(...) methods.  Else, WebCore will be angry!!

  // Requiring a base_url here seems like a good idea for security reasons.
  ASSERT(!base_url.isEmpty());
  ASSERT(!mime_type.isEmpty());

  StopLoading();

  // Reset any pre-existing scroll offset
  frameview()->setContentsPos(0, 0);

  // Make sure the correct document type is constructed.
  frame_->loader()->setResponseMIMEType(mime_type);

  // TODO(darin): Inform the FrameLoader of the charset somehow.

  frame_->loader()->begin(base_url);
  frame_->loader()->write(data);
  frame_->loader()->end();
}

void WebFrameImpl::set_currently_loading_history_item(
    WebHistoryItemImpl* item) {
  currently_loading_history_item_ = item;
}


static WebDataSource* DataSourceForDocLoader(DocumentLoader* loader) {
  return (loader ?
          static_cast<WebDocumentLoaderImpl*>(loader)->GetDataSource() : NULL);
}

WebDataSource* WebFrameImpl::GetDataSource() const {
  if (!frame_->loader())
    return NULL;
  return DataSourceForDocLoader(frame_->loader()->documentLoader());
}

WebDataSourceImpl* WebFrameImpl::GetDataSourceImpl() const {
  return static_cast<WebDataSourceImpl*>(GetDataSource());
}

WebDataSource* WebFrameImpl::GetProvisionalDataSource() const {
  FrameLoader* frame_loader = frame_->loader();
  if (!frame_loader)
    return NULL;

  // We regard the policy document loader as still provisional.
  DocumentLoader* doc_loader = frame_loader->provisionalDocumentLoader();
  if (!doc_loader)
    doc_loader = frame_loader->policyDocumentLoader();

  return DataSourceForDocLoader(doc_loader);
}

WebDataSourceImpl* WebFrameImpl::GetProvisionalDataSourceImpl() const {
  return static_cast<WebDataSourceImpl*>(GetProvisionalDataSource());
}

void WebFrameImpl::CacheCurrentRequestInfo(WebDataSourceImpl* datasource) {
  // Cache our current request info on the data source.  It contains its
  // own requests, so the extra data needs to be transferred.
  scoped_refptr<WebRequest::ExtraData> extra;

  // Our extra data may come from a request issued via LoadRequest, or a
  // history navigation from WebCore.
  if (currently_loading_request_) {
    extra = currently_loading_request_->GetExtraData();
  } else if (currently_loading_history_item_) {
    extra = currently_loading_history_item_->GetExtraData();
    currently_loading_history_item_ = 0;
  }

  // We must only update this if it is valid, or the valid state will be lost.
  if (extra)
    datasource->SetExtraData(extra);
}

void WebFrameImpl::StopLoading() {
  if (!frame_)
    return;

  // TODO(darin): Figure out what we should really do here.  It seems like a
  // bug that FrameLoader::stopLoading doesn't call stopAllLoaders.
  frame_->loader()->stopAllLoaders();
  frame_->loader()->stopLoading(false);
}

WebFrame* WebFrameImpl::GetOpener() const {
  if (frame_) {
    Frame* opener = frame_->loader()->opener();
    if (opener)
      return FromFrame(opener);
  }
  return NULL;
}

WebFrame* WebFrameImpl::GetParent() const {
  if (frame_) {
    Frame *parent = frame_->tree()->parent();
    if (parent)
      return FromFrame(parent);
  }
  return NULL;
}

WebFrame* WebFrameImpl::GetChildFrame(const std::wstring& xpath) const {
  // xpath string can represent a frame deep down the tree (across multiple
  // frame DOMs).
  // Example, /html/body/table/tbody/tr/td/iframe\n/frameset/frame[0]
  // should break into 2 xpaths
  // /html/body/table/tbody/tr/td/iframe & /frameset/frame[0]

  if (xpath.empty())
    return NULL;

  std::wstring secondary;
  String xpath_str;

  std::wstring::size_type delim_pos = xpath.find_first_of(L'\n');
  if (delim_pos != std::wstring::npos) {
    std::wstring primary = xpath.substr(0, delim_pos);
    secondary = xpath.substr(delim_pos + 1);
    xpath_str = webkit_glue::StdWStringToString(primary);
  } else {
    xpath_str = webkit_glue::StdWStringToString(xpath);
  }

  Document* document = frame_->document();

  ExceptionCode ec = 0;
  PassRefPtr<XPathResult> xpath_result =
    document->evaluate(xpath_str,
    document,
    NULL, /* namespace */
    XPathResult::ORDERED_NODE_ITERATOR_TYPE,
    NULL, /* XPathResult object */
    ec);

  if (!xpath_result.get())
    return NULL;

  Node* node = xpath_result->iterateNext(ec);

  if (!node || !node->isFrameOwnerElement())
    return NULL;
  HTMLFrameOwnerElement* frame_element =
    static_cast<HTMLFrameOwnerElement*>(node);
  WebFrame* web_frame = FromFrame(frame_element->contentFrame());

  if (secondary.empty())
    return web_frame;
  else
    return web_frame->GetChildFrame(secondary);
}

void WebFrameImpl::SetInViewSourceMode(bool enable) {
  if (frame_)
    frame_->setInViewSourceMode(enable);
}

bool WebFrameImpl::GetInViewSourceMode() const {
  if (frame_)
    return frame_->inViewSourceMode();

  return false;
}

WebView* WebFrameImpl::GetView() const {
  return webview_impl_;
}

void WebFrameImpl::BindToWindowObject(const std::wstring& name,
                                      NPObject* object) {
    assert(frame_);
    if (!frame_ || !frame_->scriptBridge()->isEnabled())
      return;

    String key = webkit_glue::StdWStringToString(name);
    frame_->scriptBridge()->BindToWindowObject(frame_.get(), key, object);
}


// Call JavaScript garbage collection.
void WebFrameImpl::CallJSGC() {
    if (!frame_) return;
    if (!frame_->settings()->isJavaScriptEnabled()) return;
    frame_->scriptBridge()->CollectGarbage();
}


void WebFrameImpl::GetContentAsPlainText(int max_chars,
                                         std::wstring* text) const {
  text->clear();
  if (!frame_)
    return;

  FrameContentAsPlainText(max_chars, frame_.get(), text);
}

void WebFrameImpl::InvalidateArea(AreaToInvalidate area) {
  ASSERT(frame() && frame()->view());
  FrameView* view = frame()->view();

  if ((area & INVALIDATE_ALL) == INVALIDATE_ALL) {
    view->addToDirtyRegion(view->frameGeometry());
  } else {
    if ((area & INVALIDATE_CONTENT_AREA) == INVALIDATE_CONTENT_AREA) {
      IntRect content_area(view->x(),
                           view->y(),
                           view->visibleWidth(),
                           view->visibleHeight());
      view->addToDirtyRegion(content_area);
    }

    if ((area & INVALIDATE_SCROLLBAR) == INVALIDATE_SCROLLBAR) {
      // Invalidate the vertical scroll bar region for the view.
      IntRect scroll_bar_vert(view->x() + view->visibleWidth(),
                              view->y(),
                              PlatformScrollbar::verticalScrollbarWidth(),
                              view->visibleHeight());
      view->addToDirtyRegion(scroll_bar_vert);
    }
  }
}

void WebFrameImpl::InvalidateTickmark(RefPtr<WebCore::Range> tickmark) {
  ASSERT(frame() && frame()->view());
  FrameView* view = frame()->view();

  IntRect pos = tickmark->boundingBox();
  pos.move(-view->contentsX(), -view->contentsY());
  view->addToDirtyRegion(pos);
}

void WebFrameImpl::IncreaseMatchCount(int count, int request_id) {
  total_matchcount_ += count;

  // Update the UI with the latest findings.
  WebViewDelegate* webview_delegate = GetView()->GetDelegate();
  DCHECK(webview_delegate);
  if (webview_delegate)
    webview_delegate->ReportFindInPageMatchCount(total_matchcount_, request_id,
                                                 frames_scoping_count_ == 0);
}

void WebFrameImpl::ReportFindInPageSelection(const gfx::Rect& selection_rect,
                                             int active_match_ordinal,
                                             int request_id) {
  // Update the UI with the latest selection rect.
  WebViewDelegate* webview_delegate = GetView()->GetDelegate();
  DCHECK(webview_delegate);
  if (webview_delegate) {
    webview_delegate->ReportFindInPageSelection(
        request_id,
        OrdinalOfFirstMatchForFrame(this) + active_match_ordinal,
        selection_rect);
  }
}

void WebFrameImpl::ResetMatchCount() {
  total_matchcount_ = 0;
  frames_scoping_count_ = 0;
}

bool WebFrameImpl::Find(const FindInPageRequest& request,
                        bool wrap_within_frame,
                        gfx::Rect* selection_rect) {
  WebCore::String webcore_string =
      webkit_glue::StdWStringToString(request.search_string);

  // Starts the search from the current selection.
  bool start_in_selection = true;  // Policy. Can it be made configurable?

  // If the user has selected something since the last Find operation we want
  // to start from there. Otherwise, we start searching from where the last Find
  // operation left off (either a Find or a FindNext operation).
  Selection selection(frame()->selectionController()->selection());
  if (selection.isNone() && last_active_range_) {
    selection = Selection(last_active_range_.get());
    frame()->selectionController()->setSelection(selection);
  }

  DCHECK(frame() && frame()->view());
  bool found = frame()->findString(webcore_string, request.forward,
                                   request.match_case, wrap_within_frame,
                                   start_in_selection);
  if (found) {
    // We found something, so we can now query the selection for its position.
    Selection new_selection(frame()->selectionController()->selection());

    // If we thought we found something, but it couldn't be selected (perhaps
    // because it was marked -webkit-user-select: none), call it not-found so
    // we don't crash. See http://b/1169294.  This matches Safari's behavior,
    // including some oddities when selectable and un-selectable text are mixed
    // on a page: see http://b/1180007.
    if (new_selection.isNone()) {
      // Fall through to clean up selection and tickmarks.
      found = false;
    } else {
      last_active_range_ = new_selection.toRange();
      active_selection_rect_ = new_selection.toRange()->boundingBox();
      ClearSelection();  // We'll draw our own highlight for the active item.

      if (selection_rect) {
        gfx::Rect rect(
            frame()->view()->convertToContainingWindow(active_selection_rect_));
        rect.Offset(-frameview()->scrollOffset().width(),
                    -frameview()->scrollOffset().height());
        *selection_rect = rect;
      }
    }
  }

  if (!found) {
    active_selection_rect_ = IntRect();
    last_active_range_ = NULL;

    if (!tickmarks_.isEmpty()) {
      // Let the frame know that we found no matches.
      tickmarks_.clear();
      // Erase all previous tickmarks and highlighting.
      InvalidateArea(INVALIDATE_ALL);
    }
  }

  return found;
}

bool WebFrameImpl::FindNext(const FindInPageRequest& request,
                            bool wrap_within_frame) {
  if (tickmarks_.isEmpty())
    return false;

  // Save the old tickmark (if any). We will use this to invalidate the area
  // of the tickmark that becomes unselected.
  WebFrameImpl* const main_frame_impl =
    static_cast<WebFrameImpl*>(GetView()->GetMainFrame());
  WebFrameImpl* const active_frame = main_frame_impl->active_tickmark_frame_;
  RefPtr<WebCore::Range> old_tickmark = NULL;
  if (active_frame &&
      (active_frame->active_tickmark_ != WidgetClientWin::kNoTickmark)) {
    // When we get a reference to |old_tickmark| we can be in a state where
    // the |active_tickmark_| points outside the tickmark vector, possibly
    // during teardown of the frame. This doesn't reproduce normally, so if you
    // hit this during debugging, update issue http://b/1277569 with
    // reproduction steps - or contact the assignee. In release, we can ignore
    // this and continue on (and let |old_tickmark| be null).
    if (active_frame->active_tickmark_ >= active_frame->tickmarks_.size())
      NOTREACHED() << L"Active tickmark points outside the tickmark vector!";
    else
      old_tickmark = active_frame->tickmarks_[active_frame->active_tickmark_];
  }

  // See if we have another match to select, and select it.
  if (request.forward) {
    const bool at_end = (active_tickmark_ == (tickmarks_.size() - 1));
    if ((active_tickmark_ == WidgetClientWin::kNoTickmark) ||
        (at_end && wrap_within_frame)) {
      // Wrapping within a frame is only done for single frame pages. So when we
      // reach the end we go back to the beginning (or back to the end if
      // searching backwards).
      active_tickmark_ = 0;
    } else if (at_end) {
      return false;
    } else {
      ++active_tickmark_;
      DCHECK(active_tickmark_ < tickmarks_.size());
    }
  } else {
    const bool at_end = (active_tickmark_ == 0);
    if ((active_tickmark_ == WidgetClientWin::kNoTickmark) ||
        (at_end && wrap_within_frame)) {
      // Wrapping within a frame is not done for multi-frame pages, but if no
      // tickmark is active we still need to set the index to the end so that
      // we don't skip the frame during FindNext when searching backwards.
      active_tickmark_ = tickmarks_.size() - 1;
    } else if (at_end) {
      return false;
    } else {
      --active_tickmark_;
      DCHECK(active_tickmark_ < tickmarks_.size());
    }
  }

  if (active_frame != this) {
    // If we are jumping between frames, reset the active tickmark in the old
    // frame and invalidate the area.
    active_frame->active_tickmark_ = WidgetClientWin::kNoTickmark;
    active_frame->InvalidateArea(INVALIDATE_CONTENT_AREA);
    main_frame_impl->active_tickmark_frame_ = this;
  } else {
    // Invalidate the old tickmark.
    if (old_tickmark)
      active_frame->InvalidateTickmark(old_tickmark);
  }

  Selection selection(tickmarks_[active_tickmark_].get());
  frame()->selectionController()->setSelection(selection);
  frame()->revealSelection();  // Scroll the selection into view if necessary.
  // Make sure we save where the selection was after the operation so that
  // we can set the selection to it for the next Find operation (if needed).
  last_active_range_ = tickmarks_[active_tickmark_];
  ClearSelection();  // We will draw our own highlighting.

  // Notify browser of new location for the selected rectangle.
  IntRect pos = tickmarks_[active_tickmark_]->boundingBox();
  pos.move(-frameview()->scrollOffset().width(),
           -frameview()->scrollOffset().height());
  ReportFindInPageSelection(
      gfx::Rect(frame()->view()->convertToContainingWindow(pos)),
      active_tickmark_ + 1,
      request.request_id);

  return true;  // Found a match.
}

int WebFrameImpl::OrdinalOfFirstMatchForFrame(WebFrameImpl* frame) const {
  int ordinal = 0;
  WebFrameImpl* const main_frame_impl =
    static_cast<WebFrameImpl*>(GetView()->GetMainFrame());
  // Iterate from the main frame up to (but not including) this frame and
  // add up the number of tickmarks.
  for (WebFrameImpl* frame = main_frame_impl;
       frame != this;
       frame = static_cast<WebFrameImpl*>(
           webview_impl_->GetNextFrameAfter(frame, true))) {
    ordinal += frame->tickmarks().size();
  }

  return ordinal;
}

bool WebFrameImpl::ShouldScopeMatches(FindInPageRequest request) {
  // Don't scope if we can't find a frame or if the frame is not visible.
  // The user may have closed the tab/application, so abort.
  if (!frame() || !Visible())
    return false;

  DCHECK(frame()->document() && frame()->view());

  // If the frame completed the scoping operation and found 0 matches the last
  // time it was searched, then we don't have to search it again if the user is
  // just adding to the search string or sending the same search string again.
  if (scoping_complete_ &&
      last_search_string_ != std::wstring(L"") && last_match_count_ == 0) {
    // Check to see if the search string prefixes match.
    std::wstring previous_search_prefix =
        request.search_string.substr(0, last_search_string_.length());

    if (previous_search_prefix == last_search_string_) {
      return false;  // Don't search this frame, it will be fruitless.
    }
  }

  return true;
}

void WebFrameImpl::InvalidateIfNecessary() {
  if (last_match_count_ > next_invalidate_after_) {
    // TODO(finnur): (http://b/1088165) Optimize the drawing of the
    // tickmarks and remove this. This calculation sets a milestone for when
    // next to invalidate the scrollbar and the content area. We do this so that
    // we don't spend too much time drawing the scrollbar over and over again.
    // Basically, up until the first 500 matches there is no throttle. After the
    // first 500 matches, we set set the milestone further and further out (750,
    // 1125, 1688, 2K, 3K).
    static const int start_slowing_down_after = 500;
    static const int slowdown = 750;
    int i = (last_match_count_ / start_slowing_down_after);
    next_invalidate_after_ += i * slowdown;

    // Invalidating content area draws both highlighting and in-page
    // tickmarks, but not the scrollbar.
    // TODO(finnur): (http://b/1088165) invalidate content area only if
    // match found on-screen.
    InvalidateArea(INVALIDATE_CONTENT_AREA);
    InvalidateArea(INVALIDATE_SCROLLBAR);
  }
}

// static
bool WebFrameImpl::RangeShouldBeHighlighted(Range* range) {
  ExceptionCode exception = 0;
  Node* common_ancestor_container = range->commonAncestorContainer(exception);

  if (exception)
    return false;

  RenderObject* renderer = common_ancestor_container->renderer();

  if (!renderer)
    return false;

  IntRect overflow_clip_rect = renderer->absoluteClippedOverflowRect();
  return range->boundingBox().intersects(overflow_clip_rect);
}

void WebFrameImpl::selectNodeFromInspector(WebCore::Node* node) {
  inspected_node_ = node;
}

void WebFrameImpl::ScopeStringMatches(FindInPageRequest request,
                                      bool reset) {
  if (!ShouldScopeMatches(request))
    return;

  WebFrameImpl* main_frame_impl =
    static_cast<WebFrameImpl*>(GetView()->GetMainFrame());

  if (reset) {
    // This is a brand new search, so we need to reset everything.
    // Scoping is just about to begin.
    scoping_complete_ = false;
    // First of all, all previous tickmarks need to be erased.
    tickmarks_.clear();
    // Clear the counters from last operation.
    last_match_count_ = 0;
    next_invalidate_after_ = 0;

    main_frame_impl->frames_scoping_count_++;

    // Now, defer scoping until later to allow find operation to finish quickly.
    MessageLoop::current()->PostTask(FROM_HERE,
        scope_matches_factory_.NewRunnableMethod(
            &WebFrameImpl::ScopeStringMatches,
            request,
            false));  // false=we just reset, so don't do it again.
    return;
  }

  WebCore::String webcore_string =
      webkit_glue::StdWStringToString(request.search_string);

  RefPtr<Range> searchRange(rangeOfContents(frame()->document()));

  ExceptionCode ec = 0, ec2 = 0;
  if (!reset && !tickmarks_.isEmpty()) {
    // This is a continuation of a scoping operation that timed out and didn't
    // complete last time around, so we should start from where we left off.
    RefPtr<Range> start_range = tickmarks_.last();
    searchRange->setStart(start_range->startNode(),
                          start_range->startOffset(ec2) + 1, ec);
    if (ec != 0 || ec2 != 0) {
      NOTREACHED();
      return;
    }
  }

  // This timeout controls how long we scope (in ms) before releasing control.
  // This value does not prevent us from running for longer than this, but it
  // is periodically checked to see if we have exceeded our allocated time.
  static const int kTimeout = 100;  // ms

  int matchCount = 0;
  bool timeout = false;
  Time start_time = Time::Now();
  do {
    // Find next occurrence of the search string.
    // TODO(finnur): (http://b/1088245) This WebKit operation may run
    // for longer than the timeout value, and is not interruptible as it is
    // currently written. We may need to rewrite it with interruptibility in
    // mind, or find an alternative.
    RefPtr<Range> resultRange(findPlainText(searchRange.get(),
                                            webcore_string,
                                            true,
                                            request.match_case));
    if (resultRange->collapsed(ec))
      break;  // no further matches.

    // A non-collapsed result range can in some funky whitespace cases still not
    // advance the range's start position (4509328). Break to avoid infinite
    // loop. (This function is based on the implementation of Frame::FindString,
    // which is where this safeguard comes from).
    VisiblePosition newStart =
        endVisiblePosition(resultRange.get(), WebCore::DOWNSTREAM);
    if (newStart ==
        startVisiblePosition(searchRange.get(), WebCore::DOWNSTREAM))
      break;

    ++matchCount;

    // Add the location we just found to the tickmarks collection.
    tickmarks_.append(resultRange);

    setStart(searchRange.get(), newStart);

    // If the Find function found a match it will have stored where the
    // match was found in active_selection_rect_ on the current frame. If we
    // find this rect during scoping it means we have found the active
    // tickmark.
    if (!active_selection_rect_.isEmpty() &&
        active_selection_rect_ == resultRange->boundingBox()) {
      // We have found the active tickmark frame.
      main_frame_impl->active_tickmark_frame_ = this;
      // We also know which tickmark is active now.
      active_tickmark_ = tickmarks_.size() - 1;
      // To stop looking for the active tickmark, we clear this rectangle.
      active_selection_rect_ = IntRect();

      // Notify browser of new location for the selected rectangle.
      IntRect pos = tickmarks_[active_tickmark_]->boundingBox();
      pos.move(-frameview()->scrollOffset().width(),
        -frameview()->scrollOffset().height());
      ReportFindInPageSelection(
          gfx::Rect(frame()->view()->convertToContainingWindow(pos)),
          active_tickmark_ + 1,
          request.request_id);
    }

    timeout = (Time::Now() - start_time).InMilliseconds() >= kTimeout;
  } while (!timeout);

  // Remember what we search for last time, so we can skip searching if more
  // letters are added to the search string (and last outcome was 0).
  last_search_string_ = request.search_string;

  if (matchCount > 0) {
    last_match_count_ += matchCount;

    // Let the mainframe know how much we found during this pass.
    main_frame_impl->IncreaseMatchCount(matchCount, request.request_id);
  }

  if (timeout) {
    // If we found anything during this pass, we should redraw. However, we
    // don't want to spam too much if the page is extremely long, so if we
    // reach a certain point we start throttling the redraw requests.
    if (matchCount > 0)
      InvalidateIfNecessary();

    // Scoping effort ran out of time, lets ask for another time-slice.
    MessageLoop::current()->PostTask(FROM_HERE,
        scope_matches_factory_.NewRunnableMethod(
            &WebFrameImpl::ScopeStringMatches,
            request,
            false));  // don't reset.

    return;  // Done for now, resume work later.
  }

  // This frame has no further scoping left, so it is done. Other frames might,
  // of course, continue to scope matches.
  scoping_complete_ = true;
  main_frame_impl->frames_scoping_count_--;

  // If this is the last frame to finish scoping we need to trigger the final
  // update to be sent.
  if (main_frame_impl->frames_scoping_count_ == 0)
    main_frame_impl->IncreaseMatchCount(0, request.request_id);

  // This frame is done, so show any tickmark/highlight we haven't drawn yet.
  InvalidateArea(INVALIDATE_ALL);

  return;
}

void WebFrameImpl::CancelPendingScopingEffort() {
  scope_matches_factory_.RevokeAll();
  active_tickmark_ = WidgetClientWin::kNoTickmark;
}

void WebFrameImpl::StopFinding() {
  CancelPendingScopingEffort();

  // Let the frame know that we don't want tickmarks or highlighting anymore.
  tickmarks_.clear();
  InvalidateArea(INVALIDATE_ALL);
}

void WebFrameImpl::SelectAll() {
  frame()->selectionController()->selectAll();

  WebViewDelegate* d = GetView()->GetDelegate();
  if (d)
    d->UserMetricsRecordAction(L"SelectAll");
}

void WebFrameImpl::Copy() {
  frame()->editor()->copy();

  WebViewDelegate* d = GetView()->GetDelegate();
  if (d)
    d->UserMetricsRecordAction(L"Copy");
}

void WebFrameImpl::Cut() {
  frame()->editor()->cut();

  WebViewDelegate* d = GetView()->GetDelegate();
  if (d)
    d->UserMetricsRecordAction(L"Cut");
}

// Returns a copy of data from a data handle retrieved from the clipboard. The
// data is decoded according to the format that it is in. The caller is
// responsible for freeing the data.
static wchar_t* GetDataFromHandle(HGLOBAL data_handle,
                                  unsigned int clipboard_format) {
  switch (clipboard_format) {
    case CF_TEXT: {
      char* string_data = static_cast<char*>(::GlobalLock(data_handle));
      int n_chars = ::MultiByteToWideChar(CP_ACP, 0, string_data, -1, NULL, 0);
      wchar_t* wcs_data =
        static_cast<wchar_t*>(malloc((n_chars * sizeof(wchar_t)) +
          sizeof(wchar_t)));
      if (!wcs_data) {
        ::GlobalUnlock(data_handle);
        return NULL;
      }

      ::MultiByteToWideChar(CP_ACP, 0, string_data, -1, wcs_data, n_chars);
      ::GlobalUnlock(data_handle);
      wcs_data[n_chars] = '\0';
      return wcs_data;
    }
    case CF_UNICODETEXT: {
      wchar_t* string_data = static_cast<wchar_t*>(::GlobalLock(data_handle));
      size_t data_size_in_bytes = ::GlobalSize(data_handle);
      wchar_t* wcs_data =
        static_cast<wchar_t*>(malloc(data_size_in_bytes + sizeof(wchar_t)));
      if (!wcs_data) {
        ::GlobalUnlock(data_handle);
        return NULL;
      }

      size_t n_chars = static_cast<int>(data_size_in_bytes / sizeof(wchar_t));
      wmemcpy_s(wcs_data, n_chars, string_data, n_chars);
      ::GlobalUnlock(data_handle);
      wcs_data[n_chars] = '\0';
      return wcs_data;
    }
  }
  return NULL;
}

void WebFrameImpl::Paste() {
  frame()->editor()->paste();

  WebViewDelegate* d = GetView()->GetDelegate();
  if (d)
    d->UserMetricsRecordAction(L"Paste");
}

void WebFrameImpl::Replace(const std::wstring& wtext) {
  String text = webkit_glue::StdWStringToString(wtext);
  frame()->editor()->replaceSelectionWithText(text, false, true);
}

void WebFrameImpl::Delete() {
  frame()->editor()->command("Delete").execute();

  WebViewDelegate* d = GetView()->GetDelegate();
  if (d)
    d->UserMetricsRecordAction(L"DeleteSelection");
}

void WebFrameImpl::Undo() {
  frame()->editor()->undo();

  WebViewDelegate* d = GetView()->GetDelegate();
  if (d)
    d->UserMetricsRecordAction(L"Undo");
}

void WebFrameImpl::Redo() {
  frame()->editor()->redo();

  WebViewDelegate* d = GetView()->GetDelegate();
  if (d)
    d->UserMetricsRecordAction(L"Redo");
}

void WebFrameImpl::ClearSelection() {
  frame()->selectionController()->clear();
}

void WebFrameImpl::CreateFrameView() {
  ASSERT(frame_);  // If frame_ doesn't exist, we probably didn't init properly.

  WebCore::Page* page = frame_->page();
  DCHECK(page);

  DCHECK(page->mainFrame() != NULL);

  // Detach the current view. This ensures that UI widgets like plugins,
  // etc are detached(hidden)
  if (frame_->view())
    frame_->view()->detachFromWindow();

  frame_->setView(0);

  WebCore::FrameView* view = new FrameView(frame_.get());

  frame_->setView(view);

  // Attaching the view ensures that UI widgets like plugins, display/hide
  // correctly.
  frame_->view()->attachToWindow();

  if (margin_width_ >= 0)
    view->setMarginWidth(margin_width_);
  if (margin_height_ >= 0)
    view->setMarginHeight(margin_height_);
  if (!allows_scrolling_)
    view->setScrollbarsMode(WebCore::ScrollbarAlwaysOff);

  // TODO(darin): The Mac code has a comment about this possibly being
  // unnecessary.  See installInFrame in WebCoreFrameBridge.mm
  if (frame_->ownerRenderer())
    frame_->ownerRenderer()->setWidget(view);

  view->initScrollbars();

  // FrameViews are created with a refcount of 1 so it needs releasing after we
  // assign it to a RefPtr.
  view->deref();

  WebFrameImpl* parent = static_cast<WebFrameImpl*>(GetParent());
  if (parent) {
    parent->frameview()->addChild(view);
  } else {
    view->setClient(webview_impl_);

    IntRect geom(0, 0, webview_impl_->size().width(),
                       webview_impl_->size().height());
    view->setFrameGeometry(geom);
  }
}

// static
WebFrameImpl* WebFrameImpl::FromFrame(WebCore::Frame* frame) {
  return static_cast<WebFrameLoaderClient*>(
      frame->loader()->client())->webframe();
}

// WebFrame --------------------------------------------------------------------

void WebFrameImpl::Layout() {
  // layout this frame
  if (frame_->document())
    frame_->document()->updateLayout();
  // layout child frames
  Frame* child = frame_->tree()->firstChild();
  for (; child; child = child->tree()->nextSibling())
    FromFrame(child)->Layout();
}

void WebFrameImpl::Paint(gfx::PlatformCanvasWin* canvas, const gfx::Rect& rect) {
  static StatsRate rendering(L"WebFramePaintTime");
  StatsScope<StatsRate> rendering_scope(rendering);

  if (!rect.IsEmpty()) {
    PlatformContextSkia context(canvas);

    // PlatformGraphicsContext is actually a pointer to PlatformContextSkia
    GraphicsContext gc(reinterpret_cast<PlatformGraphicsContext*>(&context));
    IntRect dirty_rect(rect.x(), rect.y(), rect.width(), rect.height());

    if (frame_->document() && frameview()) {
      frameview()->paint(&gc, dirty_rect);
    } else {
      gc.fillRect(dirty_rect, Color::white);
    }
  }
}

gfx::BitmapPlatformDeviceWin WebFrameImpl::CaptureImage(bool scroll_to_zero) {
  // Must layout before painting.
  Layout();

  gfx::PlatformCanvasWin canvas(frameview()->width(), frameview()->height(), true);
  PlatformContextSkia context(&canvas);

  GraphicsContext gc(reinterpret_cast<PlatformGraphicsContext*>(&context));
  frameview()->paint(&gc, IntRect(0, 0, frameview()->width(),
                                  frameview()->height()));

  gfx::BitmapPlatformDeviceWin& device =
      static_cast<gfx::BitmapPlatformDeviceWin&>(canvas.getTopPlatformDevice());
  device.fixupAlphaBeforeCompositing();
  return device;
}

bool WebFrameImpl::IsLoading() {
  // I'm assuming this does what we want.
  return frame_->loader()->isLoading();
}

void WebFrameImpl::Closing() {
  // let go of our references, this breaks reference cycles and will
  // usually eventually lead to us being destroyed.
  if (frameview())
    frameview()->clear();
  if (frame_) {
    StopLoading();
    frame_ = NULL;
  }
  alt_error_page_fetcher_.reset();
  webview_impl_ = NULL;
}

void WebFrameImpl::DidReceiveData(DocumentLoader* loader,
                                  const char* data, int length) {
  // Set the text encoding.  This calls begin() for us.  It is safe to call
  // this multiple times (Mac does: page/mac/WebCoreFrameBridge.mm).
  bool user_chosen = true;
  String encoding = frame_->loader()->documentLoader()->overrideEncoding();
  if (encoding.isNull()) {
    user_chosen = false;
    encoding = loader->response().textEncodingName();
  }
  frame_->loader()->setEncoding(encoding, user_chosen);

  // NOTE: mac only does this if there is a document
  frame_->loader()->addData(data, length);

  // It's possible that we get a DNS failure followed by a second load that
  // succeeds before we hear back from the alternate error page server.  In
  // that case, cancel the alt error page download.
  alt_error_page_fetcher_.reset();
}

void WebFrameImpl::DidFail(const ResourceError& error, bool was_provisional) {
  // Make sure we never show errors in view source mode.
  SetInViewSourceMode(false);

  WebViewDelegate* delegate = webview_impl_->delegate();
  if (delegate) {
    WebErrorImpl web_error(error);
    if (was_provisional) {
      delegate->DidFailProvisionalLoadWithError(webview_impl_, web_error,
                                                this);
    } else {
      delegate->DidFailLoadWithError(webview_impl_, web_error, this);
    }
  }
}

void WebFrameImpl::LoadAlternateHTMLErrorPage(const WebRequest* request,
                                              const WebError& error,
                                              const GURL& error_page_url,
                                              bool replace,
                                              const GURL& fake_url) {
  // Load alternate HTML in place of the previous request.  We create a copy of
  // the original request so we can replace its URL with a dummy URL.  That
  // prevents other web content from the same origin as the failed URL to
  // script the error page.
  scoped_ptr<WebRequest> failed_request(request->Clone());
  failed_request->SetURL(fake_url);

  LoadAlternateHTMLString(failed_request.get(), std::string(),
                          error.GetFailedURL(), replace);

  WebErrorImpl weberror_impl(error);
  alt_error_page_fetcher_.reset(
      new AltErrorPageResourceFetcher(webview_impl_, weberror_impl, this,
                                      error_page_url));
}

std::wstring WebFrameImpl::GetName() {
  return webkit_glue::StringToStdWString(frame_->tree()->name());
}

WebTextInput* WebFrameImpl::GetTextInput() {
  if (!webtextinput_impl_.get()) {
    webtextinput_impl_.reset(new WebTextInputImpl(this));
  }
  return webtextinput_impl_.get();
}

void WebFrameImpl::SetPrinting(bool printing,
                               float page_width_min,
                               float page_width_max) {
  frame_->setPrinting(printing,
                      page_width_min,
                      page_width_max,
                      true);
}

bool WebFrameImpl::Visible() {
  return frame()->view()->visibleWidth() > 0 &&
         frame()->view()->visibleHeight() > 0;
}

void WebFrameImpl::CreateChildFrame(const FrameLoadRequest& r,
                                    HTMLFrameOwnerElement* owner_element,
                                    bool allows_scrolling,
                                    int margin_height,
                                    int margin_width,
                                    Frame*& result) {
  // TODO(darin): share code with initWithName()

  scoped_refptr<WebFrameImpl> webframe = new WebFrameImpl();

  // Add an extra ref on behalf of the Frame/FrameLoader, which references the
  // WebFrame via the FrameLoaderClient interface. See the comment at the top
  // of this file for more info.
  webframe->AddRef();

  webframe->allows_scrolling_ = allows_scrolling;
  webframe->margin_width_ = margin_width;
  webframe->margin_height_ = margin_height;

  webframe->frame_ =
    new Frame(frame_->page(), owner_element, &webframe->frame_loader_client_);
  webframe->frame_->tree()->setName(r.frameName());

  webframe->webview_impl_ = webview_impl_;  // owning ref


  // Note that Frames already start out with a refcount of 1.
  // We wait until loader()->load() returns before deref-ing the Frame.
  // Otherwise the danger is that the onload handler can cause
  // the Frame to be dealloc-ed, and subsequently trash memory.
  // (b:1055700)
  WTF::RefPtr<Frame> protector(WTF::adoptRef(webframe->frame_.get()));

  frame_->tree()->appendChild(webframe->frame_);

  // Frame::init() can trigger onload event in the parent frame,
  // which may detach this frame and trigger a null-pointer access
  // in FrameTree::removeChild. Move init() after appendChild call
  // so that webframe->frame_ is in the tree before triggering
  // onload event handler.
  // Because the event handler may set webframe->frame_ to null,
  // it is necessary to check the value after calling init() and
  // return without loading URL.
  // (b:791612)
  webframe->frame_->init();      // create an empty document
  if (!webframe->frame_.get())
    return;

  // The following code was pulled from WebFrame.mm:_loadURL, with minor
  // modifications.  The purpose is to ensure we load the right HistoryItem for
  // this child frame.
  HistoryItem* parentItem = frame_->loader()->currentHistoryItem();
  FrameLoadType loadType = frame_->loader()->loadType();
  FrameLoadType childLoadType = WebCore::FrameLoadTypeRedirectWithLockedHistory;
  KURL new_url = r.resourceRequest().url();

  // If we're moving in the backforward list, we might want to replace the
  // content of this child frame with whatever was there at that point.
  // Reload will maintain the frame contents, LoadSame will not.
  if (parentItem && parentItem->children().size() != 0 &&
      (isBackForwardLoadType(loadType) ||
       loadType == WebCore::FrameLoadTypeReload ||
       loadType == WebCore::FrameLoadTypeReloadAllowingStaleData)) {
    HistoryItem* childItem = parentItem->childItemWithName(r.frameName());
    if (childItem) {
      // Use the original URL to ensure we get all the side-effects, such as
      // onLoad handlers, of any redirects that happened. An example of where
      // this is needed is Radar 3213556.
      new_url = KURL(KURL(""),
                     childItem->originalURLString().deprecatedString());

      // These behaviors implied by these loadTypes should apply to the child
      // frames
      childLoadType = loadType;

      if (isBackForwardLoadType(loadType)) {
        // For back/forward, remember this item so we can traverse any child
        // items as child frames load.
        webframe->frame_->loader()->setProvisionalHistoryItem(childItem);
      } else {
        // For reload, just reinstall the current item, since a new child frame
        // was created but we won't be creating a new BF item
        webframe->frame_->loader()->setCurrentHistoryItem(childItem);
      }
    }
  }

  webframe->frame_->loader()->load(new_url,
                                   r.resourceRequest().httpReferrer(),
                                   childLoadType,
                                   String(), NULL, NULL);

  // A synchronous navigation (about:blank) would have already processed
  // onload, so it is possible for the frame to have already been destroyed by
  // script in the page.
  result = webframe->frame_.get();
}

bool WebFrameImpl::ExecuteCoreCommandByName(const std::string& name,
                                            const std::string& value) {
  ASSERT(frame());
  return frame()->editor()->command(webkit_glue::StdStringToString(name))
      .execute(webkit_glue::StdStringToString(value));
}

void WebFrameImpl::AddMessageToConsole(const std::wstring& msg,
                                       ConsoleMessageLevel level) {
  ASSERT(frame());

  WebCore::MessageLevel webcore_message_level;
  switch (level) {
    case MESSAGE_LEVEL_TIP:
      webcore_message_level = WebCore::TipMessageLevel;
      break;
    case MESSAGE_LEVEL_LOG:
      webcore_message_level = WebCore::LogMessageLevel;
      break;
    case MESSAGE_LEVEL_WARNING:
      webcore_message_level = WebCore::WarningMessageLevel;
      break;
    case MESSAGE_LEVEL_ERROR:
      webcore_message_level = WebCore::ErrorMessageLevel;
      break;
    default:
      NOTREACHED();
      return;
  }

  frame()->page()->chrome()->addMessageToConsole(
      WebCore::OtherMessageSource, webcore_message_level,
      webkit_glue::StdWStringToString(msg), 1, String());
}

void WebFrameImpl::ClosePage() {
  // TODO(creis): Find a way to use WebView::Close() instead.  (See comments in
  // webframe.h and RenderView::OnClosePage.)
  frame_->loader()->closeURL();
}

gfx::Size WebFrameImpl::ScrollOffset() const {
  WebCore::FrameView* view = frameview();
  if (view) {
    WebCore::IntSize s = view->scrollOffset();
    return gfx::Size(s.width(), s.height());
  }

  return gfx::Size();
}

void WebFrameImpl::SetAllowsScrolling(bool flag) {
  allows_scrolling_ = flag;
  frame_->view()->setAllowsScrolling(flag);
}

bool WebFrameImpl::SetPrintingMode(bool printing,
                                  float page_width_min,
                                  float page_width_max,
                                  int* width) {
  // Make sure main frame is loaded.
  WebCore::FrameView* view = frameview();
  if (!view) {
    NOTREACHED();
    return false;
  }
  printing_ = printing;
  if (printing) {
    view->setScrollbarsMode(WebCore::ScrollbarAlwaysOff);
  } else {
    view->setScrollbarsMode(WebCore::ScrollbarAuto);
  }
  DCHECK_EQ(frame()->isFrameSet(), false);

  SetPrinting(printing, page_width_min, page_width_max);
  if (!printing)
    pages_.clear();

  // The document width is well hidden.
  if (width)
    *width = frame()->document()->renderer()->width();
  return true;
}

int WebFrameImpl::ComputePageRects(const gfx::Size& page_size_px) {
  if (!printing_ ||
      !frame() ||
      !frame()->document()) {
    NOTREACHED();
    return 0;
  }
  // In Safari, they are using:
  // (0,0) + soft margins top/left
  // (phys width, phys height) - hard margins -
  //     soft margins top/left - soft margins right/bottom
  // TODO(maruel): Weird. We don't do that.
  // Everything is in pixels :(
  // pages_ and page_height are actually output parameters.
  int page_height;
  WebCore::IntRect rect(0, 0, page_size_px.width(), page_size_px.height());
  computePageRectsForFrame(frame(), rect, 0, 0, 1.0, pages_, page_height);
  return pages_.size();
}

void WebFrameImpl::GetPageRect(int page, gfx::Rect* page_size) const {
  if (page < 0 || page >= static_cast<int>(pages_.size())) {
    NOTREACHED();
    return;
  }
  *page_size = pages_[page];
}

bool WebFrameImpl::SpoolPage(int page,
                            PlatformContextSkia* context) {
  // Ensure correct state.
  if (!context ||
      !printing_ ||
      page < 0 ||
      page >= static_cast<int>(pages_.size())) {
    NOTREACHED();
    return false;
  }

  if (!frame() || !frame()->document()) {
    NOTREACHED();
    return false;
  }

  GraphicsContext spool(reinterpret_cast<PlatformGraphicsContext*>(context));
  DCHECK(pages_[page].x() == 0);
  // Offset to get the right square.
  spool.translate(0, -static_cast<float>(pages_[page].y()));
  frame()->paint(&spool, pages_[page]);
  return true;
}

bool WebFrameImpl::HasUnloadListener() {
  if (frame() && frame()->document()) {
    Document* doc = frame()->document();
    return doc->hasUnloadEventListener();
  }
  return false;
}

bool WebFrameImpl::IsReloadAllowingStaleData() const {
  FrameLoader* loader = frame() ? frame()->loader() : NULL;
  if (loader) {
    return WebCore::FrameLoadTypeReloadAllowingStaleData ==
           loader->policyLoadType();
  }
  return false;
}
