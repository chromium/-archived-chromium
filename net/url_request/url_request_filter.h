// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

#include <hash_map>
#include <map>
#include <string>

#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

class GURL;

class URLRequestFilter {
 public:
  // scheme,hostname -> ProtocolFactory
  typedef std::map<std::pair<std::string, std::string>,
      URLRequest::ProtocolFactory*> HostnameHandlerMap;
  typedef stdext::hash_map<std::string, URLRequest::ProtocolFactory*>
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
