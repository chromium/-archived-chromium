// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBDATASOURCE_H__
#define WEBKIT_GLUE_WEBDATASOURCE_H__

#include <vector>

#include "base/basictypes.h"

class GURL;
struct PasswordForm;
class SearchableFormData;
class WebFrame;
class WebRequest;
class WebResponse;

class WebDataSource {
 public:
  //
  //  @method webFrame
  //  @result Return the frame that represents this data source.
  //  - (WebFrame *)webFrame;
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
  // be modified by WebKit.
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

  virtual std::wstring GetResponseMimeType() const = 0;

  //
  //  @method unreachableURL
  //  @discussion This will be non-nil only for dataSources created by calls to the
  //  WebFrame method loadAlternateHTMLString:baseURL:forUnreachableURL:.
  //  @result returns the unreachableURL for which this dataSource is showing alternate content, or nil
  //  - (NSURL *)unreachableURL;
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
  virtual std::wstring GetPageTitle() const = 0;

  /*
  These functions are not implemented yet, and we are not yet sure whether or not
  we need them, so we have commented them out both here and in the
  webdatasource_impl.cc file.

  //
  //  @method data
  //  @discussion The data will be incomplete until the datasource has completely loaded.
  //  @result Returns the raw data associated with the datasource.  Returns nil
  //  if the datasource hasn't loaded any data.
  //  - (NSData *)data;
  virtual void GetData(IStream** data) = 0;

  //
  //  @method representation
  //  @discussion A representation holds a type specific representation
  //  of the datasource's data.  The representation class is determined by mapping
  //  a MIME type to a class.  The representation is created once the MIME type
  //  of the datasource content has been determined.
  //  @result Returns the representation associated with this datasource.
  //  Returns nil if the datasource hasn't created it's representation.
  //  - (id <WebDocumentRepresentation>)representation;
  virtual void GetRepresentation(IWebDocumentRepresentation** rep) = 0;

  //
  //  @method response
  //  @result returns the WebResourceResponse for the data source.
  //  - (NSURLResponse *)response;
  virtual void GetResponse(IWebURLResponse** response) = 0;

  //
  //  @method textEncodingName
  //  @result Returns either the override encoding, as set on the WebView for this
  //  dataSource or the encoding from the response.
  //  - (NSString *)textEncodingName;
  virtual void GetTextEncodingName(std::wstring* name) = 0;

  //
  //  @method isLoading
  //  @discussion Returns YES if there are any pending loads.
  //  - (BOOL)isLoading;
  virtual bool IsLoading() = 0;

  //
  //  @method webArchive
  //  @result A WebArchive representing the data source, its subresources and child frames.
  //  @description This method constructs a WebArchive using the original downloaded data.
  //  In the case of HTML, if the current state of the document is preferred, webArchive should be
  //  called on the DOM document instead.
  //  - (WebArchive *)webArchive;
  virtual void GetWebArchive(IWebArchive** archive) = 0;

  //
  //  @method mainResource
  //  @result A WebResource representing the data source.
  //  @description This method constructs a WebResource using the original downloaded data.
  //  This method can be used to construct a WebArchive in case the archive returned by
  //  WebDataSource's webArchive isn't sufficient.
  //  - (WebResource *)mainResource;
  virtual void GetMainResource(IWebResource** resource) = 0;

  //
  //  @method subresources
  //  @abstract Returns all the subresources associated with the data source.
  //  @description The returned array only contains subresources that have fully downloaded.
  //  - (NSArray *)subresources;
  virtual void GetSubresources(int* count, IWebResource*** resources);

  //
  //  method subresourceForURL:
  //  @abstract Returns a subresource for a given URL.
  //  @param URL The URL of the subresource.
  //  @description Returns non-nil if the data source has fully downloaded a subresource with the given URL.
  //  - (WebResource *)subresourceForURL:(NSURL *)URL;
  virtual void GetSubresourceForURL(const std::wstring& url,
                                    IWebResource** resource) = 0;

  //
  //  @method addSubresource:
  //  @abstract Adds a subresource to the data source.
  //  @param subresource The subresource to be added.
  //  @description addSubresource: adds a subresource to the data source's list of subresources.
  //  Later, if something causes the data source to load the URL of the subresource, the data source
  //  will load the data from the subresource instead of from the network. For example, if one wants to add
  //  an image that is already downloaded to a web page, addSubresource: can be called so that the data source
  //  uses the downloaded image rather than accessing the network. NOTE: If the data source already has a
  //  subresource with the same URL, addSubresource: will replace it.
  //  - (void)addSubresource:(WebResource *)subresource;
  virtual void AddSubresource(IWebResource* subresource) = 0;
  */

  WebDataSource() { }
  virtual ~WebDataSource() { }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(WebDataSource);
};



#endif  // #ifndef WEBKIT_GLUE_WEBDATASOURCE_H__

