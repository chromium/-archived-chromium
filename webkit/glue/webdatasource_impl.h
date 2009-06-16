// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBDATASOURCE_IMPL_H_
#define WEBKIT_GLUE_WEBDATASOURCE_IMPL_H_

#include "DocumentLoader.h"

#include "webkit/glue/webdatasource.h"
#include "webkit/glue/webresponse_impl.h"
#include "webkit/glue/weburlrequest_impl.h"

class WebFrameImpl;
class WebDocumentLoaderImpl;

class WebDataSourceImpl : public WebCore::DocumentLoader, public WebDataSource {
 public:
  static PassRefPtr<WebDataSourceImpl> Create(const WebCore::ResourceRequest&,
                                              const WebCore::SubstituteData&);

  static WebDataSourceImpl* FromLoader(WebCore::DocumentLoader* loader) {
    return static_cast<WebDataSourceImpl*>(loader);
  }

  // WebDataSource methods:
  virtual const WebRequest& GetInitialRequest() const;
  virtual const WebRequest& GetRequest() const;
  virtual const WebResponse& GetResponse() const;
  virtual GURL GetUnreachableURL() const;
  virtual bool HasUnreachableURL() const;
  virtual const std::vector<GURL>& GetRedirectChain() const;
  virtual string16 GetPageTitle() const;
  virtual double GetTriggeringEventTime() const;
  virtual WebNavigationType GetNavigationType() const;
  virtual ExtraData* GetExtraData() const;
  virtual void SetExtraData(ExtraData*);

  static WebNavigationType NavigationTypeToWebNavigationType(
      WebCore::NavigationType type);

  void ClearRedirectChain();
  void AppendRedirect(const GURL& url);

 private:
  WebDataSourceImpl(const WebCore::ResourceRequest&,
                    const WebCore::SubstituteData&);
  ~WebDataSourceImpl();

  // Mutable because the const getters will magically sync these to the
  // latest version from WebKit.
  mutable WebRequestImpl initial_request_;
  mutable WebRequestImpl request_;
  mutable WebResponseImpl response_;

  // Lists all intermediate URLs that have redirected for the current
  // provisional load. See WebFrameLoaderClient::
  // dispatchDidReceiveServerRedirectForProvisionalLoad for a description of
  // who modifies this when to keep it up to date.
  std::vector<GURL> redirect_chain_;

  OwnPtr<ExtraData> extra_data_;

  DISALLOW_COPY_AND_ASSIGN(WebDataSourceImpl);
};

#endif  // WEBKIT_GLUE_WEBDATASOURCE_IMPL_H_
