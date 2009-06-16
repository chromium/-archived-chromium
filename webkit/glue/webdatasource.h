// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBDATASOURCE_H_
#define WEBKIT_GLUE_WEBDATASOURCE_H_

#include <vector>

#include "base/string16.h"

class GURL;
class WebFrame;
class WebRequest;
class WebResponse;

namespace base {
class Time;
}

enum WebNavigationType {
  WebNavigationTypeLinkClicked,
  WebNavigationTypeFormSubmitted,
  WebNavigationTypeBackForward,
  WebNavigationTypeReload,
  WebNavigationTypeFormResubmitted,
  WebNavigationTypeOther
};

class WebDataSource {
 public:
  // A base class for extra data that may be associated with this datasource.
  // See Set/GetExtraData below.
  class ExtraData {
   public:
    virtual ~ExtraData() {}
  };

  virtual ~WebDataSource() {}

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

  // Returns the page title.
  virtual string16 GetPageTitle() const = 0;

  // The time in seconds (since the epoch) of the event if any that triggered
  // this navigation.  Returns 0 if unknown.
  virtual double GetTriggeringEventTime() const = 0;

  // Returns the reason the document was loaded.
  virtual WebNavigationType GetNavigationType() const = 0;

  // Extra data associated with this datasource.  If non-null, the extra data
  // pointer will be deleted when the datasource is destroyed.  Setting the
  // extra data pointer will cause any existing non-null extra data pointer to
  // be deleted.
  virtual ExtraData* GetExtraData() const = 0;
  virtual void SetExtraData(ExtraData* extra_data) = 0;
};

#endif  // #ifndef WEBKIT_GLUE_WEBDATASOURCE_H_
