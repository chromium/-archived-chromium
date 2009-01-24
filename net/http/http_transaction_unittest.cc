// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_transaction_unittest.h"

#include "base/hash_tables.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "net/base/load_flags.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/http/http_transaction.h"
#include "testing/gtest/include/gtest/gtest.h"

//-----------------------------------------------------------------------------
// mock transaction data

const MockTransaction kSimpleGET_Transaction = {
  "http://www.google.com/",
  "GET",
  "",
  net::LOAD_NORMAL,
  "HTTP/1.1 200 OK",
  "Cache-Control: max-age=10000\n",
  "<html><body>Google Blah Blah</body></html>",
  TEST_MODE_NORMAL,
  NULL,
  0
};

const MockTransaction kSimplePOST_Transaction = {
  "http://bugdatabase.com/edit",
  "POST",
  "",
  net::LOAD_NORMAL,
  "HTTP/1.1 200 OK",
  "",
  "<html><body>Google Blah Blah</body></html>",
  TEST_MODE_NORMAL,
  NULL,
  0
};

const MockTransaction kTypicalGET_Transaction = {
  "http://www.example.com/~foo/bar.html",
  "GET",
  "",
  net::LOAD_NORMAL,
  "HTTP/1.1 200 OK",
  "Date: Wed, 28 Nov 2007 09:40:09 GMT\n"
  "Last-Modified: Wed, 28 Nov 2007 00:40:09 GMT\n",
  "<html><body>Google Blah Blah</body></html>",
  TEST_MODE_NORMAL,
  NULL,
  0
};

const MockTransaction kETagGET_Transaction = {
  "http://www.google.com/foopy",
  "GET",
  "",
  net::LOAD_NORMAL,
  "HTTP/1.1 200 OK",
  "Cache-Control: max-age=10000\n"
  "Etag: foopy\n",
  "<html><body>Google Blah Blah</body></html>",
  TEST_MODE_NORMAL,
  NULL,
  0
};

const MockTransaction kRangeGET_Transaction = {
  "http://www.google.com/",
  "GET",
  "Range: 0-100\r\n",
  net::LOAD_NORMAL,
  "HTTP/1.1 200 OK",
  "Cache-Control: max-age=10000\n",
  "<html><body>Google Blah Blah</body></html>",
  TEST_MODE_NORMAL,
  NULL,
  0
};

static const MockTransaction* const kBuiltinMockTransactions[] = {
  &kSimpleGET_Transaction,
  &kSimplePOST_Transaction,
  &kTypicalGET_Transaction,
  &kETagGET_Transaction,
  &kRangeGET_Transaction
};

typedef base::hash_map<std::string, const MockTransaction*>
    MockTransactionMap;
static MockTransactionMap mock_transactions;

void AddMockTransaction(const MockTransaction* trans) {
  mock_transactions[GURL(trans->url).spec()] = trans;
}

void RemoveMockTransaction(const MockTransaction* trans) {
  mock_transactions.erase(GURL(trans->url).spec());
}

const MockTransaction* FindMockTransaction(const GURL& url) {
  // look for overrides:
  MockTransactionMap::const_iterator it = mock_transactions.find(url.spec());
  if (it != mock_transactions.end())
    return it->second;

  // look for builtins:
  for (size_t i = 0; i < arraysize(kBuiltinMockTransactions); ++i) {
    if (url == GURL(kBuiltinMockTransactions[i]->url))
      return kBuiltinMockTransactions[i];
  }
  return NULL;
}


//-----------------------------------------------------------------------------

// static
int TestTransactionConsumer::quit_counter_ = 0;


//-----------------------------------------------------------------------------
// helpers

int ReadTransaction(net::HttpTransaction* trans, std::string* result) {
  int rv;

  TestCompletionCallback callback;

  std::string content;
  do {
    scoped_refptr<net::IOBuffer> buf = new net::IOBuffer(256);
    rv = trans->Read(buf, 256, &callback);
    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();
    if (rv > 0) {
      content.append(buf->data(), rv);
    } else if (rv < 0) {
      return rv;
    }
  } while (rv > 0);

  result->swap(content);
  return net::OK;
}
