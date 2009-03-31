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
//   Page O------- Frame (m_mainFrame) O-------O FrameView
//                   ||
//                   ||
//               FrameLoader O-------- WebFrame (via FrameLoaderClient)
//
// FrameLoader and Frame are formerly one object that was split apart because
// it got too big. They basically have the same lifetime, hence the double line.
//
// WebFrame is refcounted and has one ref on behalf of the FrameLoader/Frame.
// This is not a normal reference counted pointer because that would require
// changing WebKit code that we don't control. Instead, it is created with this
// ref initially and it is removed when the FrameLoader is getting destroyed.
//
// WebFrames are created in two places, first in WebViewImpl when the root
// frame is created, and second in WebFrame::CreateChildFrame when sub-frames
// are created. WebKit will hook up this object to the FrameLoader/Frame
// and the refcount will be correct.
//
// How frames are destroyed
// ------------------------
//
// The main frame is never destroyed and is re-used. The FrameLoader is re-used
// and a reference to the main frame is kept by the Page.
//
// When frame content is replaced, all subframes are destroyed. This happens
// in FrameLoader::detachFromParent for each subframe.
//
// Frame going away causes the FrameLoader to get deleted. In FrameLoader's
// destructor, it notifies its client with frameLoaderDestroyed. This calls
// WebFrame::Closing and then derefs the WebFrame and will cause it to be
// deleted (unless an external someone is also holding a reference).

#include "config.h"

#include "base/compiler_specific.h"
#include "build/build_config.h"

#include <algorithm>
#include <string>

MSVC_PUSH_WARNING_LEVEL(0);
#include "HTMLFormElement.h"  // need this before Document.h
#include "Chrome.h"
#include "ChromeClientChromium.h"
#include "Console.h"
#include "Document.h"
#include "DocumentFragment.h"  // Only needed for ReplaceSelectionCommand.h :(
#include "DocumentLoader.h"
#include "DocumentMarker.h"
#include "DOMWindow.h"
#include "Editor.h"
#include "EventHandler.h"
#include "FormState.h"
#include "Frame.h"
#include "FrameChromium.h"
#include "FrameLoader.h"
#include "FrameLoadRequest.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLHeadElement.h"
#include "HTMLLinkElement.h"
#include "HTMLNames.h"
#include "HistoryItem.h"
#include "InspectorController.h"
#include "markup.h"
#include "Page.h"
#include "PlatformContextSkia.h"
#include "PrintContext.h"
#include "RenderFrame.h"
#if defined(OS_WIN)
#include "RenderThemeChromiumWin.h"
#endif
#include "RenderWidget.h"
#include "ReplaceSelectionCommand.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
#include "ScriptValue.h"
#include "ScrollbarTheme.h"
#include "SelectionController.h"
#include "Settings.h"
#include "SkiaUtils.h"
#include "SubstituteData.h"
#include "TextIterator.h"
#include "TextAffinity.h"
#include "XPathResult.h"

MSVC_POP_WARNING();

#undef LOG

#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "base/time.h"
#include "net/base/net_errors.h"
#include "skia/ext/bitmap_platform_device.h"
#include "skia/ext/platform_canvas.h"
#include "webkit/glue/alt_error_page_resource_fetcher.h"
#include "webkit/glue/dom_operations.h"
#include "webkit/glue/dom_operations_private.h"
#include "webkit/glue/feed.h"
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webdatasource_impl.h"
#include "webkit/glue/weberror_impl.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webhistoryitem_impl.h"
#include "webkit/glue/weburlrequest_impl.h"
#include "webkit/glue/webtextinput_impl.h"
#include "webkit/glue/webview_impl.h"

#if defined(OS_LINUX)
#include <gdk/gdk.h>
#endif

#if USE(JSC)
#include "bridge/c/c_instance.h"
#include "bridge/runtime_object.h"
#endif

using base::Time;
using WebCore::ChromeClientChromium;
using WebCore::Color;
using WebCore::Document;
using WebCore::DocumentFragment;
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
using WebCore::Range;
using WebCore::ReloadIgnoringCacheData;
using WebCore::RenderObject;
using WebCore::ResourceError;
using WebCore::ResourceHandle;
using WebCore::ResourceRequest;
using WebCore::VisibleSelection;
using WebCore::SharedBuffer;
using WebCore::String;
using WebCore::SubstituteData;
using WebCore::TextIterator;
using WebCore::VisiblePosition;
using WebCore::XPathResult;

// Key for a StatsCounter tracking how many WebFrames are active.
static const char* const kWebFrameActiveCount = "WebFrameActiveCount";

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

  if (!frame->view())
    return;

  // TextIterator iterates over the visual representation of the DOM. As such,
  // it requires you to do a layout before using it (otherwise it'll crash).
  if (frame->view()->needsLayout())
    frame->view()->layout();

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
      const uint16* chars = reinterpret_cast<const uint16*>(it.characters());
      if (!chars) {
        if (it.length() != 0) {
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

        // Just got a NULL node, we can forge ahead!
        continue;
      }
      int to_append = std::min(it.length(),
                               max_chars - static_cast<int>(output->size()));
      std::wstring wstr;
      UTF16ToWide(reinterpret_cast<const char16*>(chars), to_append, &wstr);
      output->append(wstr.c_str(), to_append);
      if (output->size() >= static_cast<size_t>(max_chars))
        return;  // Filled up the buffer.
    }
  }

  // Recursively walk the children.
  FrameTree* frame_tree = frame->tree();
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

// Simple class to override some of PrintContext behavior.
class ChromePrintContext : public WebCore::PrintContext {
 public:
  ChromePrintContext(Frame* frame)
      : PrintContext(frame),
        printed_page_width_(0) {
  }
  void begin(float width) {
    DCHECK(!printed_page_width_);
    printed_page_width_ = width;
    WebCore::PrintContext::begin(printed_page_width_);
  }
  // Spools the printed page, a subrect of m_frame.
  // Skip the scale step. NativeTheme doesn't play well with scaling. Scaling
  // is done browser side instead.
  // Returns the scale to be applied.
  float spoolPage(GraphicsContext& ctx, int pageNumber) {
    IntRect pageRect = m_pageRects[pageNumber];
    float scale = printed_page_width_ / pageRect.width();

    ctx.save();
    ctx.translate(static_cast<float>(-pageRect.x()),
                  static_cast<float>(-pageRect.y()));
    ctx.clip(pageRect);
    m_frame->view()->paintContents(&ctx, pageRect);
    ctx.restore();
    return scale;
  }
 protected:
  // Set when printing.
  float printed_page_width_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromePrintContext);
};

// WebFrameImpl ----------------------------------------------------------------

int WebFrameImpl::live_object_count_ = 0;

// static
WebFrame* WebFrame::RetrieveActiveFrame() {
  WebCore::Frame* frame = WebCore::ScriptController::retrieveActiveFrame();
  if (frame)
    return WebFrameImpl::FromFrame(frame);
  else
    return NULL;
}

WebFrameImpl::WebFrameImpl()
// Don't complain about using "this" in initializer list.
MSVC_PUSH_DISABLE_WARNING(4355)
  : frame_loader_client_(this),
    scope_matches_factory_(this),
MSVC_POP_WARNING()
    currently_loading_request_(NULL),
    plugin_delegate_(NULL),
    active_match_frame_(NULL),
    active_match_index_(-1),
    locating_active_rect_(false),
    resume_scoping_from_range_(NULL),
    last_match_count_(-1),
    total_matchcount_(-1),
    frames_scoping_count_(-1),
    scoping_complete_(false),
    next_invalidate_after_(0) {
  StatsCounter(kWebFrameActiveCount).Increment();
  live_object_count_++;
}

WebFrameImpl::~WebFrameImpl() {
  StatsCounter(kWebFrameActiveCount).Decrement();
  live_object_count_--;

  CancelPendingScopingEffort();
  ClearPasswordListeners();
}

// WebFrame -------------------------------------------------------------------

void WebFrameImpl::InitMainFrame(WebViewImpl* webview_impl) {
  webview_impl_ = webview_impl;

  RefPtr<Frame> frame =
      Frame::create(webview_impl_->page(), 0, &frame_loader_client_);
  frame_ = frame.get();

  // Add reference on behalf of FrameLoader.  See comments in
  // WebFrameLoaderClient::frameLoaderDestroyed for more info.
  AddRef();

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
    String script = decodeURLEscapeSequences(
        kurl.string().substring(sizeof("javascript:")-1));
    WebCore::ScriptValue result = frame_->loader()->executeScript(script, true);
    String scriptResult;
    if (result.getString(scriptResult) &&
        !frame_->loader()->isScheduledLocationChangePending()) {
      // TODO(darin): We need to figure out how to represent this in session
      // history.  Hint: don't re-eval script when the user or script navigates
      // back-n-forth (instead store the script result somewhere).
      LoadDocumentData(kurl, scriptResult, String("text/html"), String());
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
    frame_->loader()->load(resource_request, data, false);
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
      current_item = HistoryItem::create();
      current_item->setLastVisitWasFailure(true);
      frame_->loader()->setCurrentHistoryItem(current_item);
      webview_impl_->SetCurrentHistoryItem(current_item.get());
    }

    frame_->loader()->goToItem(request_impl->history_item().get(),
                               WebCore::FrameLoadTypeIndexedBackForward);
  } else if (resource_request.cachePolicy() == ReloadIgnoringCacheData) {
    frame_->loader()->reload();
  } else {
    frame_->loader()->load(resource_request, false);
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
  RefPtr<SharedBuffer> buf = SharedBuffer::create(html_text.data(), len);

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
            webkit_glue::CastToHTMLLinkElement(child);
        if (link_element && link_element->type() == kOSDType &&
            link_element->rel() == kOSDRel && !link_element->href().isEmpty()) {
          return webkit_glue::KURLToGURL(link_element->href());
        }
      }
    }
  }
  return GURL();
}

scoped_refptr<FeedList> WebFrameImpl::GetFeedList() const {
  scoped_refptr<FeedList> feedlist = new FeedList();

  WebCore::FrameLoader* frame_loader = frame_->loader();
  if (frame_loader->state() != WebCore::FrameStateComplete ||
      !frame_->document() ||
      !frame_->document()->head() ||
      frame_->tree()->parent())
    return feedlist;

  // We only consider HTML documents with <head> tags.
  // (Interestingly, isHTMLDocument() returns false for some pages --
  // perhaps an XHTML thing?  It doesn't really matter because head() is
  // a method on Documents anyway.)
  WebCore::HTMLHeadElement* head = frame_->document()->head();
  if (!head)
    return feedlist;

  // Iterate through all children of the <head>, looking for feed links.
  for (WebCore::Node* node = head->firstChild();
       node; node = node->nextSibling()) {
    // Skip over all nodes except <link ...>.
    if (!node->isHTMLElement())
      continue;
    if (!static_cast<WebCore::Element*>(node)->hasLocalName("link"))
      continue;

    const WebCore::HTMLLinkElement* link =
        static_cast<WebCore::HTMLLinkElement*>(node);

    // Look at the 'rel' tag and see if we have a feed.
    std::wstring rel = webkit_glue::StringToStdWString(link->rel());
    bool is_feed = false;
    if (LowerCaseEqualsASCII(rel, "feed") ||
        LowerCaseEqualsASCII(rel, "feed alternate")) {
      // rel="feed" or rel="alternate feed" always means this is a feed.
      is_feed = true;
    } else if (LowerCaseEqualsASCII(rel, "alternate")) {
      // Otherwise, rel="alternate" may mean a feed if it has a certain mime
      // type.
      std::wstring link_type = webkit_glue::StringToStdWString(link->type());
      TrimWhitespace(link_type, TRIM_ALL, &link_type);
      if (LowerCaseEqualsASCII(link_type, "application/atom+xml") ||
          LowerCaseEqualsASCII(link_type, "application/rss+xml")) {
        is_feed = true;
      }
    }

    if (is_feed) {
      FeedItem feedItem;
      feedItem.title = webkit_glue::StringToStdWString(link->title());
      TrimWhitespace(feedItem.title, TRIM_ALL, &feedItem.title);
      feedItem.type = webkit_glue::StringToStdWString(link->type());
      TrimWhitespace(feedItem.type, TRIM_ALL, &feedItem.type);
      feedItem.url = webkit_glue::KURLToGURL(link->href());
      feedlist->Add(feedItem);
    }
  }

  return feedlist;
}

bool WebFrameImpl::GetPreviousHistoryState(std::string* history_state) const {
  // We use the previous item here because documentState (filled-out forms)
  // only get saved to history when it becomes the previous item.  The caller
  // is expected to query the history state after a navigation occurs, after
  // the desired history item has become the previous entry.
  RefPtr<HistoryItem> item = webview_impl_->GetPreviousHistoryItem();
  if (!item)
    return false;

  static StatsCounterTimer history_timer("GetHistoryTimer");
  StatsScope<StatsCounterTimer> history_scope(history_timer);

  webkit_glue::HistoryItemToString(item, history_state);
  return true;
}

bool WebFrameImpl::GetCurrentHistoryState(std::string* state) const {
  if (frame_->loader())
    frame_->loader()->saveDocumentAndScrollState();
  RefPtr<HistoryItem> item = frame_->page()->backForwardList()->currentItem();
  if (!item)
    return false;

  webkit_glue::HistoryItemToString(item, state);
  return true;
}

bool WebFrameImpl::HasCurrentHistoryState() const {
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
  frameview()->setScrollPosition(WebCore::IntPoint());

  // Make sure the correct document type is constructed.
  frame_->loader()->setResponseMIMEType(mime_type);

  // TODO(darin): Inform the FrameLoader of the charset somehow.

  frame_->loader()->begin(base_url);
  frame_->loader()->write(data);
  frame_->loader()->end();
}

static WebDataSource* DataSourceForDocLoader(DocumentLoader* loader) {
  return loader ? WebDataSourceImpl::FromLoader(loader) : NULL;
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

void WebFrameImpl::set_currently_loading_history_item(
    WebHistoryItemImpl* item) {
  currently_loading_history_item_ = item;
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

WebFrame* WebFrameImpl::GetTop() const {
  if (frame_)
    return FromFrame(frame_->tree()->top());

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

std::string WebFrameImpl::GetSecurityOrigin() const {
  if (frame_) {
    if (frame_->document())
      return webkit_glue::StringToStdString(
          frame_->document()->securityOrigin()->toString());
  }
  return "null";
}

void WebFrameImpl::BindToWindowObject(const std::wstring& name,
                                      NPObject* object) {
  assert(frame_);
  if (!frame_ || !frame_->script()->isEnabled())
    return;

  // TODO(mbelshe): Move this to the ScriptController and make it JS neutral.

  String key = webkit_glue::StdWStringToString(name);
#if USE(V8)
  frame_->script()->BindToWindowObject(frame_, key, object);
#endif

#if USE(JSC)
  JSC::JSGlobalObject* window = frame_->script()->globalObject();
  JSC::ExecState* exec = window->globalExec();
  JSC::Bindings::RootObject* root = frame_->script()->bindingRootObject();
  ASSERT(exec);
  JSC::RuntimeObjectImp* instance(JSC::Bindings::Instance::createRuntimeObject(
      exec, JSC::Bindings::CInstance::create(object, root)));
  JSC::Identifier id(exec, key.latin1().data());
  JSC::PutPropertySlot slot;
  window->put(exec, id, instance, slot);
#endif
}


// Call JavaScript garbage collection.
void WebFrameImpl::CallJSGC() {
  if (!frame_)
    return;
  if (!frame_->settings()->isJavaScriptEnabled())
    return;
  // TODO(mbelshe): Move this to the ScriptController and make it JS neutral.
#if USE(V8)
  frame_->script()->collectGarbage();
#endif
}

void WebFrameImpl::GetContentAsPlainText(int max_chars,
                                         std::wstring* text) const {
  text->clear();
  if (!frame_)
    return;

  FrameContentAsPlainText(max_chars, frame_, text);
}

NPObject* WebFrameImpl::GetWindowNPObject() {
  if (!frame_)
    return NULL;

  return frame_->script()->windowScriptNPObject();
}

void WebFrameImpl::InvalidateArea(AreaToInvalidate area) {
  ASSERT(frame() && frame()->view());
#if defined(OS_WIN)
  // TODO(pinkerton): Fix Mac invalidation to be more like Win ScrollView
  FrameView* view = frame()->view();

  if ((area & INVALIDATE_ALL) == INVALIDATE_ALL) {
    view->invalidateRect(view->frameRect());
  } else {
    if ((area & INVALIDATE_CONTENT_AREA) == INVALIDATE_CONTENT_AREA) {
      IntRect content_area(
          view->x(), view->y(), view->visibleWidth(), view->visibleHeight());
      view->invalidateRect(content_area);
    }

    if ((area & INVALIDATE_SCROLLBAR) == INVALIDATE_SCROLLBAR) {
      // Invalidate the vertical scroll bar region for the view.
      IntRect scroll_bar_vert(
          view->x() + view->visibleWidth(), view->y(),
          WebCore::ScrollbarTheme::nativeTheme()->scrollbarThickness(),
          view->visibleHeight());
      view->invalidateRect(scroll_bar_vert);
    }
  }
#endif
}

void WebFrameImpl::IncreaseMatchCount(int count, int request_id) {
  // This function should only be called on the mainframe.
  DCHECK(this == static_cast<WebFrameImpl*>(GetView()->GetMainFrame()));

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
      webkit_glue::String16ToString(request.search_string);

  WebFrameImpl* const main_frame_impl =
      static_cast<WebFrameImpl*>(GetView()->GetMainFrame());

  if (!request.find_next)
    frame()->page()->unmarkAllTextMatches();

  // Starts the search from the current selection.
  bool start_in_selection = true;

  DCHECK(frame() && frame()->view());
  bool found = frame()->findString(webcore_string, request.forward,
                                   request.match_case, wrap_within_frame,
                                   start_in_selection);
  if (found) {
#if defined(OS_WIN)
    WebCore::RenderThemeChromiumWin::setFindInPageMode(true);
#endif
    // Store which frame was active. This will come in handy later when we
    // change the active match ordinal below.
    WebFrameImpl* old_active_frame = main_frame_impl->active_match_frame_;
    // Set this frame as the active frame (the one with the active highlight).
    main_frame_impl->active_match_frame_ = this;

    // We found something, so we can now query the selection for its position.
    VisibleSelection new_selection(frame()->selection()->selection());
    IntRect curr_selection_rect;

    // If we thought we found something, but it couldn't be selected (perhaps
    // because it was marked -webkit-user-select: none), we can't set it to
    // be active but we still continue searching. This matches Safari's
    // behavior, including some oddities when selectable and un-selectable text
    // are mixed on a page: see https://bugs.webkit.org/show_bug.cgi?id=19127.
    if (new_selection.isNone() ||
        (new_selection.start() == new_selection.end())) {
      active_match_ = NULL;
    } else {
      active_match_ = new_selection.toNormalizedRange();
      curr_selection_rect = active_match_->boundingBox();
    }

    if (!request.find_next) {
      // This is a Find operation, so we set the flag to ask the scoping effort
      // to find the active rect for us so we can update the ordinal (n of m).
      locating_active_rect_ = true;
    } else {
      if (old_active_frame != this) {
        // If the active frame has changed it means that we have a multi-frame
        // page and we just switch to searching in a new frame. Then we just
        // want to reset the index.
        if (request.forward)
          active_match_index_ = 0;
        else
          active_match_index_ = last_match_count_ - 1;
      } else {
        // We are still the active frame, so increment (or decrement) the
        // |active_match_index|, wrapping if needed (on single frame pages).
        request.forward ? ++active_match_index_ : --active_match_index_;
        if (active_match_index_ + 1 > last_match_count_)
          active_match_index_ = 0;
        if (active_match_index_ + 1 == 0)
          active_match_index_ = last_match_count_ - 1;
      }
#if defined(OS_WIN)
      // TODO(pinkerton): Fix Mac scrolling to be more like Win ScrollView
      if (selection_rect) {
        gfx::Rect rect = webkit_glue::FromIntRect(
            frame()->view()->convertToContainingWindow(curr_selection_rect));
        rect.Offset(-frameview()->scrollOffset().width(),
                    -frameview()->scrollOffset().height());
        *selection_rect = rect;

        ReportFindInPageSelection(rect,
                                  active_match_index_ + 1,
                                  request.request_id);
      }
#endif
    }
  } else {
    // Nothing was found in this frame.
    active_match_ = NULL;

    // Erase all previous tickmarks and highlighting.
    InvalidateArea(INVALIDATE_ALL);
  }

  return found;
}

int WebFrameImpl::OrdinalOfFirstMatchForFrame(WebFrameImpl* frame) const {
  int ordinal = 0;
  WebFrameImpl* const main_frame_impl =
    static_cast<WebFrameImpl*>(GetView()->GetMainFrame());
  // Iterate from the main frame up to (but not including) |frame| and
  // add up the number of matches found so far.
  for (WebFrameImpl* it = main_frame_impl;
       it != frame;
       it = static_cast<WebFrameImpl*>(
           webview_impl_->GetNextFrameAfter(it, true))) {
    if (it->last_match_count_ > 0)
      ordinal += it->last_match_count_;
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
      !last_search_string_.empty() && last_match_count_ == 0) {
    // Check to see if the search string prefixes match.
    string16 previous_search_prefix =
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

    InvalidateArea(INVALIDATE_SCROLLBAR);
  }
}

void WebFrameImpl::AddMarker(WebCore::Range* range) {
  // Use a TextIterator to visit the potentially multiple nodes the range
  // covers.
  TextIterator markedText(range);
  for (; !markedText.atEnd(); markedText.advance()) {
    RefPtr<Range> textPiece = markedText.range();
    int exception = 0;

    WebCore::DocumentMarker marker = {
        WebCore::DocumentMarker::TextMatch,
        textPiece->startOffset(exception),
        textPiece->endOffset(exception),
        "" };

    if (marker.endOffset > marker.startOffset) {
      // Find the node to add a marker to and add it.
      Node* node = textPiece->startContainer(exception);
      frame()->document()->addMarker(node, marker);

      // Rendered rects for markers in WebKit are not populated until each time
      // the markers are painted. However, we need it to happen sooner, because
      // the whole purpose of tickmarks on the scrollbar is to show where
      // matches off-screen are (that haven't been painted yet).
      Vector<WebCore::DocumentMarker> markers =
          frame()->document()->markersForNode(node);
      frame()->document()->setRenderedRectForMarker(
          textPiece->startContainer(exception),
          markers[markers.size() - 1],
          range->boundingBox());
    }
  }
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
    // Clear highlighting for this frame.
    if (frame()->markedTextMatchesAreHighlighted())
      frame()->page()->unmarkAllTextMatches();
    // Clear the counters from last operation.
    last_match_count_ = 0;
    next_invalidate_after_ = 0;

    resume_scoping_from_range_ = NULL;

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
      webkit_glue::String16ToString(request.search_string);

  RefPtr<Range> search_range(rangeOfContents(frame()->document()));

  ExceptionCode ec = 0, ec2 = 0;
  if (resume_scoping_from_range_.get()) {
    // This is a continuation of a scoping operation that timed out and didn't
    // complete last time around, so we should start from where we left off.
    search_range->setStart(resume_scoping_from_range_->startContainer(),
                           resume_scoping_from_range_->startOffset(ec2) + 1,
                           ec);
    if (ec != 0 || ec2 != 0) {
      if (ec2 != 0)  // A non-zero |ec| happens when navigating during search.
        NOTREACHED();
      return;
    }
  }

  // This timeout controls how long we scope (in ms) before releasing control.
  // This value does not prevent us from running for longer than this, but it
  // is periodically checked to see if we have exceeded our allocated time.
  static const int kTimeout = 100;  // ms

  int match_count = 0;
  bool timeout = false;
  Time start_time = Time::Now();
  do {
    // Find next occurrence of the search string.
    // TODO(finnur): (http://b/1088245) This WebKit operation may run
    // for longer than the timeout value, and is not interruptible as it is
    // currently written. We may need to rewrite it with interruptibility in
    // mind, or find an alternative.
    RefPtr<Range> result_range(findPlainText(search_range.get(),
                                            webcore_string,
                                            true,
                                            request.match_case));
    if (result_range->collapsed(ec)) {
      if (!result_range->startContainer()->isInShadowTree())
        break;

      search_range = rangeOfContents(frame()->document());
      search_range->setStartAfter(
          result_range->startContainer()->shadowAncestorNode(), ec);
      continue;
    }

    // A non-collapsed result range can in some funky whitespace cases still not
    // advance the range's start position (4509328). Break to avoid infinite
    // loop. (This function is based on the implementation of
    // Frame::markAllMatchesForText, which is where this safeguard comes from).
    VisiblePosition new_start = endVisiblePosition(result_range.get(),
                                                   WebCore::DOWNSTREAM);
    if (new_start == startVisiblePosition(search_range.get(),
                                          WebCore::DOWNSTREAM))
      break;

    // Only treat the result as a match if it is visible
    if (frame()->editor()->insideVisibleArea(result_range.get())) {
      ++match_count;

      AddMarker(result_range.get());

      setStart(search_range.get(), new_start);
      Node* shadow_tree_root = search_range->shadowTreeRootNode();
      if (search_range->collapsed(ec) && shadow_tree_root)
        search_range->setEnd(shadow_tree_root,
                             shadow_tree_root->childNodeCount(), ec);

      // Catch a special case where Find found something but doesn't know what
      // the bounding box for it is. In this case we set the first match we find
      // as the active rect.
      IntRect result_bounds = result_range->boundingBox();
      IntRect active_selection_rect;
      if (locating_active_rect_) {
        active_selection_rect = active_match_.get() ?
            active_match_->boundingBox() : result_bounds;
      }

      // If the Find function found a match it will have stored where the
      // match was found in active_selection_rect_ on the current frame. If we
      // find this rect during scoping it means we have found the active
      // tickmark.
      if (locating_active_rect_ && (active_selection_rect == result_bounds)) {
        // We have found the active tickmark frame.
        main_frame_impl->active_match_frame_ = this;
        // We also know which tickmark is active now.
        active_match_index_ = match_count - 1;
        // To stop looking for the active tickmark, we set this flag.
        locating_active_rect_ = false;

  #if defined(OS_WIN)
        // TODO(pinkerton): Fix Mac invalidation to be more like Win ScrollView
        // Notify browser of new location for the selected rectangle.
        result_bounds.move(-frameview()->scrollOffset().width(),
                           -frameview()->scrollOffset().height());
        ReportFindInPageSelection(
            webkit_glue::FromIntRect(
                frame()->view()->convertToContainingWindow(result_bounds)),
                active_match_index_ + 1,
                request.request_id);
  #endif
      }
    }

    resume_scoping_from_range_ = result_range;
    timeout = (Time::Now() - start_time).InMilliseconds() >= kTimeout;
  } while (!timeout);

  // Remember what we search for last time, so we can skip searching if more
  // letters are added to the search string (and last outcome was 0).
  last_search_string_ = request.search_string;

  if (match_count > 0) {
    frame()->setMarkedTextMatchesAreHighlighted(true);

    last_match_count_ += match_count;

    // Let the mainframe know how much we found during this pass.
    main_frame_impl->IncreaseMatchCount(match_count, request.request_id);
  }

  if (timeout) {
    // If we found anything during this pass, we should redraw. However, we
    // don't want to spam too much if the page is extremely long, so if we
    // reach a certain point we start throttling the redraw requests.
    if (match_count > 0)
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

  // This frame is done, so show any scrollbar tickmarks we haven't drawn yet.
  InvalidateArea(INVALIDATE_SCROLLBAR);

  return;
}

void WebFrameImpl::CancelPendingScopingEffort() {
  scope_matches_factory_.RevokeAll();
  active_match_index_ = -1;
}

void WebFrameImpl::SetFindEndstateFocusAndSelection() {
  WebFrameImpl* main_frame_impl =
      static_cast<WebFrameImpl*>(GetView()->GetMainFrame());

  if (this == main_frame_impl->active_match_frame() &&
      active_match_.get()) {
    // If the user has changed the selection since the match was found, we
    // don't focus anything.
    VisibleSelection selection(frame()->selection()->selection());
    if (selection.isNone() || (selection.start() == selection.end()) ||
        active_match_->boundingBox() !=
            selection.toNormalizedRange()->boundingBox())
      return;

    // We will be setting focus ourselves, so we want the view to forget its
    // stored focus node so that it won't change it after we are done.
    static_cast<WebViewImpl*>(GetView())->ReleaseFocusReferences();

    // Try to find the first focusable node up the chain, which will, for
    // example, focus links if we have found text within the link.
    Node* node = active_match_->firstNode();
    while (node && !node->isFocusable() && node != frame()->document())
      node = node->parent();

    if (node && node != frame()->document()) {
      // Found a focusable parent node. Set focus to it.
      frame()->document()->setFocusedNode(node);
    } else {
      // Iterate over all the nodes in the range until we find a focusable node.
      // This, for example, sets focus to the first link if you search for
      // text and text that is within one or more links.
      node = active_match_->firstNode();
      while (node && node != active_match_->pastLastNode()) {
        if (node->isFocusable()) {
          frame()->document()->setFocusedNode(node);
          break;
        }
        node = node->traverseNextNode();
      }
    }
  }
}

void WebFrameImpl::StopFinding(bool clear_selection) {
  if (!clear_selection)
    SetFindEndstateFocusAndSelection();
  CancelPendingScopingEffort();

#if defined(OS_WIN)
  WebCore::RenderThemeChromiumWin::setFindInPageMode(false);
#endif

  // Remove all markers for matches found and turn off the highlighting.
  if (this == static_cast<WebFrameImpl*>(GetView()->GetMainFrame()))
    frame()->document()->removeMarkers(WebCore::DocumentMarker::TextMatch);
  frame()->setMarkedTextMatchesAreHighlighted(false);

  // Let the frame know that we don't want tickmarks or highlighting anymore.
  InvalidateArea(INVALIDATE_ALL);
}

void WebFrameImpl::SelectAll() {
  frame()->selection()->selectAll();

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

#if defined(OS_WIN)
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
#endif

void WebFrameImpl::Paste() {
  frame()->editor()->paste();

  WebViewDelegate* d = GetView()->GetDelegate();
  if (d)
    d->UserMetricsRecordAction(L"Paste");
}

void WebFrameImpl::Replace(const std::wstring& wtext) {
  String text = webkit_glue::StdWStringToString(wtext);
  RefPtr<DocumentFragment> fragment = createFragmentFromText(
      frame()->selection()->toNormalizedRange().get(), text);
  WebCore::applyCommand(WebCore::ReplaceSelectionCommand::create(
      frame()->document(), fragment.get(), false, true, true));
}

void WebFrameImpl::ToggleSpellCheck() {
  frame()->editor()->toggleContinuousSpellChecking();
}

bool WebFrameImpl::SpellCheckEnabled() {
  return frame()->editor()->isContinuousSpellCheckingEnabled();
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
  frame()->selection()->clear();
}

std::string WebFrameImpl::GetSelection(bool as_html) {
  RefPtr<Range> range = frame()->selection()->toNormalizedRange();
  if (!range.get())
    return std::string();

  if (as_html) {
    String markup = WebCore::createMarkup(range.get(), 0);
    return webkit_glue::StringToStdString(markup);
  } else {
    return webkit_glue::StringToStdString(range->text());
  }
}

void WebFrameImpl::CreateFrameView() {
  ASSERT(frame_);  // If frame_ doesn't exist, we probably didn't init properly.

  WebCore::Page* page = frame_->page();
  DCHECK(page);

  DCHECK(page->mainFrame() != NULL);

  bool is_main_frame = frame_ == page->mainFrame();
  if (is_main_frame && frame_->view())
    frame_->view()->setParentVisible(false);

  frame_->setView(0);

  WebCore::FrameView* view;
  if (is_main_frame) {
    IntSize initial_size(
        webview_impl_->size().width(), webview_impl_->size().height());
    view = new FrameView(frame_, initial_size);
  } else {
    view = new FrameView(frame_);
  }

  frame_->setView(view);

  // TODO(darin): The Mac code has a comment about this possibly being
  // unnecessary.  See installInFrame in WebCoreFrameBridge.mm
  if (frame_->ownerRenderer())
    frame_->ownerRenderer()->setWidget(view);

  if (HTMLFrameOwnerElement* owner = frame_->ownerElement()) {
    view->setCanHaveScrollbars(
        owner->scrollingMode() != WebCore::ScrollbarAlwaysOff);
  }

  if (is_main_frame)
    view->setParentVisible(true);

  // FrameViews are created with a refcount of 1 so it needs releasing after we
  // assign it to a RefPtr.
  view->deref();
}

// static
WebFrameImpl* WebFrameImpl::FromFrame(WebCore::Frame* frame) {
  return static_cast<WebFrameLoaderClient*>(
      frame->loader()->client())->webframe();
}

// WebFrame --------------------------------------------------------------------

void WebFrameImpl::Layout() {
  // layout this frame
  FrameView* view = frame_->view();
  if (view)
    view->layout();

  // recursively layout child frames
  Frame* child = frame_->tree()->firstChild();
  for (; child; child = child->tree()->nextSibling())
    FromFrame(child)->Layout();
}

void WebFrameImpl::Paint(skia::PlatformCanvas* canvas, const gfx::Rect& rect) {
  static StatsRate rendering("WebFramePaintTime");
  StatsScope<StatsRate> rendering_scope(rendering);

  if (!rect.IsEmpty()) {
    IntRect dirty_rect(rect.x(), rect.y(), rect.width(), rect.height());
#if defined(OS_MACOSX)
    CGContextRef context = canvas->getTopPlatformDevice().GetBitmapContext();
    GraphicsContext gc(context);
#else
    PlatformContextSkia context(canvas);

    // PlatformGraphicsContext is actually a pointer to PlatformContextSkia
    GraphicsContext gc(reinterpret_cast<PlatformGraphicsContext*>(&context));
#endif
    if (frame_->document() && frameview()) {
      frameview()->paint(&gc, dirty_rect);
      frame_->page()->inspectorController()->drawNodeHighlight(gc);
    } else {
      gc.fillRect(dirty_rect, Color::white);
    }
  }
}

bool WebFrameImpl::CaptureImage(scoped_ptr<skia::BitmapPlatformDevice>* image,
                                bool scroll_to_zero) {
  if (!image) {
    NOTREACHED();
    return false;
  }

  // Must layout before painting.
  Layout();

  skia::PlatformCanvas canvas;
  if (!canvas.initialize(frameview()->width(), frameview()->height(), true))
    return false;

#if defined(OS_WIN) || defined(OS_LINUX)
  PlatformContextSkia context(&canvas);
  GraphicsContext gc(reinterpret_cast<PlatformGraphicsContext*>(&context));
#elif defined(OS_MACOSX)
  CGContextRef context = canvas.beginPlatformPaint();
  GraphicsContext gc(context);
#endif
  frameview()->paint(&gc, IntRect(0, 0, frameview()->width(),
                                  frameview()->height()));
#if defined(OS_MACOSX)
  canvas.endPlatformPaint();
#endif

  skia::BitmapPlatformDevice& device =
      static_cast<skia::BitmapPlatformDevice&>(canvas.getTopPlatformDevice());

  image->reset(new skia::BitmapPlatformDevice(device));
  return true;
}

bool WebFrameImpl::IsLoading() {
  // I'm assuming this does what we want.
  return frame_->loader()->isLoading();
}

void WebFrameImpl::Closing() {
  alt_error_page_fetcher_.reset();
  webview_impl_ = NULL;
  frame_ = NULL;
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

void WebFrameImpl::ExecuteScript(const webkit_glue::WebScriptSource& source) {
  frame_->loader()->executeScript(
      WebCore::ScriptSourceCode(
          webkit_glue::StdStringToString(source.source),
          webkit_glue::GURLToKURL(source.url),
          source.start_line));
}

bool WebFrameImpl::InsertCSSStyles(const std::string& css) {
  Document* document = frame()->document();
  if (!document)
    return false;
  WebCore::Element* document_element = document->documentElement();
  if (!document_element)
    return false;

  RefPtr<WebCore::Element> stylesheet = document->createElement(
      WebCore::HTMLNames::styleTag, false);
  ExceptionCode err = 0;
  stylesheet->setTextContent(webkit_glue::StdStringToString(css), err);
  DCHECK(!err) << "Failed to set style element content";
  WebCore::Node* first = document_element->firstChild();
  bool success = document_element->insertBefore(stylesheet, first, err);
  DCHECK(success) << "Failed to insert stylesheet";
  return success;
}

void WebFrameImpl::ExecuteScriptInNewContext(
    const webkit_glue::WebScriptSource* sources_in, int num_sources) {
  Vector<WebCore::ScriptSourceCode> sources;

  for (int i = 0; i < num_sources; ++i) {
    sources.append(WebCore::ScriptSourceCode(
        webkit_glue::StdStringToString(sources_in[i].source),
        webkit_glue::GURLToKURL(sources_in[i].url),
        sources_in[i].start_line));
  }

  frame_->script()->evaluateInNewContext(sources);
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

bool WebFrameImpl::Visible() {
  return frame()->view()->visibleWidth() > 0 &&
         frame()->view()->visibleHeight() > 0;
}

PassRefPtr<Frame> WebFrameImpl::CreateChildFrame(
    const FrameLoadRequest& request, HTMLFrameOwnerElement* owner_element) {
  // TODO(darin): share code with initWithName()

  scoped_refptr<WebFrameImpl> webframe = new WebFrameImpl();

  // Add an extra ref on behalf of the Frame/FrameLoader, which references the
  // WebFrame via the FrameLoaderClient interface. See the comment at the top
  // of this file for more info.
  webframe->AddRef();

  RefPtr<Frame> child_frame = Frame::create(
      frame_->page(), owner_element, &webframe->frame_loader_client_);
  webframe->frame_ = child_frame.get();
  webframe->webview_impl_ = webview_impl_;

  child_frame->tree()->setName(request.frameName());

  frame_->tree()->appendChild(child_frame);

  // Frame::init() can trigger onload event in the parent frame,
  // which may detach this frame and trigger a null-pointer access
  // in FrameTree::removeChild. Move init() after appendChild call
  // so that webframe->frame_ is in the tree before triggering
  // onload event handler.
  // Because the event handler may set webframe->frame_ to null,
  // it is necessary to check the value after calling init() and
  // return without loading URL.
  // (b:791612)
  child_frame->init();      // create an empty document
  if (!child_frame->tree()->parent())
    return NULL;

  frame_->loader()->loadURLIntoChildFrame(
      request.resourceRequest().url(),
      request.resourceRequest().httpReferrer(),
      child_frame.get());
  // A synchronous navigation (about:blank) would have already processed
  // onload, so it is possible for the frame to have already been destroyed by
  // script in the page.
  if (!child_frame->tree()->parent())
    return NULL;

  return child_frame.release();
}

bool WebFrameImpl::ExecuteCoreCommandByName(const std::string& name,
                                            const std::string& value) {
  ASSERT(frame());
  return frame()->editor()->command(webkit_glue::StdStringToString(name))
      .execute(webkit_glue::StdStringToString(value));
}

bool WebFrameImpl::IsCoreCommandEnabled(const std::string& name) {
  ASSERT(frame());
  return frame()->editor()->command(webkit_glue::StdStringToString(name))
      .isEnabled();
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

  frame()->domWindow()->console()->addMessage(
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
  frame_->view()->setCanHaveScrollbars(flag);
}

bool WebFrameImpl::BeginPrint(const gfx::Size& page_size_px,
                              int* page_count) {
  DCHECK_EQ(frame()->document()->isFrameSet(), false);

  print_context_.reset(new ChromePrintContext(frame()));
  WebCore::FloatRect rect(0, 0,
                          static_cast<float>(page_size_px.width()),
                          static_cast<float>(page_size_px.height()));
  print_context_->begin(rect.width());
  float page_height;
  // We ignore the overlays calculation for now since they are generated in the
  // browser. page_height is actually an output parameter.
  print_context_->computePageRects(rect, 0, 0, 1.0, page_height);
  if (page_count)
    *page_count = print_context_->pageCount();
  return true;
}

float WebFrameImpl::PrintPage(int page, skia::PlatformCanvas* canvas) {
  // Ensure correct state.
  if (!print_context_.get() || page < 0 || !frame() || !frame()->document()) {
    NOTREACHED();
    return 0;
  }

#if defined(OS_WIN) || defined(OS_LINUX)
  PlatformContextSkia context(canvas);
  GraphicsContext spool(&context);
#elif defined(OS_MACOSX)
  CGContextRef context = canvas->beginPlatformPaint();
  GraphicsContext spool(context);
#endif

  return print_context_->spoolPage(spool, page);
}

void WebFrameImpl::EndPrint() {
  DCHECK(print_context_.get());
  if (print_context_.get())
    print_context_->end();
  print_context_.reset(NULL);
}

int WebFrameImpl::PendingFrameUnloadEventCount() const {
  return frame()->eventHandler()->pendingFrameUnloadEventCount();
}

void WebFrameImpl::RegisterPasswordListener(
    PassRefPtr<WebCore::HTMLInputElement> input_element,
    webkit_glue::PasswordAutocompleteListener* listener) {
  RefPtr<WebCore::HTMLInputElement> element = input_element;
  DCHECK(password_listeners_.find(element) == password_listeners_.end());
  password_listeners_.set(element, listener);
}

webkit_glue::PasswordAutocompleteListener* WebFrameImpl::GetPasswordListener(
    WebCore::HTMLInputElement* input_element) {
  return password_listeners_.get(input_element);
}

void WebFrameImpl::ClearPasswordListeners() {
  for (PasswordListenerMap::iterator iter = password_listeners_.begin();
       iter != password_listeners_.end(); ++iter) {
    delete iter->second;
  }
  password_listeners_.clear();
}
