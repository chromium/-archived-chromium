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

#ifndef WEBKIT_GLUE_WEBDATASOURCE_IMPL_H_
#define WEBKIT_GLUE_WEBDATASOURCE_IMPL_H_

#include "webkit/glue/webdatasource.h"
#include "webkit/glue/webresponse_impl.h"
#include "webkit/glue/weburlrequest_impl.h"

struct PasswordForm;
class SearchableFormData;
class WebFrameImpl;
class WebDocumentLoaderImpl;

class WebDataSourceImpl : public WebDataSource {
 public:
  static WebDataSourceImpl* CreateInstance(WebFrameImpl* frame,
                                           WebDocumentLoaderImpl* loader);

 protected:
  WebDataSourceImpl(WebFrameImpl* frame, WebDocumentLoaderImpl* loader);
  ~WebDataSourceImpl();

 public:
  // WebDataSource
  virtual WebFrame* GetWebFrame();
  virtual const WebRequest& GetInitialRequest() const;
  virtual const WebRequest& GetRequest() const;
  virtual const WebResponse& GetResponse() const;
  virtual std::wstring GetResponseMimeType() const;
  virtual GURL GetUnreachableURL() const;
  virtual bool HasUnreachableURL() const;
  virtual const std::vector<GURL>& GetRedirectChain() const;

  // WebDataSourceImpl
  
  // Called after creating a new data source if there is request info
  // available. Since we store copies of the WebRequests, the original
  // WebRequest that the embedder created was lost, and the exra data would
  // go with it. This preserves the request info so retrieving the requests
  // later will have the same data.
  void SetExtraData(WebRequest::ExtraData* extra);

  void ClearRedirectChain();
  void AppendRedirect(const GURL& url);

  virtual const SearchableFormData* GetSearchableFormData() const;
  virtual const PasswordForm* GetPasswordFormData() const;

  virtual bool IsFormSubmit() const;

  virtual std::wstring GetPageTitle() const;

private:
  WebFrameImpl* frame_;
  WebDocumentLoaderImpl* loader_;

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

  DISALLOW_COPY_AND_ASSIGN(WebDataSourceImpl);
};

#endif  // #ifndef WEBKIT_GLUE_WEBDATASOURCE_IMPL_H_
