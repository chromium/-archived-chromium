// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_TEST_URL_FETCHER_FACTORY_H_
#define CHROME_BROWSER_NET_TEST_URL_FETCHER_FACTORY_H_

#include <map>

#include "chrome/browser/net/url_fetcher.h"
#include "googleurl/src/gurl.h"

// TestURLFetcher and TestURLFetcherFactory are used for testing consumers of
// URLFetcher. TestURLFetcherFactory is a URLFetcher::Factory that creates
// TestURLFetchers. TestURLFetcher::Start is overriden to do nothing. It is
// expected that you'll grab the delegate from the TestURLFetcher and invoke
// the callback method when appropriate. In this way it's easy to mock a
// URLFetcher.
// Typical usage:
//   // TestURLFetcher requires a MessageLoop:
//   MessageLoopForUI message_loop;
//   // Create and register factory.
//   TestURLFetcherFactory factory;
//   URLFetcher::set_factory(&factory);
//   // Do something that triggers creation of a URLFetcher.
//   TestURLFetcher* fetcher = factory.GetFetcherByID(expected_id);
//   ASSERT(fetcher);
//   // Notify delegate with whatever data you want.
//   fetcher->delegate()->OnURLFetchComplete(...);
//   // Make sure consumer of URLFetcher does the right thing.
//   ...
//   // Reset factory.
//   URLFetcher::set_factory(NULL);


class TestURLFetcher : public URLFetcher {
 public:
  TestURLFetcher(const GURL& url, RequestType request_type, Delegate* d);

  // Returns the delegate installed on the URLFetcher.
  Delegate* delegate() const { return URLFetcher::delegate(); }

  // Overriden to do nothing. It is assumed the caller will notify the delegate.
  virtual void Start() {}

  // URL we were created with. Because of how we're using URLFetcher url()
  // always returns an empty URL. Chances are you'll want to use original_url()
  // in your tests.
  const GURL& original_url() const { return original_url_; }

 private:
  const GURL original_url_;

  DISALLOW_COPY_AND_ASSIGN(TestURLFetcher);
};

// Simple URLFetcher::Factory method that creates TestURLFetchers. All fetchers
// are registered in a map by the id passed to the create method.
class TestURLFetcherFactory : public URLFetcher::Factory {
 public:
  TestURLFetcherFactory() {}

  virtual URLFetcher* CreateURLFetcher(int id,
                                       const GURL& url,
                                       URLFetcher::RequestType request_type,
                                       URLFetcher::Delegate* d);

  TestURLFetcher* GetFetcherByID(int id) const;

 private:
  // Maps from id passed to create to the returned URLFetcher.
  typedef std::map<int, TestURLFetcher*> Fetchers;
  Fetchers fetchers_;

  DISALLOW_COPY_AND_ASSIGN(TestURLFetcherFactory);
};

#endif  // CHROME_TEST_TEST_URL_FETCHER_FACTORY_H_
