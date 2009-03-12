// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBDATASOURCE_H_
#define WEBKIT_GLUE_WEBDATASOURCE_H_

#include <vector>

#include "base/string16.h"

class GURL;
class SearchableFormData;
class WebFrame;
class WebRequest;
class WebResponse;
struct PasswordForm;

class WebDataSource {
 public:
  // Returns the frame that represents this data source.
  virtual WebFrame* GetWebFrame() = 0;

  // Returns a reference to the original request data that created the
  // datasource. This request will be unmodified by WebKit.
  //
  // Note that this will be a different physical object than the WebRequest
  // that was specified in the load request initiated by the embedder, but the
  // data members will be copied.
  //
  // This call will update the request with the latest information from WebKit,
  // so it is important that the caller not cache the result or keep the
  // reference across entries into WebKit.
  virtual const WebRequest& GetInitialRequest() const = 0;

  // Returns the request that was used to create this datasource. This may
  // be modified by WebKit.  This is the same as what GetInitialRequest
  // returns unless there was a redirect.
  //
  // Note that this will be a different physical object than the WebRequest
  // that was specified in the load request initiated by the embedder.
  //
  // This call will update the request with the latest information from WebKit,
  // so it is important that the caller not cache the result or keep the
  // reference across entries into WebKit.
  virtual const WebRequest& GetRequest() const = 0;

  // Returns the response associated to this datasource.
  virtual const WebResponse& GetResponse() const = 0;

  // Returns the unreachable URL for which this datasource is showing alternate
  // content.  See WebFrame::LoadAlternateHTML{ErrorPage,String}.
  virtual GURL GetUnreachableURL() const = 0;

  // Returns true if there is a non-null unreachable URL.
  virtual bool HasUnreachableURL() const = 0;

  // Returns all redirects that occurred (both client and server) before at
  // last committing the current page. This will contain one entry for each
  // intermediate URL, and one entry for the last URL (so if there are no
  // redirects, it will contain exactly the current URL, and if there is one
  // redirect, it will contain the source and destination URL).
  virtual const std::vector<GURL>& GetRedirectChain() const = 0;

  // Returns the SearchableFormData, or NULL if the request wasn't a search
  // request. The returned object is owned by the WebDataSource (actually
  // the WebDocumentLoader) and shouldn't be freed.
  virtual const SearchableFormData* GetSearchableFormData() const = 0;

  // Returns the PasswordFormData, or NULL if the request isn't a form submission
  // or doesn't have any password fields. The returned object is owned by the
  // WebDataSource (actually the WebDocumentLoader) and shouldn't be freed.
  virtual const PasswordForm* GetPasswordFormData() const = 0;

  // Returns true if this request was the result of submitting a form.
  // NOTE: this returns false if the user submitted a form, but the form used
  // script to do the actuall submission.
  virtual bool IsFormSubmit() const = 0;

  // Returns the page title.
  virtual string16 GetPageTitle() const = 0;

  virtual ~WebDataSource() {}
};

#endif  // #ifndef WEBKIT_GLUE_WEBDATASOURCE_H_
