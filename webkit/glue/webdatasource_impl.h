// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBDATASOURCE_IMPL_H_
#define WEBKIT_GLUE_WEBDATASOURCE_IMPL_H_

#include "DocumentLoader.h"
#include <wtf/Vector.h>
#include <wtf/OwnPtr.h>

#include "webkit/api/public/WebDataSource.h"
#include "webkit/api/src/WrappedResourceRequest.h"
#include "webkit/api/src/WrappedResourceResponse.h"

class GURL;
class WebFrameImpl;
class WebDocumentLoaderImpl;

class WebDataSourceImpl : public WebCore::DocumentLoader,
                          public WebKit::WebDataSource {
 public:
  static PassRefPtr<WebDataSourceImpl> Create(const WebCore::ResourceRequest&,
                                              const WebCore::SubstituteData&);

  static WebDataSourceImpl* FromLoader(WebCore::DocumentLoader* loader) {
    return static_cast<WebDataSourceImpl*>(loader);
  }

  // WebDataSource methods:
  virtual const WebKit::WebURLRequest& originalRequest() const;
  virtual const WebKit::WebURLRequest& request() const;
  virtual const WebKit::WebURLResponse& response() const;
  virtual bool hasUnreachableURL() const;
  virtual WebKit::WebURL unreachableURL() const;
  virtual void redirectChain(WebKit::WebVector<WebKit::WebURL>&) const;
  virtual WebKit::WebString pageTitle() const;
  virtual WebKit::WebNavigationType navigationType() const;
  virtual double triggeringEventTime() const;
  virtual ExtraData* extraData() const;
  virtual void setExtraData(ExtraData*);

  static WebKit::WebNavigationType NavigationTypeToWebNavigationType(
      WebCore::NavigationType type);

  bool HasRedirectChain() const { return !redirect_chain_.isEmpty(); }
  GURL GetEndOfRedirectChain() const;
  void ClearRedirectChain();
  void AppendRedirect(const GURL& url);

 private:
  WebDataSourceImpl(const WebCore::ResourceRequest&,
                    const WebCore::SubstituteData&);
  ~WebDataSourceImpl();

  // Mutable because the const getters will magically sync these to the
  // latest version from WebKit.
  mutable WebKit::WrappedResourceRequest original_request_;
  mutable WebKit::WrappedResourceRequest request_;
  mutable WebKit::WrappedResourceResponse response_;

  // Lists all intermediate URLs that have redirected for the current
  // provisional load. See WebFrameLoaderClient::
  // dispatchDidReceiveServerRedirectForProvisionalLoad for a description of
  // who modifies this when to keep it up to date.
  Vector<WebKit::WebURL> redirect_chain_;

  OwnPtr<ExtraData> extra_data_;

  DISALLOW_COPY_AND_ASSIGN(WebDataSourceImpl);
};

#endif  // WEBKIT_GLUE_WEBDATASOURCE_IMPL_H_
