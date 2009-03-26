// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBFRAMELOADERCLIENT_IMPL_H__
#define WEBKIT_GLUE_WEBFRAMELOADERCLIENT_IMPL_H__

#include <set>

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "FrameLoaderClient.h"
MSVC_POP_WARNING();

#include "build/build_config.h"
#include "base/scoped_ptr.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/webview_delegate.h"
#include "webkit/glue/window_open_disposition.h"

namespace WebCore {
class Frame;
class HTMLFormElement;
class Widget;
}

class Alt404PageResourceFetcher;
class NetAgentImpl;
class WebFrameImpl;
class WebPluginContainer;

class WebFrameLoaderClient : public WebCore::FrameLoaderClient {
 public:
  WebFrameLoaderClient(WebFrameImpl* webframe);
  virtual ~WebFrameLoaderClient();

  WebFrameImpl* webframe() const { return webframe_; }

  // WebCore::FrameLoaderClient ----------------------------------------------

  virtual void frameLoaderDestroyed();

  // Notifies the WebView delegate that the JS window object has been cleared,
  // giving it a chance to bind native objects to the window before script
  // parsing begins.
  virtual void windowObjectCleared();
  virtual void documentElementAvailable();

  virtual bool hasWebView() const; // mainly for assertions
  virtual bool hasFrameView() const; // ditto

  virtual void makeRepresentation(WebCore::DocumentLoader*);
  virtual void forceLayout();
  virtual void forceLayoutForNonHTML();

  virtual void setCopiesOnScroll();

  virtual void detachedFromParent2();
  virtual void detachedFromParent3();

  virtual void assignIdentifierToInitialRequest(unsigned long identifier, WebCore::DocumentLoader*, const WebCore::ResourceRequest&);

  virtual void dispatchWillSendRequest(WebCore::DocumentLoader*, unsigned long identifier, WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse);
  virtual bool shouldUseCredentialStorage(WebCore::DocumentLoader*, unsigned long identifier);
  virtual void dispatchDidReceiveAuthenticationChallenge(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::AuthenticationChallenge&);
  virtual void dispatchDidCancelAuthenticationChallenge(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::AuthenticationChallenge&);
  virtual void dispatchDidReceiveResponse(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceResponse&);
  virtual void dispatchDidReceiveContentLength(WebCore::DocumentLoader*, unsigned long identifier, int lengthReceived);
  virtual void dispatchDidFinishLoading(WebCore::DocumentLoader*, unsigned long identifier);
  virtual void dispatchDidFailLoading(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceError&);
  virtual bool dispatchDidLoadResourceFromMemoryCache(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, int length);

  virtual void dispatchDidHandleOnloadEvents();
  virtual void dispatchDidReceiveServerRedirectForProvisionalLoad();
  virtual void dispatchDidCancelClientRedirect();
  virtual void dispatchWillPerformClientRedirect(const WebCore::KURL&, double interval, double fireDate);
  virtual void dispatchDidChangeLocationWithinPage();
  virtual void dispatchWillClose();
  virtual void dispatchDidReceiveIcon();
  virtual void dispatchDidStartProvisionalLoad();
  virtual void dispatchDidReceiveTitle(const WebCore::String& title);
  virtual void dispatchDidCommitLoad();
  virtual void dispatchDidFailProvisionalLoad(const WebCore::ResourceError&);
  virtual void dispatchDidFailLoad(const WebCore::ResourceError&);
  virtual void dispatchDidFinishDocumentLoad();
  virtual void dispatchDidFinishLoad();
  virtual void dispatchDidFirstLayout();
  virtual void dispatchDidFirstVisuallyNonEmptyLayout();

  virtual WebCore::Frame* dispatchCreatePage();
  virtual void dispatchShow();

  virtual void dispatchDecidePolicyForMIMEType(WebCore::FramePolicyFunction function, const WebCore::String& mime_type, const WebCore::ResourceRequest&);
  virtual void dispatchDecidePolicyForNewWindowAction(WebCore::FramePolicyFunction function, const WebCore::NavigationAction& action, const WebCore::ResourceRequest& request, PassRefPtr<WebCore::FormState> form_state, const WebCore::String& frame_name);
  virtual void dispatchDecidePolicyForNavigationAction(WebCore::FramePolicyFunction function, const WebCore::NavigationAction& action, const WebCore::ResourceRequest& request, PassRefPtr<WebCore::FormState> form_state);
  virtual void cancelPolicyCheck();

  virtual void dispatchUnableToImplementPolicy(const WebCore::ResourceError&);

  virtual void dispatchWillSubmitForm(WebCore::FramePolicyFunction, PassRefPtr<WebCore::FormState>);

  virtual void dispatchDidLoadMainResource(WebCore::DocumentLoader*);
  virtual void revertToProvisionalState(WebCore::DocumentLoader*);
  virtual void setMainDocumentError(WebCore::DocumentLoader*, const WebCore::ResourceError&);

  // Maybe these should go into a ProgressTrackerClient some day
  virtual void willChangeEstimatedProgress() { }
  virtual void didChangeEstimatedProgress() { }
  virtual void postProgressStartedNotification();
  virtual void postProgressEstimateChangedNotification();
  virtual void postProgressFinishedNotification();

  virtual void setMainFrameDocumentReady(bool);

  virtual void startDownload(const WebCore::ResourceRequest&);

  virtual void willChangeTitle(WebCore::DocumentLoader*);
  virtual void didChangeTitle(WebCore::DocumentLoader*);

  virtual void committedLoad(WebCore::DocumentLoader*, const char*, int);
  virtual void finishedLoading(WebCore::DocumentLoader*);

  virtual void updateGlobalHistory();
  virtual void updateGlobalHistoryRedirectLinks();

  virtual bool shouldGoToHistoryItem(WebCore::HistoryItem*) const;

  virtual WebCore::ResourceError blockedError(const WebCore::ResourceRequest&);
  virtual WebCore::ResourceError cancelledError(const WebCore::ResourceRequest&);
  virtual WebCore::ResourceError cannotShowURLError(const WebCore::ResourceRequest&);
  virtual WebCore::ResourceError interruptForPolicyChangeError(const WebCore::ResourceRequest&);

  virtual WebCore::ResourceError cannotShowMIMETypeError(const WebCore::ResourceResponse&);
  virtual WebCore::ResourceError fileDoesNotExistError(const WebCore::ResourceResponse&);
  virtual WebCore::ResourceError pluginWillHandleLoadError(const WebCore::ResourceResponse&);

  virtual bool shouldFallBack(const WebCore::ResourceError&);

  virtual bool canHandleRequest(const WebCore::ResourceRequest&) const;
  virtual bool canShowMIMEType(const WebCore::String& MIMEType) const;
  virtual bool representationExistsForURLScheme(const WebCore::String& URLScheme) const;
  virtual WebCore::String generatedMIMETypeForURLScheme(const WebCore::String& URLScheme) const;

  virtual void frameLoadCompleted();
  virtual void saveViewStateToItem(WebCore::HistoryItem*);
  virtual void restoreViewState();
  virtual void provisionalLoadStarted();
  virtual void didFinishLoad();
  virtual void prepareForDataSourceReplacement();

  virtual PassRefPtr<WebCore::DocumentLoader> createDocumentLoader(
                                              const WebCore::ResourceRequest&,
                                              const WebCore::SubstituteData&);
  virtual void setTitle(const WebCore::String& title, const WebCore::KURL&);

  virtual WebCore::String userAgent(const WebCore::KURL&);

  virtual void savePlatformDataToCachedFrame(WebCore::CachedFrame*);
  virtual void transitionToCommittedFromCachedFrame(WebCore::CachedFrame*);
  virtual void transitionToCommittedForNewPage();

  virtual bool canCachePage() const;
  virtual void download(WebCore::ResourceHandle* handle,
                        const WebCore::ResourceRequest& request,
                        const WebCore::ResourceRequest& initialRequest,
                        const WebCore::ResourceResponse& response);
  virtual PassRefPtr<WebCore::Frame> createFrame(
                                      const WebCore::KURL& url,
                                      const WebCore::String& name,
                                      WebCore::HTMLFrameOwnerElement* ownerElement,
                                      const WebCore::String& referrer,
                                      bool allowsScrolling, int marginWidth,
                                      int marginHeight);
  virtual WebCore::Widget* createPlugin(const WebCore::IntSize&,
                                        WebCore::HTMLPlugInElement*,
                                        const WebCore::KURL&,
                                        const WTF::Vector<WebCore::String>&,
                                        const WTF::Vector<WebCore::String>&,
                                        const WebCore::String&,
                                        bool loadManually);
  virtual void redirectDataToPlugin(WebCore::Widget* pluginWidget);

  virtual WebCore::Widget* createJavaAppletWidget(
      const WebCore::IntSize&,
      WebCore::HTMLAppletElement*,
      const WebCore::KURL& baseURL,
      const WTF::Vector<WebCore::String>& paramNames,
      const WTF::Vector<WebCore::String>& paramValues);

  virtual WebCore::ObjectContentType objectContentType(const WebCore::KURL& url,
                                              const WebCore::String& mimeType);
  virtual WebCore::String overrideMediaType() const;

  virtual void didPerformFirstNavigation() const;

  virtual void registerForIconNotification(bool listen = true);

  // Callback function for download of alternate 404 pages.  If the server is
  // down or we take more than 1s to download the page, html will be an empty
  // string.
  void Alt404PageFinished(WebCore::DocumentLoader* loader,
                          const std::string& html);

 private:
  void makeDocumentView();

  // Given a NavigationAction, determine the associated window opening
  // disposition.  For example, a middle click means "open in background tab".
  static bool ActionSpecifiesDisposition(
      const WebCore::NavigationAction& action,
      WindowOpenDisposition* disposition);

  // Returns a valid GURL if we have an alt 404 server URL.
  GURL GetAlt404PageUrl(WebCore::DocumentLoader* loader);

  // Returns NavigationGestureAuto if the last load was not user initiated,
  // otherwise returns NavigationGestureUnknown.
  NavigationGesture NavigationGestureForLastLoad();

  // Returns NetAgent instance if network tracking is enabled.
  NetAgentImpl* GetNetAgentImpl();

  // The WebFrame that owns this object and manages its lifetime. Therefore,
  // the web frame object is guaranteed to exist.
  WebFrameImpl* webframe_;

  // Resource fetcher for downloading an alternate 404 page.
  scoped_ptr<Alt404PageResourceFetcher> alt_404_page_fetcher_;

  bool postpone_loading_data_;
  std::string postponed_data_;

  // True if makeRepresentation was called.  We don't actually have a concept
  // of a "representation", but we need to know when we're expected to have one.
  // See finishedLoading().
  bool has_representation_;

  // Used to help track client redirects. When a provisional load starts, it
  // has no redirects in its chain. But in the case of client redirects, we want
  // to add that initial load as a redirect. When we get a new provisional load
  // and the dest URL matches that load, we know that it was the result of a
  // previous client redirect and the source should be added as a redirect.
  // Both should be empty if unused.
  GURL expected_client_redirect_src_;
  GURL expected_client_redirect_dest_;

  // Contains a pointer to the plugin widget.
  WebPluginContainer* plugin_widget_;
  // Indicates if we need to send over the initial notification to the plugin
  // which specifies that the plugin should be ready to accept data.
  bool sent_initial_response_to_plugin_;

  // The disposition to use for the next call to dispatchCreatePage.
  WindowOpenDisposition next_window_open_disposition_;
};

#endif  // #ifndef WEBKIT_GLUE_WEBFRAMELOADERCLIENT_IMPL_H__
