// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include <string>
#include <vector>

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "Chrome.h"
#include "CString.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Element.h"
#include "HistoryItem.h"
#include "HTMLFormElement.h"  // needed by FormState.h
#include "HTMLFormControlElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "FormState.h"
#include "FrameLoader.h"
#include "FrameLoadRequest.h"
#include "FrameView.h"
#include "MIMETypeRegistry.h"
#include "MouseEvent.h"
#include "Page.h"
#include "PlatformString.h"
#include "PluginData.h"
#include "RefPtr.h"
#include "WindowFeatures.h"
MSVC_POP_WARNING();

#undef LOG
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#if defined(OS_WIN)
#include "webkit/activex_shim/activex_shared.h"
#endif
#include "webkit/glue/autofill_form.h"
#include "webkit/glue/alt_404_page_resource_fetcher.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/password_form_dom_manager.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/searchable_form_data.h"
#include "webkit/glue/webdatasource_impl.h"
#include "webkit/glue/webdocumentloader_impl.h"
#include "webkit/glue/weberror_impl.h"
#include "webkit/glue/webframeloaderclient_impl.h"
#include "webkit/glue/webhistoryitem_impl.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webplugin_impl.h"
#include "webkit/glue/webresponse_impl.h"
#include "webkit/glue/webview_delegate.h"
#include "webkit/glue/webview_impl.h"
#include "webkit/glue/weburlrequest.h"

using namespace WebCore;

// Domain for internal error codes.
static const char kInternalErrorDomain[] = "webkit_glue";

// An internal error code.  Used to note a policy change error resulting from
// dispatchDecidePolicyForMIMEType not passing the PolicyUse option.
enum {
  ERR_POLICY_CHANGE = -10000,
};

WebFrameLoaderClient::WebFrameLoaderClient(WebFrameImpl* frame) :
    webframe_(frame),
    postpone_loading_data_(false),
    has_representation_(false),
    plugin_widget_(NULL),
    sent_initial_response_to_plugin_(false),
    next_window_open_disposition_(IGNORE_ACTION) {
}

WebFrameLoaderClient::~WebFrameLoaderClient() {
}

void WebFrameLoaderClient::frameLoaderDestroyed() {
  // When the WebFrame was created, it had an extra reference given to it on
  // behalf of the Frame.  Since the WebFrame owns us, this extra ref also
  // serves to keep us alive until the FrameLoader is done with us.  The
  // FrameLoader calls this method when it's going away.  Therefore, we balance
  // out that extra reference, which may cause 'this' to be deleted.
  webframe_->Closing();
  webframe_->Release();
}

void WebFrameLoaderClient::windowObjectCleared() {
  WebViewImpl* webview = webframe_->webview_impl();
  WebViewDelegate* d = webview->delegate();
  if (d)
    d->WindowObjectCleared(webframe_);
}

void WebFrameLoaderClient::didPerformFirstNavigation() const {
}

void WebFrameLoaderClient::registerForIconNotification(bool listen){
}

bool WebFrameLoaderClient::hasWebView() const {
  return webframe_->webview_impl() != NULL;
}

bool WebFrameLoaderClient::hasFrameView() const {
  // The Mac port has this notion of a WebFrameView, which seems to be
  // some wrapper around an NSView.  Since our equivalent is HWND, I guess
  // we have a "frameview" whenever we have the toplevel HWND.
  return webframe_->webview_impl() != NULL;
}

void WebFrameLoaderClient::makeDocumentView() {
  webframe_->CreateFrameView();
}

void WebFrameLoaderClient::makeRepresentation(DocumentLoader*) {
  has_representation_ = true;
}

void WebFrameLoaderClient::forceLayout() {
  // FIXME
}
void WebFrameLoaderClient::forceLayoutForNonHTML() {
  // FIXME
}

void WebFrameLoaderClient::setCopiesOnScroll() {
  // FIXME
}

void WebFrameLoaderClient::detachedFromParent2() {
  // Nothing to do here.
}

void WebFrameLoaderClient::detachedFromParent3() {
  // Nothing to do here.
}

// This function is responsible for associating the |identifier| with a given
// subresource load.  The following functions that accept an |identifier| are
// called for each subresource, so they should not be dispatched to the
// WebFrame.
void WebFrameLoaderClient::assignIdentifierToInitialRequest(
    unsigned long identifier, DocumentLoader* loader,
    const ResourceRequest& request) {
  WebViewImpl* webview = webframe_->webview_impl();
  WebViewDelegate* d = webview->delegate();
  if (d) {
    WebRequestImpl webreq(request);
    d->AssignIdentifierToRequest(webview, identifier, webreq);
  }
}

// Determines whether the request being loaded by |loader| is a frame or a
// subresource. A subresource in this context is anything other than a frame --
// this includes images and xmlhttp requests.  It is important to note that a
// subresource is NOT limited to stuff loaded through the frame's subresource
// loader. Synchronous xmlhttp requests for example, do not go through the
// subresource loader, but we still label them as TargetIsSubResource.
//
// The important edge cases to consider when modifying this function are
// how synchronous resource loads are treated during load/unload threshold.
static ResourceRequest::TargetType DetermineTargetTypeFromLoader(
    DocumentLoader* loader) {
  if (loader == loader->frameLoader()->provisionalDocumentLoader()) {
    if (loader->frameLoader()->isLoadingMainFrame()) {
      return ResourceRequest::TargetIsMainFrame;
    } else {
      return ResourceRequest::TargetIsSubFrame;
    }
  }
  return ResourceRequest::TargetIsSubResource;
}

void WebFrameLoaderClient::dispatchWillSendRequest(
    DocumentLoader* loader, unsigned long identifier, ResourceRequest& request,
    const ResourceResponse& redirectResponse) {
  // We set the Frame on the ResourceRequest to provide load context to the
  // ResourceHandle implementation.
  request.setFrame(webframe_->frame());

  // We want to distinguish between a request for a document to be loaded into
  // the main frame, a sub-frame, or the sub-objects in that document.
  request.setTargetType(DetermineTargetTypeFromLoader(loader));

  // FrameLoader::loadEmptyDocumentSynchronously() creates an empty document
  // with no URL.  We don't like that, so we'll rename it to about:blank.
  if (request.url().isEmpty())
    request.setURL(KURL("about:blank"));
  if (request.mainDocumentURL().isEmpty())
    request.setMainDocumentURL(KURL("about:blank"));

  // Give the delegate a crack at the request.
  WebViewImpl* webview = webframe_->webview_impl();
  WebViewDelegate* d = webview->delegate();
  if (d) {
    WebRequestImpl webreq(request);
    d->WillSendRequest(webview, identifier, &webreq);
    request = webreq.frame_load_request().resourceRequest();
  }
}

bool WebFrameLoaderClient::shouldUseCredentialStorage(DocumentLoader*,
    unsigned long identifier) {
  // FIXME
  // Intended to pass through to a method on the resource load delegate.
  // If implemented, that method controls whether the browser should ask the
  // networking layer for a stored default credential for the page (say from
  // the Mac OS keychain). If the method returns false, the user should be
  // presented with an authentication challenge whether or not the networking
  // layer has a credential stored.
  // This returns true for backward compatibility: the ability to override the
  // system credential store is new. (Actually, not yet fully implemented in
  // WebKit, as of this writing.)
  return true;   
}

void WebFrameLoaderClient::dispatchDidReceiveAuthenticationChallenge(
    DocumentLoader*, unsigned long identifier, const AuthenticationChallenge&) {
  // FIXME
}

void WebFrameLoaderClient::dispatchDidCancelAuthenticationChallenge(
    DocumentLoader*, unsigned long identifier, const AuthenticationChallenge&) {
  // FIXME
}

void WebFrameLoaderClient::dispatchDidReceiveResponse(DocumentLoader* loader,
                                                      unsigned long identifier,
                                                      const ResourceResponse& response) {


  /* TODO(evanm): reenable this once we properly sniff XHTML from text/xml documents.
  // True if the request was for the page's main frame, or a subframe.
  bool is_frame = ResourceType::IsFrame(DetermineTargetTypeFromLoader(loader));
  if (is_frame &&
      response.httpStatusCode() == 200 &&
      mime_util::IsViewSourceMimeType(
          webkit_glue::CStringToStdString(response.mimeType().latin1()).c_str())) {
    loader->frame()->setInViewSourceMode();
  }*/

  // When the frame request first 404's, chrome may replace it with the alternate
  // 404 page's contents. It does this using substitute data in the document
  // loader, so the original response and url of the request can be preserved.
  // We need to avoid replacing the current page, if it has already been
  // replaced (otherwise could loop on setting alt-404 page!)
  bool is_substitute_data = loader->substituteData().isValid();

  // If it's a 404 page, we wait until we get 512 bytes of data before trying
  // to load the document.  This allows us to put up an alternate 404 page if
  // there's short text.
  ResourceRequest::TargetType target_type =
      DetermineTargetTypeFromLoader(loader);
  postpone_loading_data_ =
      ResourceRequest::TargetIsMainFrame == target_type &&
      !is_substitute_data &&
      response.httpStatusCode() == 404 &&
      GetAlt404PageUrl(loader).is_valid();
  if (postpone_loading_data_)
    postponed_data_.clear();

  // Cancel any pending loads.
  alt_404_page_fetcher_.reset(NULL);
}

void WebFrameLoaderClient::dispatchDidReceiveContentLength(DocumentLoader* loader,
                                                           unsigned long identifier,
                                                           int lengthReceived) {
  // FIXME
}

// Called when a particular resource load completes
void WebFrameLoaderClient::dispatchDidFinishLoading(DocumentLoader* loader,
                                                    unsigned long identifier) {
  if (postpone_loading_data_) {
    // The server returned a 404 and the content was < 512 bytes (which we
    // suppressed).  Go ahead and fetch the alternate page content.
    const GURL& url = GetAlt404PageUrl(loader);
    DCHECK(url.is_valid()) <<
        "URL changed? It was valid in dispatchDidReceiveResponse.";
    alt_404_page_fetcher_.reset(new Alt404PageResourceFetcher(this,
        webframe_->frame(), loader, url));
  }

  WebViewImpl* webview = webframe_->webview_impl();
  WebViewDelegate* d = webview->delegate();
  if (d)
    d->DidFinishLoading(webview, identifier);
}

GURL WebFrameLoaderClient::GetAlt404PageUrl(DocumentLoader* loader) {
  WebViewImpl* webview = webframe_->webview_impl();
  WebViewDelegate* d = webview->delegate();
  if (!d)
    return GURL();

  const GURL& failedURL = webkit_glue::KURLToGURL(loader->url());

  // If trying to view source on a 404 page, just show the original page
  // content.
  if (webframe_->frame()->inViewSourceMode())
    return GURL();

  // Construct the URL to fetch from the alt error page server. "html404"
  // is understood by the link doctor server.
  return d->GetAlternateErrorPageURL(failedURL, WebViewDelegate::HTTP_404);
}

void WebFrameLoaderClient::Alt404PageFinished(DocumentLoader* loader,
                                              const std::string& html) {
  if (html.length() > 0) {
    // TODO(tc): Handle backoff on so we don't hammer the alt error page
    // servers.
    webframe_->LoadHTMLString(html, webkit_glue::KURLToGURL(loader->url()));
  } else {
    // Fall back on original text
    webframe_->LoadHTMLString(postponed_data_,
                              webkit_glue::KURLToGURL(loader->url()));
  }
}

void WebFrameLoaderClient::dispatchDidFailLoading(DocumentLoader* loader,
                                                  unsigned long identifier,
                                                  const ResourceError& error) {
  WebViewImpl* webview = webframe_->webview_impl();
  if (webview && webview->delegate()) {
    webview->delegate()->DidFailLoadingWithError(webview, identifier,
                                                 WebErrorImpl(error));
  }
}

void WebFrameLoaderClient::dispatchDidFinishDocumentLoad() {
  WebViewImpl* webview = webframe_->webview_impl();
  WebViewDelegate* d = webview->delegate();

  // A frame may be reused.  This call ensures we don't hold on to our password
  // listeners and their associated HTMLInputElements.
  webframe_->ClearPasswordListeners();

  // The document has now been fully loaded.
  // Scan for password forms to be sent to the browser
  PassRefPtr<WebCore::HTMLCollection> forms =
      webframe_->frame()->document()->forms();

  std::vector<PasswordForm> passwordForms;

  unsigned int form_count = forms->length();
  for (unsigned int i = 0; i < form_count; ++i) {
    // Strange but true, sometimes item can be NULL.
    WebCore::Node* item = forms->item(i);
    if (item) {
      WebCore::HTMLFormElement* form =
          static_cast<WebCore::HTMLFormElement*>(item);

      // Honour autocomplete=off.
      if (!form->autoComplete())
        continue;

      scoped_ptr<PasswordForm> passwordFormPtr(
          PasswordFormDomManager::CreatePasswordForm(form));

      if (passwordFormPtr.get())
        passwordForms.push_back(*passwordFormPtr);
    }
  }

  if (d && (passwordForms.size() > 0))
    d->OnPasswordFormsSeen(webview, passwordForms);
  if (d)
    d->DidFinishDocumentLoadForFrame(webview, webframe_);
}

bool WebFrameLoaderClient::dispatchDidLoadResourceFromMemoryCache(
    DocumentLoader* loader,
    const ResourceRequest& request,
    const ResourceResponse& response,
    int length) {
  WebViewImpl* webview = webframe_->webview_impl();
  WebViewDelegate* d = webview->delegate();
  if (d) {
    WebRequestImpl webreq(request);
    WebResponseImpl webresp(response);
    return d->DidLoadResourceFromMemoryCache(webview, webreq, webresp,
                                             webframe_);
  }

  return false;
}

void WebFrameLoaderClient::dispatchDidHandleOnloadEvents() {
  // During the onload event of a subframe, the subframe can be removed.  In
  // that case, it has no page.  This is covered by
  // LayoutTests/fast/dom/replaceChild.html
  if (!webframe_->frame()->page())
    return;
  WebViewImpl* webview = webframe_->webview_impl();
  WebViewDelegate* d = webview->delegate();
  if (d)
    d->DidHandleOnloadEventsForFrame(webview, webframe_);
}

// Redirect Tracking
// =================
// We want to keep track of the chain of redirects that occur during page
// loading. There are two types of redirects, server redirects which are HTTP
// response codes, and client redirects which are document.location= and meta
// refreshes.
//
// This outlines the callbacks that we get in different redirect situations,
// and how each call modifies the redirect chain.
//
// Normal page load
// ----------------
//   dispatchDidStartProvisionalLoad() -> adds URL to the redirect list
//   dispatchDidCommitLoad()           -> DISPATCHES & clears list
//
// Server redirect (success)
// -------------------------
//   dispatchDidStartProvisionalLoad()                    -> adds source URL
//   dispatchDidReceiveServerRedirectForProvisionalLoad() -> adds dest URL
//   dispatchDidCommitLoad()                              -> DISPATCHES
//
// Client redirect (success)
// -------------------------
//   (on page)
//   dispatchWillPerformClientRedirect() -> saves expected redirect
//   dispatchDidStartProvisionalLoad()   -> appends redirect source (since
//                                          it matches the expected redirect)
//                                          and the current page as the dest)
//   dispatchDidCancelClientRedirect()   -> clears expected redirect
//   dispatchDidCommitLoad()             -> DISPATCHES
//
// Client redirect (cancelled)
// (e.g meta-refresh trumped by manual doc.location change, or just cancelled
// because a link was clicked that requires the meta refresh to be rescheduled
// (the SOURCE URL may have changed).
// ---------------------------
//   dispatchDidCancelClientRedirect()                 -> clears expected redirect
//   dispatchDidStartProvisionalLoad()                 -> adds only URL to redirect list
//   dispatchDidCommitLoad()                           -> DISPATCHES & clears list
//   rescheduled ? dispatchWillPerformClientRedirect() -> saves expected redirect
//               : nothing

// Client redirect (failure)
// -------------------------
//   (on page)
//   dispatchWillPerformClientRedirect() -> saves expected redirect
//   dispatchDidStartProvisionalLoad()   -> appends redirect source (since
//                                          it matches the expected redirect)
//                                          and the current page as the dest)
//   dispatchDidCancelClientRedirect()
//   dispatchDidFailProvisionalLoad()
//
// Load 1 -> Server redirect to 2 -> client redirect to 3 -> server redirect to 4
// ------------------------------------------------------------------------------
//   dispatchDidStartProvisionalLoad()                    -> adds source URL 1
//   dispatchDidReceiveServerRedirectForProvisionalLoad() -> adds dest URL 2
//   dispatchDidCommitLoad()                              -> DISPATCHES 1+2
//    -- begin client redirect and NEW DATA SOURCE
//   dispatchWillPerformClientRedirect()                  -> saves expected redirect
//   dispatchDidStartProvisionalLoad()                    -> appends URL 2 and URL 3
//   dispatchDidReceiveServerRedirectForProvisionalLoad() -> appends destination URL 4
//   dispatchDidCancelClientRedirect()                    -> clears expected redirect
//   dispatchDidCommitLoad()                              -> DISPATCHES
//
// Interesting case with multiple location changes involving anchors.
// Load page 1 containing future client-redirect (back to 1, e.g meta refresh) > Click
// on a link back to the same page (i.e an anchor href) >
// client-redirect finally fires (with new source, set to 1#anchor)
// -----------------------------------------------------------------------------
//   dispatchWillPerformClientRedirect(non-zero 'interval' param) -> saves expected redirect
//   -- click on anchor href
//   dispatchDidCancelClientRedirect()                            -> clears expected redirect
//   dispatchDidStartProvisionalLoad()                            -> adds 1#anchor source
//   dispatchDidCommitLoad()                                      -> DISPATCHES 1#anchor
//   dispatchWillPerformClientRedirect()                          -> saves exp. source (1#anchor)
//   -- redirect timer fires
//   dispatchDidStartProvisionalLoad()                            -> appends 1#anchor (src) and 1 (dest)
//   dispatchDidCancelClientRedirect()                            -> clears expected redirect
//   dispatchDidCommitLoad()                                      -> DISPATCHES 1#anchor + 1
//
void WebFrameLoaderClient::dispatchDidReceiveServerRedirectForProvisionalLoad() {
  WebDataSourceImpl* ds = webframe_->GetProvisionalDataSourceImpl();
  if (!ds) {
    NOTREACHED() << "Got a server redirect when there is no provisional DS";
    return;
  }

  // A provisional load should have started already, which should have put an
  // entry in our redirect chain.
  DCHECK(!ds->GetRedirectChain().empty());

  // The URL of the destination is on the provisional data source. We also need
  // to update the redirect chain to account for this addition (we do this
  // before the callback so the callback can look at the redirect chain to see
  // what happened).
  ds->AppendRedirect(ds->GetRequest().GetURL());

  // Dispatch callback
  WebViewImpl* webview = webframe_->webview_impl();
  WebViewDelegate* d = webview->delegate();
  if (d)
    d->DidReceiveProvisionalLoadServerRedirect(webview, webframe_);
}

// Called on both success and failure of a client redirect.
void WebFrameLoaderClient::dispatchDidCancelClientRedirect() {
  // No longer expecting a client redirect.
  WebViewImpl* webview = webframe_->webview_impl();
  WebViewDelegate* d = webview ? webview->delegate() : NULL;
  if (d) {
    expected_client_redirect_src_ = GURL();
    expected_client_redirect_dest_ = GURL();

    d->DidCancelClientRedirect(webview, webframe_);
  }

  // No need to clear the redirect chain, since that data source has already
  // been deleted by the time this function is called.
}

void WebFrameLoaderClient::dispatchWillPerformClientRedirect(const KURL& url,
                                                             double interval,
                                                             double fire_date) {
  // Tells dispatchDidStartProvisionalLoad that if it sees this item it is a
  // redirect and the source item should be added as the start of the chain.
  WebViewImpl* webview = webframe_->webview_impl();
  WebViewDelegate* d = webview ? webview->delegate() : NULL;
  if (d) {
    expected_client_redirect_src_ = webframe_->GetURL();
    expected_client_redirect_dest_ = webkit_glue::KURLToGURL(url);

    // TODO(timsteele): bug 1135512. Webkit does not properly notify us of
    // cancelling http > file client redirects. Since the FrameLoader's policy
    // is to never carry out such a navigation anyway, the best thing we can do
    // for now to not get confused is ignore this notification.
    if (expected_client_redirect_dest_.SchemeIsFile() &&
       (expected_client_redirect_src_.SchemeIs("http") ||
        expected_client_redirect_src_.SchemeIsSecure())) {
      expected_client_redirect_src_ = GURL();
      expected_client_redirect_dest_ = GURL();
      return;
    }

    d->WillPerformClientRedirect(webview,
                                 webframe_,
                                 expected_client_redirect_src_,
                                 expected_client_redirect_dest_,
                                 static_cast<unsigned int>(interval),
                                 static_cast<unsigned int>(fire_date));
  }
}

void WebFrameLoaderClient::dispatchDidChangeLocationWithinPage() {
  // Anchor fragment navigations are not normal loads, so we need to synthesize
  // some events for our delegate.
  WebViewImpl* webview = webframe_->webview_impl();
  WebViewDelegate* d = webview->delegate();
  if (d)
    d->DidStartLoading(webview);

  WebDataSourceImpl* ds = webframe_->GetDataSourceImpl();
  DCHECK(ds) << "DataSource NULL when navigating to reference fragment";
  if (ds) {
    GURL url = ds->GetRequest().GetURL();
    GURL chain_end = ds->GetRedirectChain().back();
    ds->ClearRedirectChain();

    // Figure out if this location change is because of a JS-initiated client
    // redirect (e.g onload/setTimeout document.location.href=).
    // TODO(timsteele): (bugs 1085325, 1046841) We don't get proper redirect
    // performed/cancelled notifications across anchor navigations, so the
    // other redirect-tracking code in this class (see dispatch*ClientRedirect()
    // and dispatchDidStartProvisionalLoad) is insufficient to catch and
    // properly flag these transitions. Once a proper fix for this bug is
    // identified and applied the following block may no longer be required.
    bool was_client_redirect =
        ((url == expected_client_redirect_dest_) &&
         (chain_end == expected_client_redirect_src_)) ||
        (NavigationGestureForLastLoad() == NavigationGestureAuto);

    if (was_client_redirect) {
      if (d)
        d->DidCompleteClientRedirect(webview, webframe_, chain_end);
      ds->AppendRedirect(chain_end);
      // Make sure we clear the expected redirect since we just effectively
      // completed it.
      expected_client_redirect_src_ = GURL();
      expected_client_redirect_dest_ = GURL();
     }

    // Regardless of how we got here, we are navigating to a URL so we need to
    // add it to the redirect chain.
    ds->AppendRedirect(url);

    // WebKit will re-use requests for in-page navigations, but we want to
    // think of it as a new request that has a page ID in session history.
    // This will set the proper page ID, etc. on the request so that the
    // browser will treat it properly.
    webframe_->CacheCurrentRequestInfo(ds);
  }

  bool is_new_navigation;
  webview->DidCommitLoad(&is_new_navigation);
  if (d) {
    d->DidChangeLocationWithinPageForFrame(webview, webframe_,
                                           is_new_navigation);
  }

  if (d)
    d->DidStopLoading(webview);
}

void WebFrameLoaderClient::dispatchWillClose() {
  WebViewImpl* webview = webframe_->webview_impl();
  // Make sure WebViewImpl releases the references it uses to restore focus.
  // If we didn't do this, WebViewImpl might try to restore focus to an invalid
  // element.
  webview->ReleaseFocusReferences();
  WebViewDelegate* d = webview->delegate();
  if (d)
    d->WillCloseFrame(webview, webframe_);
}

void WebFrameLoaderClient::dispatchDidReceiveIcon() {
  WebViewImpl* webview = webframe_->webview_impl();
  WebViewDelegate* d = webview->delegate();
  if (d)
    d->DidReceiveIconForFrame(webview, webframe_);
}

void WebFrameLoaderClient::dispatchDidStartProvisionalLoad() {
  // In case a redirect occurs, we need this to be set so that the redirect
  // handling code can tell where the redirect came from. Server redirects
  // will occur on the provisional load, so we need to keep track of the most
  // recent provisional load URL.
  // See dispatchDidReceiveServerRedirectForProvisionalLoad.
  WebDataSourceImpl* ds = webframe_->GetProvisionalDataSourceImpl();
  if (!ds) {
    NOTREACHED() << "Attempting to provisional load but there isn't one";
    return;
  }
  const GURL& url = ds->GetRequest().GetURL();

  // Since the provisional load just started, we should have not gotten
  // any redirects yet.
  DCHECK(ds->GetRedirectChain().empty());

  WebViewImpl* webview = webframe_->webview_impl();
  WebViewDelegate* d = webview->delegate();
  // If this load is what we expected from a client redirect, treat it as a
  // redirect from that original page. The expected redirect urls will be
  // cleared by DidCancelClientRedirect.
  bool completing_client_redirect = false;
  if (expected_client_redirect_src_.is_valid()) {
    // expected_client_redirect_dest_ could be something like
    // "javascript:history.go(-1)" thus we need to exclude url starts with
    // "javascript:". See bug: 1080873
    DCHECK(expected_client_redirect_dest_.SchemeIs("javascript") ||
           expected_client_redirect_dest_ == url);
    ds->AppendRedirect(expected_client_redirect_src_);
    completing_client_redirect = true;
  }
  ds->AppendRedirect(url);

  if (d) {
    // As the comment for DidCompleteClientRedirect in webview_delegate.h
    // points out, whatever information its invocation contains should only
    // be considered relevant until the next provisional load has started.
    // So we first tell the delegate that the load started, and then tell it
    // about the client redirect the load is responsible for completing.
    d->DidStartProvisionalLoadForFrame(webview, webframe_,
                                       NavigationGestureForLastLoad());
    if (completing_client_redirect)
      d->DidCompleteClientRedirect(webview, webframe_,
                                   expected_client_redirect_src_);
  }

  // Cancel any pending loads.
  if (alt_404_page_fetcher_.get())
    alt_404_page_fetcher_->Cancel();
}

NavigationGesture WebFrameLoaderClient::NavigationGestureForLastLoad() {
  // TODO(timsteele): userGestureHint returns too many false positives
  // (see bug 1051891) to trust it and assign NavigationGestureUser, so
  // for now we assign Unknown in those cases and Auto otherwise.
  // (Issue 874811 known false negative as well).
  return webframe_->frame()->loader()->userGestureHint() ?
      NavigationGestureUnknown :
      NavigationGestureAuto;
}

void WebFrameLoaderClient::dispatchDidReceiveTitle(const String& title) {
  WebViewImpl* webview = webframe_->webview_impl();
  WebViewDelegate* d = webview->delegate();
  if (d) {
    d->DidReceiveTitle(webview, webkit_glue::StringToStdWString(title),
                       webframe_);
  }
}

void WebFrameLoaderClient::dispatchDidCommitLoad() {
  WebViewImpl* webview = webframe_->webview_impl();

  bool is_new_navigation;
  webview->DidCommitLoad(&is_new_navigation);
  WebViewDelegate* d = webview->delegate();
  if (d)
    d->DidCommitLoadForFrame(webview, webframe_, is_new_navigation);
}

void WebFrameLoaderClient::dispatchDidFailProvisionalLoad(
    const ResourceError& error) {
  // If a policy change occured, then we do not want to inform the plugin
  // delegate.  See bug 907789 for details.
  if (error.domain() == kInternalErrorDomain &&
      error.errorCode() == ERR_POLICY_CHANGE) {
    webframe_->DidFail(cancelledError(error.failingURL()), true);
  } else {
    webframe_->DidFail(error, true);
    WebPluginDelegate* plg_delegate = webframe_->plugin_delegate();
    if (plg_delegate)
      plg_delegate->DidFinishLoadWithReason(NPRES_NETWORK_ERR);
  }
}

void WebFrameLoaderClient::dispatchDidFailLoad(const ResourceError& error) {
  webframe_->DidFail(error, false);

  WebPluginDelegate* plg_delegate = webframe_->plugin_delegate();
  if (plg_delegate)
    plg_delegate->DidFinishLoadWithReason(NPRES_NETWORK_ERR);

  // Don't clear the redirect chain, this will happen in the middle of client
  // redirects, and we need the context. The chain will be cleared when the
  // provisional load succeeds or fails, not the "real" one.
}

void WebFrameLoaderClient::dispatchDidFinishLoad() {
  WebViewImpl* webview = webframe_->webview_impl();
  WebViewDelegate* d = webview->delegate();
  if (d)
    d->DidFinishLoadForFrame(webview, webframe_);
  WebPluginDelegate* plg_delegate = webframe_->plugin_delegate();
  if (plg_delegate)
    plg_delegate->DidFinishLoadWithReason(NPRES_DONE);

  // Don't clear the redirect chain, this will happen in the middle of client
  // redirects, and we need the context. The chain will be cleared when the
  // provisional load succeeds or fails, not the "real" one.
}

void WebFrameLoaderClient::dispatchDidFirstLayout() {
 // FIXME: called when webkit finished layout of page.
 // All resources have not necessarily finished loading.
}

void WebFrameLoaderClient::dispatchDidFirstVisuallyNonEmptyLayout() {
  // FIXME: called when webkit finished layout of a page that was visually
  // non-empty.
  // All resources have not necessarily finished loading.
}

Frame* WebFrameLoaderClient::dispatchCreatePage() {
  struct WebCore::WindowFeatures features;
  Page* new_page = webframe_->frame()->page()->chrome()->createWindow(
      webframe_->frame(), FrameLoadRequest(), features);

  // Make sure that we have a valid disposition.  This should have been set in
  // the preceeding call to dispatchDecidePolicyForNewWindowAction.
  DCHECK(next_window_open_disposition_ != IGNORE_ACTION);
  WindowOpenDisposition disp = next_window_open_disposition_;
  next_window_open_disposition_ = IGNORE_ACTION;

  // createWindow can return NULL (e.g., popup blocker denies the window).
  if (!new_page)
    return NULL;

  WebViewImpl::FromPage(new_page)->set_window_open_disposition(disp);
  return new_page->mainFrame();
}

void WebFrameLoaderClient::dispatchShow() {
  WebViewImpl* webview = webframe_->webview_impl();
  WebViewDelegate* d = webview->delegate();
  if (d)
    d->Show(webview, webview->window_open_disposition());
}

static bool TreatAsAttachment(const ResourceResponse& response) {
  const String& content_disposition =
      response.httpHeaderField("Content-Disposition");
  if (content_disposition.isEmpty())
    return false;

  // Some broken sites just send
  // Content-Disposition: ; filename="file"
  // screen those out here.
  if (content_disposition.startsWith(";"))
    return false;

  if (content_disposition.startsWith("inline", false))
    return false;

  // Some broken sites just send
  // Content-Disposition: filename="file"
  // without a disposition token... screen those out.
  if (content_disposition.startsWith("filename", false))
    return false;

  // Also in use is Content-Disposition: name="file"
  if (content_disposition.startsWith("name", false))
    return false;

  // We have a content-disposition of "attachment" or unknown.
  // RFC 2183, section 2.8 says that an unknown disposition
  // value should be treated as "attachment"
  return true;
}

void WebFrameLoaderClient::dispatchDecidePolicyForMIMEType(
     FramePolicyFunction function,
     const String& mime_type,
     const ResourceRequest&) {
  const ResourceResponse& response =
      webframe_->frame()->loader()->activeDocumentLoader()->response();

  PolicyAction action;

  int status_code = response.httpStatusCode();
  if (status_code == 204 || status_code == 205) {
    // The server does not want us to replace the page contents.
    action = PolicyIgnore;
  } else if (TreatAsAttachment(response)) {
    // The server wants us to download instead of replacing the page contents.
    // Downloading is handled by the embedder, but we still get the initial
    // response so that we can ignore it and clean up properly.
    action = PolicyIgnore;
  } else if (!canShowMIMEType(mime_type)) {
    // Make sure that we can actually handle this type internally.
    action = PolicyIgnore;
  } else {
    // OK, we will render this page.
    action = PolicyUse;
  }

  // NOTE: ERR_POLICY_CHANGE will be generated when action is not PolicyUse.
  (webframe_->frame()->loader()->*function)(action);
}

void WebFrameLoaderClient::dispatchDecidePolicyForNewWindowAction(
    WebCore::FramePolicyFunction function,
    const WebCore::NavigationAction& action,
    const WebCore::ResourceRequest& request,
    PassRefPtr<WebCore::FormState> form_state,
    const WebCore::String& frame_name) {
  WindowOpenDisposition disposition;
  if (!ActionSpecifiesDisposition(action, &disposition))
    disposition = NEW_FOREGROUND_TAB;

  PolicyAction policy_action;
  if (disposition == SAVE_TO_DISK) {
    policy_action = PolicyDownload;
  } else {
    policy_action = PolicyUse;

    // Remember the disposition for when dispatchCreatePage is called.  It is
    // unfortunate that WebCore does not provide us with any context when
    // creating or showing the new window that would allow us to avoid having
    // to keep this state.
    next_window_open_disposition_ = disposition;
  }
  (webframe_->frame()->loader()->*function)(policy_action);
}

// Conversion.
static WebNavigationType NavigationTypeToWebNavigationType(
    WebCore::NavigationType t) {
  switch (t) {
    case WebCore::NavigationTypeLinkClicked:
      return WebNavigationTypeLinkClicked;
    case WebCore::NavigationTypeFormSubmitted:
      return WebNavigationTypeFormSubmitted;
    case WebCore::NavigationTypeBackForward:
      return WebNavigationTypeBackForward;
    case WebCore::NavigationTypeReload:
      return WebNavigationTypeReload;
    case WebCore::NavigationTypeFormResubmitted:
      return WebNavigationTypeFormResubmitted;
    default:
    case WebCore::NavigationTypeOther:
      return WebNavigationTypeOther;
  }
}

void WebFrameLoaderClient::dispatchDecidePolicyForNavigationAction(
    WebCore::FramePolicyFunction function,
    const WebCore::NavigationAction& action,
    const WebCore::ResourceRequest& request,
    PassRefPtr<WebCore::FormState> form_state) {
  PolicyAction policy_action = PolicyUse;

  WebViewImpl* wv = webframe_->webview_impl();
  WebViewDelegate* d = wv->delegate();
  // It is valid for this function to be invoked in code paths where the
  // the webview is closed.
  // The NULL check here is to fix a crash that seems strange
  // (see - https://bugs.webkit.org/show_bug.cgi?id=23554).
  if (d && !request.url().isNull()) {
    WindowOpenDisposition disposition = CURRENT_TAB;
    ActionSpecifiesDisposition(action, &disposition);

    // Give the delegate a chance to change the disposition.  When we do not
    // have a provisional data source here, it means that we are scrolling to
    // an anchor in the page.  We don't need to ask the WebViewDelegate about
    // such navigations.
    const WebDataSourceImpl* ds = webframe_->GetProvisionalDataSourceImpl();
    if (ds) {
      bool is_redirect = !ds->GetRedirectChain().empty();

      WebNavigationType webnav_type =
          NavigationTypeToWebNavigationType(action.type());

      disposition = d->DispositionForNavigationAction(
          wv, webframe_, &ds->GetRequest(), webnav_type, disposition, is_redirect);

      if (disposition != IGNORE_ACTION) {
        if (disposition == CURRENT_TAB) {
          policy_action = PolicyUse;
        } else if (disposition == SAVE_TO_DISK) {
          policy_action = PolicyDownload;
        } else {
          GURL referrer = webkit_glue::StringToGURL(
              request.httpHeaderField("Referer"));

          d->OpenURL(webframe_->webview_impl(),
                     webkit_glue::KURLToGURL(request.url()),
                     referrer,
                     disposition);
          policy_action = PolicyIgnore;
        }
      } else {
        policy_action = PolicyIgnore;
      }
    }
  } else {
    policy_action = PolicyIgnore;
  }

  (webframe_->frame()->loader()->*function)(policy_action);
}

void WebFrameLoaderClient::cancelPolicyCheck() {
  // FIXME
}

void WebFrameLoaderClient::dispatchUnableToImplementPolicy(const ResourceError&) {
  // FIXME
}

void WebFrameLoaderClient::dispatchWillSubmitForm(FramePolicyFunction function,
    PassRefPtr<FormState> form_ref) {
  SearchableFormData* form_data = SearchableFormData::Create(form_ref->form());
  WebDocumentLoaderImpl* loader = static_cast<WebDocumentLoaderImpl*>(
      webframe_->frame()->loader()->provisionalDocumentLoader());
  // Don't free the SearchableFormData, the loader will do that.
  loader->set_searchable_form_data(form_data);

  PasswordForm* pass_data =
      PasswordFormDomManager::CreatePasswordForm(form_ref->form());
  // Don't free the PasswordFormData, the loader will do that.
  loader->set_password_form_data(pass_data);

  WebViewImpl* webview = webframe_->webview_impl();
  WebViewDelegate* d = webview->delegate();

  // Unless autocomplete=off, record what the user put in it for future
  // autofilling.
  if (form_ref->form()->autoComplete()) {
    scoped_ptr<AutofillForm> autofill_form(
        AutofillForm::CreateAutofillForm(form_ref->form()));
    if (autofill_form.get()) {
      d->OnAutofillFormSubmitted(webview, *autofill_form);
    }
  }

  loader->set_form_submit(true);

  (webframe_->frame()->loader()->*function)(PolicyUse);
}

void WebFrameLoaderClient::dispatchDidLoadMainResource(DocumentLoader*) {
  // FIXME
}

void WebFrameLoaderClient::revertToProvisionalState(DocumentLoader*) {
  has_representation_ = true;
}

void WebFrameLoaderClient::setMainDocumentError(DocumentLoader*,
                                                const ResourceError& error) {
  if (plugin_widget_) {
    if (sent_initial_response_to_plugin_) {
      plugin_widget_->didFail(error);
      sent_initial_response_to_plugin_ = false;
    }
    plugin_widget_ = NULL;
  }
}

void WebFrameLoaderClient::postProgressStartedNotification() {
  if (hasWebView()) {
    WebViewDelegate* d = webframe_->webview_impl()->delegate();
    if (d)
      d->DidStartLoading(webframe_->webview_impl());
  }
}

void WebFrameLoaderClient::postProgressEstimateChangedNotification() {
  // FIXME
}

void WebFrameLoaderClient::postProgressFinishedNotification() {
  // TODO(ericroman): why might webframe_->webview_impl be null?
  // http://b/1234461
  if (hasWebView()) {
    WebViewDelegate* d = webframe_->webview_impl()->delegate();

    if (d)
      d->DidStopLoading(webframe_->webview_impl());
  }
}

void WebFrameLoaderClient::setMainFrameDocumentReady(bool ready) {
  // FIXME
}

// Creates a new connection and begins downloading from that (contrast this
// with |download|).
void WebFrameLoaderClient::startDownload(const ResourceRequest& request) {
  WebViewDelegate* d = webframe_->webview_impl()->delegate();
  if (d) {
    const GURL url(webkit_glue::KURLToGURL(request.url()));
    const GURL referrer(webkit_glue::StringToStdString(request.httpReferrer()));
    d->DownloadUrl(url, referrer);
  }
}

void WebFrameLoaderClient::willChangeTitle(DocumentLoader*) {
  // FIXME
}
void WebFrameLoaderClient::didChangeTitle(DocumentLoader*) {
  // FIXME
}

// Called whenever data is received.
void WebFrameLoaderClient::committedLoad(DocumentLoader* loader, const char* data, int length) {
  if (!plugin_widget_) {
    if (postpone_loading_data_) {
      postponed_data_.append(data, length);
      if (postponed_data_.length() >= 512) {
        postpone_loading_data_ = false;
        webframe_->DidReceiveData(loader, postponed_data_.c_str(),
                              static_cast<int>(postponed_data_.length()));
      }
      return;
    }
    webframe_->DidReceiveData(loader, data, length);
  }

  // The plugin widget could have been created in the webframe_->DidReceiveData
  // function.
  if (plugin_widget_) {
    if (!sent_initial_response_to_plugin_) {
      sent_initial_response_to_plugin_ = true;
      plugin_widget_->didReceiveResponse(
          webframe_->frame()->loader()->activeDocumentLoader()->response());
    }
    plugin_widget_->didReceiveData(data, length);
  }
}

void WebFrameLoaderClient::finishedLoading(DocumentLoader* dl) {
  if (plugin_widget_) {
    plugin_widget_->didFinishLoading();
    plugin_widget_ = NULL;
    sent_initial_response_to_plugin_ = false;
  } else {
    // This is necessary to create an empty document. See bug 634004.
    // However, we only want to do this if makeRepresentation has been called, to
    // match the behavior on the Mac.
    if (has_representation_)
      dl->frameLoader()->setEncoding("", false);
  }
}

void WebFrameLoaderClient::updateGlobalHistory() {
}

bool WebFrameLoaderClient::shouldGoToHistoryItem(HistoryItem*) const {
  // FIXME
  return true;
}

ResourceError WebFrameLoaderClient::blockedError(const WebCore::ResourceRequest&) {
  // FIXME
  return ResourceError();
}

ResourceError WebFrameLoaderClient::cancelledError(
    const ResourceRequest& request) {
  return ResourceError(net::kErrorDomain, net::ERR_ABORTED,
                       request.url().string(), String());
}
ResourceError WebFrameLoaderClient::cannotShowURLError(const ResourceRequest&) {
  // FIXME
  return ResourceError();
}
ResourceError WebFrameLoaderClient::interruptForPolicyChangeError(
    const ResourceRequest& request) {
  return ResourceError(kInternalErrorDomain, ERR_POLICY_CHANGE,
                       request.url().string(), String());
}

ResourceError WebFrameLoaderClient::cannotShowMIMETypeError(const ResourceResponse&) {
  // FIXME
  return ResourceError();
}

ResourceError WebFrameLoaderClient::fileDoesNotExistError(const ResourceResponse&) {
  // FIXME
  return ResourceError();
}

ResourceError WebFrameLoaderClient::pluginWillHandleLoadError(const WebCore::ResourceResponse&) {
  // FIXME
  return ResourceError();
}

bool WebFrameLoaderClient::shouldFallBack(const ResourceError& error) {
  // This method is called when we fail to load the URL for an <object> tag
  // that has fallback content (child elements) and is being loaded as a frame.
  // The error parameter indicates the reason for the load failure.
  // We should let the fallback content load only if this wasn't a cancelled
  // request.
  // Note: The mac version also has a case for "WebKitErrorPluginWillHandleLoad"
  return error.errorCode() != net::ERR_ABORTED;
}

bool WebFrameLoaderClient::canHandleRequest(const ResourceRequest&) const {
  // FIXME: this appears to be used only by the context menu code to determine
  // if "open" should be displayed in the menu when clicking on a link.
  return true;
}

bool WebFrameLoaderClient::canShowMIMEType(const String& mime_type) const {
  // This method is called to determine if the media type can be shown
  // "internally" (i.e. inside the browser) regardless of whether or not the
  // browser or a plugin is doing the rendering.

  // mime_type strings are supposed to be ASCII, but if they are not for some
  // reason, then it just means that the mime type will fail all of these "is
  // supported" checks and go down the path of an unhandled mime type.
  if (net::IsSupportedMimeType(
          webkit_glue::CStringToStdString(mime_type.latin1())))
    return true;

  // If Chrome is started with the --disable-plugins switch, pluginData is null.
  WebCore::PluginData* plugin_data = webframe_->frame()->page()->pluginData();

  // See if the type is handled by an installed plugin, if so, we can show it.
  // TODO(beng): (http://b/1085524) This is the place to stick a preference to
  //             disable full page plugins (optionally for certain types!)
  return plugin_data && plugin_data->supportsMimeType(mime_type);
}

bool WebFrameLoaderClient::representationExistsForURLScheme(const String& URLScheme) const {
  // FIXME
  return false;
}

String WebFrameLoaderClient::generatedMIMETypeForURLScheme(const String& URLScheme) const {
  // This appears to generate MIME types for protocol handlers that are handled
  // internally. The only place I can find in the WebKit code that uses this
  // function is WebView::registerViewClass, where it is used as part of the
  // process by which custom view classes for certain document representations
  // are registered.
  String mimetype("x-apple-web-kit/");
  mimetype.append(URLScheme.lower());
  return mimetype;
}

void WebFrameLoaderClient::frameLoadCompleted() {
  // FIXME: the mac port also conditionally calls setDrawsBackground:YES on
  // it's ScrollView here.

  // This comment from the Mac port:
  // Note: Can be called multiple times.
  // Even if already complete, we might have set a previous item on a frame that
  // didn't do any data loading on the past transaction. Make sure to clear these out.
  webframe_->frame()->loader()->setPreviousHistoryItem(0);
}

void WebFrameLoaderClient::saveViewStateToItem(HistoryItem*) {
  // FIXME
}


void WebFrameLoaderClient::restoreViewState() {
  // FIXME: probably scrolls to last position when you go back or forward
}

void WebFrameLoaderClient::provisionalLoadStarted() {
  // FIXME: On mac, this does various caching stuff
}

void WebFrameLoaderClient::didFinishLoad() {
  WebPluginDelegate* plg_delegate = webframe_->plugin_delegate();
  if (plg_delegate)
    plg_delegate->DidFinishLoadWithReason(NPRES_DONE);
}
void WebFrameLoaderClient::prepareForDataSourceReplacement() {
  // FIXME
}

PassRefPtr<DocumentLoader> WebFrameLoaderClient::createDocumentLoader(
    const ResourceRequest& request,
    const SubstituteData& data) {
  RefPtr<WebDocumentLoaderImpl> loader = WebDocumentLoaderImpl::create(request,
                                                                       data);

  // Attach a datasource to the loader as a way of accessing requests.
  WebDataSourceImpl* datasource =
      WebDataSourceImpl::CreateInstance(webframe_, loader.get());
  loader->SetDataSource(datasource);

  webframe_->CacheCurrentRequestInfo(datasource);

  return loader.release();
}

void WebFrameLoaderClient::setTitle(const String& title, const KURL& url) {
  // FIXME: monitor for changes in WebFrameLoaderClient.mm
  // FIXME: Set the title of the current history item. HistoryItemImpl's setter
  //        will notify its clients (e.g. the history database) that the title
  //        has changed.
  //
  // e.g.:
  // WebHistoryItem* item =
  // webframe_->webview_impl()->GetBackForwardList()->GetCurrentItem();
  // WebHistoryItemImpl* item_impl = static_cast<WebHistoryItemImpl*>(item);
  //
  // item_impl->SetTitle(webkit_glue::StringToStdWString(title));
}

String WebFrameLoaderClient::userAgent(const KURL& url) {
  return webkit_glue::StdStringToString(
      webkit_glue::GetUserAgent(webkit_glue::KURLToGURL(url)));
}

void WebFrameLoaderClient::savePlatformDataToCachedFrame(WebCore::CachedFrame*) {
  NOTREACHED() << "Page cache should be disabled";
}

void WebFrameLoaderClient::transitionToCommittedFromCachedFrame(WebCore::CachedFrame*) {
  ASSERT_NOT_REACHED();
}

// Called when the FrameLoader goes into a state in which a new page load
// will occur.
void WebFrameLoaderClient::transitionToCommittedForNewPage() {
  WebViewImpl* webview = webframe_->webview_impl();
  WebViewDelegate* d = webview->delegate();
  // Notify the RenderView.
  if (d) {
    d->TransitionToCommittedForNewPage();
  }
  makeDocumentView();
}

bool WebFrameLoaderClient::canCachePage() const {
  // Since we manage the cache, always report this page as non-cacheable to
  // FrameLoader.
  return false;
}

// Downloading is handled in the browser process, not WebKit. If we get to this
// point, our download detection code in the ResourceDispatcherHost is broken!
void WebFrameLoaderClient::download(ResourceHandle* handle,
                                    const ResourceRequest& request,
                                    const ResourceRequest& initialRequest,
                                    const ResourceResponse& response) {
  NOTREACHED();
}

PassRefPtr<Frame> WebFrameLoaderClient::createFrame(
    const KURL& url,
    const String& name,
    HTMLFrameOwnerElement* owner_element,
    const String& referrer,
    bool allows_scrolling,
    int margin_width,
    int margin_height) {
  FrameLoadRequest frame_request(ResourceRequest(url, referrer), name);
  return webframe_->CreateChildFrame(frame_request, owner_element);
}

// Utility function to convert a vector to an array of char*'s.
// Caller is responsible to free memory with DeleteToArray().
static char** ToArray(const Vector<WebCore::String> &vector) {
  char **rv = new char *[vector.size()+1];
  unsigned int index = 0;
  for (index = 0; index < vector.size(); ++index) {
    WebCore::CString src = vector[index].utf8();
    rv[index] = new char[src.length() + 1];
    base::strlcpy(rv[index], src.data(), src.length() + 1);
    rv[index][src.length()] = '\0';
  }
  rv[index] = 0;
  return rv;
}

static void DeleteToArray(char** arr) {
  char **ptr = arr;
  while (*ptr != 0) {
    delete [] *ptr;
    ++ptr;
  }
  delete [] arr;
}

Widget* WebFrameLoaderClient::createPlugin(const IntSize& size, // TODO(erikkay): how do we use this?
                                           Element *element, const KURL &url,
                                           const Vector<String> &param_names,
                                           const Vector<String> &param_values,
                                           const String &mime_type,
                                           bool load_manually) {
  WebViewImpl* webview = webframe_->webview_impl();
  WebViewDelegate* d = webview->delegate();
  if (!d)
    return NULL;

  GURL gurl = webkit_glue::KURLToGURL(url);
  std::string my_mime_type =
      webkit_glue::CStringToStdString(mime_type.latin1());
  StringToLowerASCII(&my_mime_type);

  // Get the classid and version from attributes of the object.
  std::string combined_clsid;
#if defined(OS_WIN)
  std::string clsid, version;
  if (activex_shim::IsMimeTypeActiveX(my_mime_type)) {
    GURL url = webframe_->GetURL();
    for (unsigned int i = 0; i < param_names.size(); i++) {
      String lowercase_param_name = param_names[i].lower();
      if (lowercase_param_name == "classid") {
        activex_shim::GetClsidFromClassidAttribute(
            webkit_glue::CStringToStdString(param_values[i].latin1()), &clsid);
      } else if (lowercase_param_name == "codebase") {
        version = activex_shim::GetVersionFromCodebaseAttribute(
            webkit_glue::CStringToStdString(param_values[i].latin1()));
      }
    }
    // We only allowed specific ActiveX controls to run from certain websites.
    if (!activex_shim::IsActiveXAllowed(clsid, url))
      return NULL;
    // We need to pass the combined clsid + version to PluginsList, so that it
    // would detect if the requested version is installed. If not, it needs
    // to use the default plugin to update the control.
    if (!version.empty())
      combined_clsid = clsid + "#" + version;
    else
      combined_clsid = clsid;
  }
#endif

  std::string actual_mime_type;
  WebPluginDelegate* plugin_delegate =
      d->CreatePluginDelegate(webframe_->webview_impl(), gurl, my_mime_type,
                              combined_clsid, &actual_mime_type);
  if (!plugin_delegate)
    return NULL;

  if (!actual_mime_type.empty())
    my_mime_type = actual_mime_type;

  DCHECK(param_names.size() == param_values.size());

  char **argn = NULL;
  char **argv = NULL;
  int argc = 0;
  // There is a bug in Webkit which occurs when a plugin instance is defined
  // with an OBJECT tag containing the "DATA" attribute". Please refer to the
  // webkit issue http://bugs.webkit.org/show_bug.cgi?id=15457 for more info.
  // The code below is a patch which should be taken out when a fix is
  // available in webkit. The logic is to add the "src" attribute to the list
  // of params if the "data" attribute exists.
  // TODO(iyengar) : remove this when a fix is available in webkit.
  int data_attr_index = -1;
  int src_attr_index = -1;
  for (unsigned int i = 0; i < param_names.size(); i++) {
    String param_name = param_names[i].lower();
    if (param_name == "data")
      data_attr_index = i;
    else if (param_name == "src")
      src_attr_index = i;
  }
  if ((data_attr_index != -1) && (src_attr_index == -1)) {
    Vector<String> updated_param_names = param_names;
    Vector<String> updated_param_values = param_values;
    updated_param_names.append("src");
    updated_param_values.append(param_values[data_attr_index]);

    argn = ToArray(updated_param_names);
    argv = ToArray(updated_param_values);
    argc = static_cast<int>(updated_param_names.size());
  } else {
    argn = ToArray(param_names);
    argv = ToArray(param_values);
    argc = static_cast<int>(param_names.size());
  }

  Widget* result = WebPluginImpl::Create(gurl, argn, argv, argc, element,
                                         webframe_, plugin_delegate,
                                         load_manually, my_mime_type);

  DeleteToArray(argn);
  DeleteToArray(argv);

  return result;
}

// This method gets called when a plugin is put in place of html content
// (e.g., acrobat reader).
void WebFrameLoaderClient::redirectDataToPlugin(Widget* pluginWidget) {
  plugin_widget_ = static_cast<WebPluginContainer*>(pluginWidget);
  DCHECK(plugin_widget_ != NULL);
}

Widget* WebFrameLoaderClient::createJavaAppletWidget(
                                           const IntSize& size,
                                           Element *element, const KURL &url,
                                           const Vector<String> &param_names,
                                           const Vector<String> &param_values) {
  return createPlugin(size, element, url, param_names, param_values,
    "application/x-java-applet", false);
}

ObjectContentType WebFrameLoaderClient::objectContentType(
    const KURL& url,
    const String& explicit_mime_type) {
  // This code is based on Apple's implementation from
  // WebCoreSupport/WebFrameBridge.mm.

  String mime_type = explicit_mime_type;
  if (mime_type.isEmpty()) {
    // Try to guess the MIME type based off the extension.
    String filename = url.lastPathComponent();
    int extension_pos = filename.reverseFind('.');
    if (extension_pos >= 0)
      mime_type = MIMETypeRegistry::getMIMETypeForPath(url.path());

    if (mime_type.isEmpty())
      return ObjectContentFrame;
  }

  if (MIMETypeRegistry::isSupportedImageMIMEType(mime_type))
    return ObjectContentImage;

  // If Chrome is started with the --disable-plugins switch, pluginData is null.
  PluginData* plugin_data = webframe_->frame()->page()->pluginData();
  if (plugin_data && plugin_data->supportsMimeType(mime_type))
    return ObjectContentNetscapePlugin;

  if (MIMETypeRegistry::isSupportedNonImageMIMEType(mime_type))
    return ObjectContentFrame;

  return ObjectContentNone;
}

String WebFrameLoaderClient::overrideMediaType() const {
  // FIXME
  String rv;
  return rv;
}

bool WebFrameLoaderClient::ActionSpecifiesDisposition(
    const WebCore::NavigationAction& action,
    WindowOpenDisposition* disposition) {
  if ((action.type() != NavigationTypeLinkClicked) ||
      !action.event()->isMouseEvent())
    return false;

  const MouseEvent* event = static_cast<const MouseEvent*>(action.event());
  const bool middle_or_ctrl = (event->button() == 1) || event->ctrlKey();
  const bool shift = event->shiftKey();
  const bool alt = event->altKey();
  if (!middle_or_ctrl && !shift && !alt)
    return false;

  DCHECK(disposition);
  if (middle_or_ctrl)
    *disposition = shift ? NEW_FOREGROUND_TAB : NEW_BACKGROUND_TAB;
  else
    *disposition = shift ? NEW_WINDOW : SAVE_TO_DISK;
  return true;
}
