// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class to help filter URLRequest jobs based on the URL of the request
// rather than just the scheme.  Example usage:
//
// // Use as an "http" handler.
// URLRequest::RegisterProtocolFactory("http", &URLRequestFilter::Factory);
// // Add special handling for the URL http://foo.com/
// URLRequestFilter::GetInstance()->AddUrlHandler(
//     GURL("http://foo.com/"),
//     &URLRequestCustomJob::Factory);
//
// If URLRequestFilter::Factory can't find a handle for the request, it passes
// it through to URLRequestInetJob::Factory and lets the default network stack
// handle it.

#ifndef NET_URL_REQUEST_URL_REQUEST_FILTER_H_
#define NET_URL_REQUEST_URL_REQUEST_FILTER_H_

#include <map>
#include <string>

#include "base/hash_tables.h"
#include "net/url_request/url_request.h"

class GURL;
class URLRequestJob;

class URLRequestFilter {
 public:
  // scheme,hostname -> ProtocolFactory
  typedef std::map<std::pair<std::string, std::string>,
      URLRequest::ProtocolFactory*> HostnameHandlerMap;
  typedef base::hash_map<std::string, URLRequest::ProtocolFactory*>
      UrlHandlerMap;

  // Singleton instance for use.
  static URLRequestFilter* GetInstance();

  static URLRequest::ProtocolFactory Factory;

  void AddHostnameHandler(const std::string& scheme,
                          const std::string& hostname,
                          URLRequest::ProtocolFactory* factory);
  void RemoveHostnameHandler(const std::string& scheme,
                             const std::string& hostname);

  // Returns true if we successfully added the URL handler.  This will replace
  // old handlers for the URL if one existed.
  bool AddUrlHandler(const GURL& url, URLRequest::ProtocolFactory* factory);

  void RemoveUrlHandler(const GURL& url);

  // Clear all the existing URL handlers and unregister with the
  // ProtocolFactory.
  void ClearHandlers();

 protected:
  URLRequestFilter() { }

  // Helper method that looks up the request in the url_handler_map_.
  URLRequestJob* FindRequestHandler(URLRequest* request,
                                    const std::string& scheme);

  // Maps hostnames to factories.  Hostnames take priority over URLs.
  HostnameHandlerMap hostname_handler_map_;

  // Maps URLs to factories.
  UrlHandlerMap url_handler_map_;

 private:
  // Singleton instance.
  static URLRequestFilter* shared_instance_;

  DISALLOW_EVIL_CONSTRUCTORS(URLRequestFilter);
};

#endif  // NET_URL_REQUEST_URL_REQUEST_FILTER_H_
