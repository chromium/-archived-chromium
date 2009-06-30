// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_cache.h"

#include "base/hash_tables.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "net/base/load_flags.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_transaction.h"
#include "net/http/http_transaction_unittest.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;

namespace {

//-----------------------------------------------------------------------------
// mock disk cache (a very basic memory cache implementation)

class MockDiskEntry : public disk_cache::Entry,
                      public base::RefCounted<MockDiskEntry> {
 public:
  MockDiskEntry()
      : test_mode_(0), doomed_(false), sparse_(false) {
  }

  MockDiskEntry(const std::string& key)
      : key_(key), doomed_(false), sparse_(false) {
    //
    // 'key' is prefixed with an identifier if it corresponds to a cached POST.
    // Skip past that to locate the actual URL.
    //
    // TODO(darin): It breaks the abstraction a bit that we assume 'key' is an
    // URL corresponding to a registered MockTransaction.  It would be good to
    // have another way to access the test_mode.
    //
    GURL url;
    if (isdigit(key[0])) {
      size_t slash = key.find('/');
      DCHECK(slash != std::string::npos);
      url = GURL(key.substr(slash + 1));
    } else {
      url = GURL(key);
    }
    const MockTransaction* t = FindMockTransaction(url);
    DCHECK(t);
    test_mode_ = t->test_mode;
  }

  ~MockDiskEntry() {
  }

  bool is_doomed() const { return doomed_; }

  virtual void Doom() {
    doomed_ = true;
  }

  virtual void Close() {
    Release();
  }

  virtual std::string GetKey() const {
    return key_;
  }

  virtual Time GetLastUsed() const {
    return Time::FromInternalValue(0);
  }

  virtual Time GetLastModified() const {
    return Time::FromInternalValue(0);
  }

  virtual int32 GetDataSize(int index) const {
    DCHECK(index >= 0 && index < 2);
    return static_cast<int32>(data_[index].size());
  }

  virtual int ReadData(int index, int offset, net::IOBuffer* buf, int buf_len,
                       net::CompletionCallback* callback) {
    DCHECK(index >= 0 && index < 2);

    if (offset < 0 || offset > static_cast<int>(data_[index].size()))
      return net::ERR_FAILED;
    if (static_cast<size_t>(offset) == data_[index].size())
      return 0;

    int num = std::min(buf_len, static_cast<int>(data_[index].size()) - offset);
    memcpy(buf->data(), &data_[index][offset], num);

    if (!callback || (test_mode_ & TEST_MODE_SYNC_CACHE_READ))
      return num;

    CallbackLater(callback, num);
    return net::ERR_IO_PENDING;
  }

  virtual int WriteData(int index, int offset, net::IOBuffer* buf, int buf_len,
                        net::CompletionCallback* callback, bool truncate) {
    DCHECK(index >= 0 && index < 2);
    DCHECK(truncate);

    if (offset < 0 || offset > static_cast<int>(data_[index].size()))
      return net::ERR_FAILED;

    data_[index].resize(offset + buf_len);
    if (buf_len)
      memcpy(&data_[index][offset], buf->data(), buf_len);
    return buf_len;
  }

  virtual int ReadSparseData(int64 offset, net::IOBuffer* buf, int buf_len,
                             net::CompletionCallback* completion_callback) {
    if (!sparse_)
      return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;
    if (offset < 0)
      return net::ERR_FAILED;

    DCHECK(offset < kint32max);
    int real_offset = static_cast<int>(offset);
    if (!buf_len)
      return 0;

    int num = std::min(static_cast<int>(data_[1].size()) - real_offset,
                       buf_len);
    memcpy(buf->data(), &data_[1][real_offset], num);

    if (!completion_callback || (test_mode_ & TEST_MODE_SYNC_CACHE_READ))
      return num;

    CallbackLater(completion_callback, num);
    return net::ERR_IO_PENDING;
  }

  virtual int WriteSparseData(int64 offset, net::IOBuffer* buf, int buf_len,
                              net::CompletionCallback* completion_callback) {
    if (!sparse_) {
      if (data_[1].size())
        return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;
      sparse_ = true;
    }
    if (offset < 0)
      return net::ERR_FAILED;
    if (!buf_len)
      return 0;

    DCHECK(offset < kint32max);
    int real_offset = static_cast<int>(offset);

    if (static_cast<int>(data_[1].size()) < real_offset + buf_len)
      data_[1].resize(real_offset + buf_len);

    memcpy(&data_[1][real_offset], buf->data(), buf_len);
    return buf_len;
  }

  virtual int GetAvailableRange(int64 offset, int len, int64* start) {
    if (!sparse_)
      return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;
    if (offset < 0)
      return net::ERR_FAILED;

    *start = offset;
    DCHECK(offset < kint32max);
    int real_offset = static_cast<int>(offset);
    if (static_cast<int>(data_[1].size()) < real_offset)
      return 0;

    int num = std::min(static_cast<int>(data_[1].size()) - real_offset, len);
    int count = 0;
    for (; num > 0; num--, real_offset++) {
      if (!count) {
        if (data_[1][real_offset]) {
          count++;
          *start = real_offset;
        }
      } else {
        if (!data_[1][real_offset])
          break;
        count++;
      }
    }
    return count;
  }

 private:
  // Unlike the callbacks for MockHttpTransaction, we want this one to run even
  // if the consumer called Close on the MockDiskEntry.  We achieve that by
  // leveraging the fact that this class is reference counted.
  void CallbackLater(net::CompletionCallback* callback, int result) {
    MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(this,
        &MockDiskEntry::RunCallback, callback, result));
  }
  void RunCallback(net::CompletionCallback* callback, int result) {
    callback->Run(result);
  }

  std::string key_;
  std::vector<char> data_[2];
  int test_mode_;
  bool doomed_;
  bool sparse_;
};

class MockDiskCache : public disk_cache::Backend {
 public:
  MockDiskCache() : open_count_(0), create_count_(0), fail_requests_(0) {
  }

  ~MockDiskCache() {
    EntryMap::iterator it = entries_.begin();
    for (; it != entries_.end(); ++it)
      it->second->Release();
  }

  virtual int32 GetEntryCount() const {
    return static_cast<int32>(entries_.size());
  }

  virtual bool OpenEntry(const std::string& key, disk_cache::Entry** entry) {
    if (fail_requests_)
      return false;

    EntryMap::iterator it = entries_.find(key);
    if (it == entries_.end())
      return false;

    if (it->second->is_doomed()) {
      it->second->Release();
      entries_.erase(it);
      return false;
    }

    open_count_++;

    it->second->AddRef();
    *entry = it->second;

    return true;
  }

  virtual bool CreateEntry(const std::string& key, disk_cache::Entry** entry) {
    if (fail_requests_)
      return false;

    EntryMap::iterator it = entries_.find(key);
    DCHECK(it == entries_.end());

    create_count_++;

    MockDiskEntry* new_entry = new MockDiskEntry(key);

    new_entry->AddRef();
    entries_[key] = new_entry;

    new_entry->AddRef();
    *entry = new_entry;

    return true;
  }

  virtual bool DoomEntry(const std::string& key) {
    EntryMap::iterator it = entries_.find(key);
    if (it != entries_.end()) {
      it->second->Release();
      entries_.erase(it);
    }
    return true;
  }

  virtual bool DoomAllEntries() {
    return false;
  }

  virtual bool DoomEntriesBetween(const Time initial_time,
                                  const Time end_time) {
    return true;
  }

  virtual bool DoomEntriesSince(const Time initial_time) {
    return true;
  }

  virtual bool OpenNextEntry(void** iter, disk_cache::Entry** next_entry) {
    return false;
  }

  virtual void EndEnumeration(void** iter) {}

  virtual void GetStats(
      std::vector<std::pair<std::string, std::string> >* stats) {
  }

  // returns number of times a cache entry was successfully opened
  int open_count() const { return open_count_; }

  // returns number of times a cache entry was successfully created
  int create_count() const { return create_count_; }

  // Fail any subsequent CreateEntry and OpenEntry.
  void set_fail_requests() { fail_requests_ = true; }

 private:
  typedef base::hash_map<std::string, MockDiskEntry*> EntryMap;
  EntryMap entries_;
  int open_count_;
  int create_count_;
  bool fail_requests_;
};

class MockHttpCache {
 public:
  MockHttpCache() : http_cache_(new MockNetworkLayer(), new MockDiskCache()) {
  }

  net::HttpCache* http_cache() { return &http_cache_; }

  MockNetworkLayer* network_layer() {
    return static_cast<MockNetworkLayer*>(http_cache_.network_layer());
  }
  MockDiskCache* disk_cache() {
    return static_cast<MockDiskCache*>(http_cache_.disk_cache());
  }

 private:
  net::HttpCache http_cache_;
};


//-----------------------------------------------------------------------------
// helpers

void ReadAndVerifyTransaction(net::HttpTransaction* trans,
                              const MockTransaction& trans_info) {
  std::string content;
  int rv = ReadTransaction(trans, &content);

  EXPECT_EQ(net::OK, rv);
  EXPECT_EQ(strlen(trans_info.data), content.size());
  EXPECT_EQ(0, memcmp(trans_info.data, content.data(), content.size()));
}

void RunTransactionTestWithRequest(net::HttpCache* cache,
                                   const MockTransaction& trans_info,
                                   const MockHttpRequest& request,
                                   std::string* response_headers) {
  TestCompletionCallback callback;

  // write to the cache

  scoped_ptr<net::HttpTransaction> trans(cache->CreateTransaction());
  ASSERT_TRUE(trans.get());

  int rv = trans->Start(&request, &callback);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  ASSERT_EQ(net::OK, rv);

  const net::HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response);

  if (response_headers)
    response->headers->GetNormalizedHeaders(response_headers);

  ReadAndVerifyTransaction(trans.get(), trans_info);
}

void RunTransactionTest(net::HttpCache* cache,
                        const MockTransaction& trans_info) {
  return RunTransactionTestWithRequest(
      cache, trans_info, MockHttpRequest(trans_info), NULL);
}

void RunTransactionTestWithResponse(net::HttpCache* cache,
                                    const MockTransaction& trans_info,
                                    std::string* response_headers) {
  return RunTransactionTestWithRequest(
      cache, trans_info, MockHttpRequest(trans_info), response_headers);
}

// This class provides a handler for kFastNoStoreGET_Transaction so that the
// no-store header can be included on demand.
class FastTransactionServer {
 public:
  FastTransactionServer() {
    no_store = false;
  }
  ~FastTransactionServer() {}

  void set_no_store(bool value) { no_store = value; }

  static void FastNoStoreHandler(const net::HttpRequestInfo* request,
                                 std::string* response_status,
                                 std::string* response_headers,
                                 std::string* response_data) {
    if (no_store)
      *response_headers = "Cache-Control: no-store\n";
  }

 private:
  static bool no_store;
  DISALLOW_COPY_AND_ASSIGN(FastTransactionServer);
};
bool FastTransactionServer::no_store;

const MockTransaction kFastNoStoreGET_Transaction = {
  "http://www.google.com/nostore",
  "GET",
  "",
  net::LOAD_VALIDATE_CACHE,
  "HTTP/1.1 200 OK",
  "Cache-Control: max-age=10000\n",
  "<html><body>Google Blah Blah</body></html>",
  TEST_MODE_SYNC_NET_START,
  &FastTransactionServer::FastNoStoreHandler,
  0
};

// This class provides a handler for kRangeGET_TransactionOK so that the range
// request can be served on demand.
class RangeTransactionServer {
 public:
  RangeTransactionServer() {
    no_store = false;
  }
  ~RangeTransactionServer() {}

  void set_no_store(bool value) { no_store = value; }

  static void RangeHandler(const net::HttpRequestInfo* request,
                           std::string* response_status,
                           std::string* response_headers,
                           std::string* response_data);

 private:
  static bool no_store;
  DISALLOW_COPY_AND_ASSIGN(RangeTransactionServer);
};

// Static.
void RangeTransactionServer::RangeHandler(const net::HttpRequestInfo* request,
                                          std::string* response_status,
                                          std::string* response_headers,
                                          std::string* response_data) {
  if (request->extra_headers.empty())
    return;

  std::vector<net::HttpByteRange> ranges;
  if (!net::HttpUtil::ParseRanges(request->extra_headers, &ranges) ||
      ranges.size() != 1)
    return;
  // We can handle this range request.
  net::HttpByteRange byte_range = ranges[0];
  EXPECT_TRUE(byte_range.ComputeBounds(80));
  int start = static_cast<int>(byte_range.first_byte_position());
  int end = static_cast<int>(byte_range.last_byte_position());

  EXPECT_LT(end, 80);

  std::string content_range = StringPrintf("Content-Range: bytes %d-%d/80\n",
                                           start, end);
  response_headers->append(content_range);

  if (request->extra_headers.find("If-None-Match") == std::string::npos) {
    EXPECT_EQ(9, end - start);
    std::string data = StringPrintf("rg: %02d-%02d ", start, end);
    *response_data = data;
  } else {
    response_status->assign("HTTP/1.1 304 Not Modified");
    response_data->clear();
  }
}

const MockTransaction kRangeGET_TransactionOK = {
  "http://www.google.com/range",
  "GET",
  "Range: bytes = 40-49\r\n",
  net::LOAD_NORMAL,
  "HTTP/1.1 206 Partial Content",
  "Last-Modified: Sat, 18 Apr 2009 01:10:43 GMT\n"
  "ETag: \"foo\"\n"
  "Accept-Ranges: bytes\n"
  "Content-Length: 10\n",
  "rg: 40-49 ",
  TEST_MODE_NORMAL,
  &RangeTransactionServer::RangeHandler,
  0
};

// Returns true if the response headers (|response|) match a partial content
// response for the range starting at |start| and ending at |end|.
bool Verify206Response(std::string response, int start, int end) {
  std::string raw_headers(net::HttpUtil::AssembleRawHeaders(response.data(),
                                                            response.size()));
  scoped_refptr<net::HttpResponseHeaders> headers =
      new net::HttpResponseHeaders(raw_headers);

  if (206 != headers->response_code())
    return false;

  int64 range_start, range_end, object_size;
  if (!headers->GetContentRange(&range_start, &range_end, &object_size))
    return false;
  int64 content_length = headers->GetContentLength();

  int length = end - start + 1;
  if (content_length != length)
    return false;

  if (range_start != start)
    return false;

  if (range_end != end)
    return false;

  return true;
}

}  // namespace


//-----------------------------------------------------------------------------
// tests


TEST(HttpCache, CreateThenDestroy) {
  MockHttpCache cache;

  scoped_ptr<net::HttpTransaction> trans(
      cache.http_cache()->CreateTransaction());
  ASSERT_TRUE(trans.get());
}

TEST(HttpCache, SimpleGET) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGETNoDiskCache) {
  MockHttpCache cache;

  cache.disk_cache()->set_fail_requests();

  // Read from the network, and don't use the cache.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadOnlyFromCache_Hit) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // force this transaction to read from the cache
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= net::LOAD_ONLY_FROM_CACHE;

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadOnlyFromCache_Miss) {
  MockHttpCache cache;

  // force this transaction to read from the cache
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= net::LOAD_ONLY_FROM_CACHE;

  MockHttpRequest request(transaction);
  TestCompletionCallback callback;

  scoped_ptr<net::HttpTransaction> trans(
      cache.http_cache()->CreateTransaction());
  ASSERT_TRUE(trans.get());

  int rv = trans->Start(&request, &callback);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  ASSERT_EQ(net::ERR_CACHE_MISS, rv);

  trans.reset();

  EXPECT_EQ(0, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadPreferringCache_Hit) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // force this transaction to read from the cache if valid
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= net::LOAD_PREFERRING_CACHE;

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadPreferringCache_Miss) {
  MockHttpCache cache;

  // force this transaction to read from the cache if valid
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= net::LOAD_PREFERRING_CACHE;

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadBypassCache) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // force this transaction to write to the cache again
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= net::LOAD_BYPASS_CACHE;

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadBypassCache_Implicit) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // force this transaction to write to the cache again
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.request_headers = "pragma: no-cache";

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadBypassCache_Implicit2) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // force this transaction to write to the cache again
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.request_headers = "cache-control: no-cache";

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadValidateCache) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // read from the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // force this transaction to validate the cache
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= net::LOAD_VALIDATE_CACHE;

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadValidateCache_Implicit) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // read from the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // force this transaction to validate the cache
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.request_headers = "cache-control: max-age=0";

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

struct Context {
  int result;
  TestCompletionCallback callback;
  scoped_ptr<net::HttpTransaction> trans;

  Context(net::HttpTransaction* t) : result(net::ERR_IO_PENDING), trans(t) {
  }
};

TEST(HttpCache, SimpleGET_ManyReaders) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  std::vector<Context*> context_list;
  const int kNumTransactions = 5;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(
        new Context(cache.http_cache()->CreateTransaction()));

    Context* c = context_list[i];
    int rv = c->trans->Start(&request, &c->callback);
    if (rv != net::ERR_IO_PENDING)
      c->result = rv;
  }

  // the first request should be a writer at this point, and the subsequent
  // requests should be pending.

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  for (int i = 0; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    if (c->result == net::ERR_IO_PENDING)
      c->result = c->callback.WaitForResult();
    ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);
  }

  // we should not have had to re-open the disk entry

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  for (int i = 0; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    delete c;
  }
}

// This is a test for http://code.google.com/p/chromium/issues/detail?id=4769.
// If cancelling a request is racing with another request for the same resource
// finishing, we have to make sure that we remove both transactions from the
// entry.
TEST(HttpCache, SimpleGET_RacingReaders) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);
  MockHttpRequest reader_request(kSimpleGET_Transaction);
  reader_request.load_flags = net::LOAD_ONLY_FROM_CACHE;

  std::vector<Context*> context_list;
  const int kNumTransactions = 5;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(
        new Context(cache.http_cache()->CreateTransaction()));

    Context* c = context_list[i];
    MockHttpRequest* this_request = &request;
    if (i == 1 || i == 2)
      this_request = &reader_request;

    int rv = c->trans->Start(this_request, &c->callback);
    if (rv != net::ERR_IO_PENDING)
      c->result = rv;
  }

  // The first request should be a writer at this point, and the subsequent
  // requests should be pending.

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  Context* c = context_list[0];
  ASSERT_EQ(net::ERR_IO_PENDING, c->result);
  c->result = c->callback.WaitForResult();
  ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);

  // Now we have 2 active readers and two queued transactions.

  c = context_list[1];
  ASSERT_EQ(net::ERR_IO_PENDING, c->result);
  c->result = c->callback.WaitForResult();
  ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);

  // At this point we have one reader, two pending transactions and a task on
  // the queue to move to the next transaction. Now we cancel the request that
  // is the current reader, and expect the queued task to be able to start the
  // next request.

  c = context_list[2];
  c->trans.reset();

  for (int i = 3; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    if (c->result == net::ERR_IO_PENDING)
      c->result = c->callback.WaitForResult();
    ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);
  }

  // We should not have had to re-open the disk entry.

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  for (int i = 0; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    delete c;
  }
}

// This is a test for http://code.google.com/p/chromium/issues/detail?id=4731.
// We may attempt to delete an entry synchronously with the act of adding a new
// transaction to said entry.
TEST(HttpCache, FastNoStoreGET_DoneWithPending) {
  MockHttpCache cache;

  // The headers will be served right from the call to Start() the request.
  MockHttpRequest request(kFastNoStoreGET_Transaction);
  FastTransactionServer request_handler;
  AddMockTransaction(&kFastNoStoreGET_Transaction);

  std::vector<Context*> context_list;
  const int kNumTransactions = 3;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(
        new Context(cache.http_cache()->CreateTransaction()));

    Context* c = context_list[i];
    int rv = c->trans->Start(&request, &c->callback);
    if (rv != net::ERR_IO_PENDING)
      c->result = rv;
  }

  // The first request should be a writer at this point, and the subsequent
  // requests should be pending.

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Now, make sure that the second request asks for the entry not to be stored.
  request_handler.set_no_store(true);

  for (int i = 0; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    if (c->result == net::ERR_IO_PENDING)
      c->result = c->callback.WaitForResult();
    ReadAndVerifyTransaction(c->trans.get(), kFastNoStoreGET_Transaction);
    delete c;
  }

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  RemoveMockTransaction(&kFastNoStoreGET_Transaction);
}

TEST(HttpCache, SimpleGET_ManyWriters_CancelFirst) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  std::vector<Context*> context_list;
  const int kNumTransactions = 2;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(
        new Context(cache.http_cache()->CreateTransaction()));

    Context* c = context_list[i];
    int rv = c->trans->Start(&request, &c->callback);
    if (rv != net::ERR_IO_PENDING)
      c->result = rv;
  }

  // the first request should be a writer at this point, and the subsequent
  // requests should be pending.

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  for (int i = 0; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    if (c->result == net::ERR_IO_PENDING)
      c->result = c->callback.WaitForResult();
    // destroy only the first transaction
    if (i == 0) {
      delete c;
      context_list[i] = NULL;
    }
  }

  // complete the rest of the transactions
  for (int i = 1; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);
  }

  // we should have had to re-open the disk entry

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  for (int i = 1; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    delete c;
  }
}

TEST(HttpCache, SimpleGET_AbandonedCacheRead) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  MockHttpRequest request(kSimpleGET_Transaction);
  TestCompletionCallback callback;

  scoped_ptr<net::HttpTransaction> trans(
      cache.http_cache()->CreateTransaction());
  int rv = trans->Start(&request, &callback);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  ASSERT_EQ(net::OK, rv);

  scoped_refptr<net::IOBuffer> buf = new net::IOBuffer(256);
  rv = trans->Read(buf, 256, &callback);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  // Test that destroying the transaction while it is reading from the cache
  // works properly.
  trans.reset();

  // Make sure we pump any pending events, which should include a call to
  // HttpCache::Transaction::OnCacheReadCompleted.
  MessageLoop::current()->RunAllPending();
}

TEST(HttpCache, TypicalGET_ConditionalRequest) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kTypicalGET_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // get the same URL again, but this time we expect it to result
  // in a conditional request.
  RunTransactionTest(cache.http_cache(), kTypicalGET_Transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

static void ETagGet_ConditionalRequest_Handler(
    const net::HttpRequestInfo* request,
    std::string* response_status,
    std::string* response_headers,
    std::string* response_data) {
  EXPECT_TRUE(request->extra_headers.find("If-None-Match") !=
                  std::string::npos);
  response_status->assign("HTTP/1.1 304 Not Modified");
  response_headers->assign(kETagGET_Transaction.response_headers);
  response_data->clear();
}

TEST(HttpCache, ETagGET_ConditionalRequest_304) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kETagGET_Transaction);

  // write to the cache
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // get the same URL again, but this time we expect it to result
  // in a conditional request.
  transaction.load_flags = net::LOAD_VALIDATE_CACHE;
  transaction.handler = ETagGet_ConditionalRequest_Handler;
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

static void ETagGet_ConditionalRequest_NoStore_Handler(
    const net::HttpRequestInfo* request,
    std::string* response_status,
    std::string* response_headers,
    std::string* response_data) {
  EXPECT_TRUE(request->extra_headers.find("If-None-Match") !=
              std::string::npos);
  response_status->assign("HTTP/1.1 304 Not Modified");
  response_headers->assign("Cache-Control: no-store\n");
  response_data->clear();
}

TEST(HttpCache, ETagGET_ConditionalRequest_304_NoStore) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kETagGET_Transaction);

  // Write to the cache.
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Get the same URL again, but this time we expect it to result
  // in a conditional request.
  transaction.load_flags = net::LOAD_VALIDATE_CACHE;
  transaction.handler = ETagGet_ConditionalRequest_NoStore_Handler;
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  ScopedMockTransaction transaction2(kETagGET_Transaction);

  // Write to the cache again. This should create a new entry.
  RunTransactionTest(cache.http_cache(), transaction2);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimplePOST_SkipsCache) {
  MockHttpCache cache;

  // Test that we skip the cache for POST requests that do not have an upload
  // identifier.

  RunTransactionTest(cache.http_cache(), kSimplePOST_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimplePOST_LoadOnlyFromCache_Miss) {
  MockHttpCache cache;

  // Test that we skip the cache for POST requests.  Eventually, we will want
  // to cache these, but we'll still have cases where skipping the cache makes
  // sense, so we want to make sure that it works properly.

  MockTransaction transaction(kSimplePOST_Transaction);
  transaction.load_flags |= net::LOAD_ONLY_FROM_CACHE;

  MockHttpRequest request(transaction);
  TestCompletionCallback callback;

  scoped_ptr<net::HttpTransaction> trans(
      cache.http_cache()->CreateTransaction());
  ASSERT_TRUE(trans.get());

  int rv = trans->Start(&request, &callback);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  ASSERT_EQ(net::ERR_CACHE_MISS, rv);

  trans.reset();

  EXPECT_EQ(0, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimplePOST_LoadOnlyFromCache_Hit) {
  MockHttpCache cache;

  // Test that we hit the cache for POST requests.

  MockTransaction transaction(kSimplePOST_Transaction);

  const int64 kUploadId = 1;  // Just a dummy value.

  MockHttpRequest request(transaction);
  request.upload_data = new net::UploadData();
  request.upload_data->set_identifier(kUploadId);
  request.upload_data->AppendBytes("hello", 5);

  // Populate the cache.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, request, NULL);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Load from cache.
  request.load_flags |= net::LOAD_ONLY_FROM_CACHE;
  RunTransactionTestWithRequest(cache.http_cache(), transaction, request, NULL);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

TEST(HttpCache, RangeGET_SkipsCache) {
  MockHttpCache cache;

  // Test that we skip the cache for range GET requests.  Eventually, we will
  // want to cache these, but we'll still have cases where skipping the cache
  // makes sense, so we want to make sure that it works properly.

  RunTransactionTest(cache.http_cache(), kRangeGET_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());

  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.request_headers = "If-None-Match: foo";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());

  transaction.request_headers =
      "If-Modified-Since: Wed, 28 Nov 2007 00:45:20 GMT";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

TEST(HttpCache, GET_Crazy206) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);

  // Test that receiving 206 for a regular request is handled correctly.

  // Write to the cache.
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // This should read again from the net.
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

TEST(HttpCache, DISABLED_RangeGET_OK) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);

  // Test that we can cache range requests and fetch random blocks from the
  // cache and the network.

  std::string headers;

  // Write to the cache (40-49).
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  EXPECT_TRUE(Verify206Response(headers, 40, 49));
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Read from the cache (40-49).
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  EXPECT_TRUE(Verify206Response(headers, 40, 49));
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure we are done with the previous transaction.
  MessageLoop::current()->RunAllPending();

  // Write to the cache (30-39).
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = 30-39\r\n";
  transaction.data = "rg: 30-39 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_TRUE(Verify206Response(headers, 30, 39));
  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure we are done with the previous transaction.
  MessageLoop::current()->RunAllPending();

  // Write and read from the cache (20-59).
  transaction.request_headers = "Range: bytes = 20-59\r\n";
  transaction.data = "rg: 20-29 rg: 30-39 rg: 40-49 rg: 50-59 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_TRUE(Verify206Response(headers, 20, 59));
  EXPECT_EQ(6, cache.network_layer()->transaction_count());
  EXPECT_EQ(3, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

TEST(HttpCache, DISABLED_UnknownRangeGET_1) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);

  // Test that we can cache range requests when the start or end is unknown.
  // We start with one suffix request, followed by a request from a given point.

  std::string headers;

  // Write to the cache (70-79).
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = -10\r\n";
  transaction.data = "rg: 70-79 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_TRUE(Verify206Response(headers, 70, 79));
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure we are done with the previous transaction.
  MessageLoop::current()->RunAllPending();

  // Write and read from the cache (60-79).
  transaction.request_headers = "Range: bytes = 60-\r\n";
  transaction.data = "rg: 60-69 rg: 70-79 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_TRUE(Verify206Response(headers, 60, 79));
  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

TEST(HttpCache, DISABLED_UnknownRangeGET_2) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);

  // Test that we can cache range requests when the start or end is unknown.
  // We start with one request from a given point, followed by a suffix request.

  std::string headers;

  // Write to the cache (70-79).
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = 70-\r\n";
  transaction.data = "rg: 70-79 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_TRUE(Verify206Response(headers, 70, 79));
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure we are done with the previous transaction.
  MessageLoop::current()->RunAllPending();

  // Write and read from the cache (60-79).
  transaction.request_headers = "Range: bytes = -20\r\n";
  transaction.data = "rg: 60-69 rg: 70-79 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_TRUE(Verify206Response(headers, 60, 79));
  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

TEST(HttpCache, SyncRead) {
  MockHttpCache cache;

  // This test ensures that a read that completes synchronously does not cause
  // any problems.

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.test_mode |= (TEST_MODE_SYNC_CACHE_START |
                            TEST_MODE_SYNC_CACHE_READ);

  MockHttpRequest r1(transaction),
                  r2(transaction),
                  r3(transaction);

  TestTransactionConsumer c1(cache.http_cache()),
                          c2(cache.http_cache()),
                          c3(cache.http_cache());

  c1.Start(&r1);

  r2.load_flags |= net::LOAD_ONLY_FROM_CACHE;
  c2.Start(&r2);

  r3.load_flags |= net::LOAD_ONLY_FROM_CACHE;
  c3.Start(&r3);

  MessageLoop::current()->Run();

  EXPECT_TRUE(c1.is_done());
  EXPECT_TRUE(c2.is_done());
  EXPECT_TRUE(c3.is_done());

  EXPECT_EQ(net::OK, c1.error());
  EXPECT_EQ(net::OK, c2.error());
  EXPECT_EQ(net::OK, c3.error());
}

TEST(HttpCache, ValidationResultsIn200) {
  MockHttpCache cache;

  // This test ensures that a conditional request, which results in a 200
  // instead of a 304, properly truncates the existing response data.

  // write to the cache
  RunTransactionTest(cache.http_cache(), kETagGET_Transaction);

  // force this transaction to validate the cache
  MockTransaction transaction(kETagGET_Transaction);
  transaction.load_flags |= net::LOAD_VALIDATE_CACHE;
  RunTransactionTest(cache.http_cache(), transaction);

  // read from the cache
  RunTransactionTest(cache.http_cache(), kETagGET_Transaction);
}

TEST(HttpCache, CachedRedirect) {
  MockHttpCache cache;

  ScopedMockTransaction kTestTransaction(kSimpleGET_Transaction);
  kTestTransaction.status = "HTTP/1.1 301 Moved Permanently";
  kTestTransaction.response_headers = "Location: http://www.bar.com/\n";

  MockHttpRequest request(kTestTransaction);
  TestCompletionCallback callback;

  // write to the cache
  {
    scoped_ptr<net::HttpTransaction> trans(
        cache.http_cache()->CreateTransaction());
    ASSERT_TRUE(trans.get());

    int rv = trans->Start(&request, &callback);
    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();
    ASSERT_EQ(net::OK, rv);

    const net::HttpResponseInfo* info = trans->GetResponseInfo();
    ASSERT_TRUE(info);

    EXPECT_EQ(info->headers->response_code(), 301);

    std::string location;
    info->headers->EnumerateHeader(NULL, "Location", &location);
    EXPECT_EQ(location, "http://www.bar.com/");

    // Destroy transaction when going out of scope. We have not actually
    // read the response body -- want to test that it is still getting cached.
  }
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // read from the cache
  {
    scoped_ptr<net::HttpTransaction> trans(
        cache.http_cache()->CreateTransaction());
    ASSERT_TRUE(trans.get());

    int rv = trans->Start(&request, &callback);
    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();
    ASSERT_EQ(net::OK, rv);

    const net::HttpResponseInfo* info = trans->GetResponseInfo();
    ASSERT_TRUE(info);

    EXPECT_EQ(info->headers->response_code(), 301);

    std::string location;
    info->headers->EnumerateHeader(NULL, "Location", &location);
    EXPECT_EQ(location, "http://www.bar.com/");

    // Destroy transaction when going out of scope. We have not actually
    // read the response body -- want to test that it is still getting cached.
  }
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

TEST(HttpCache, CacheControlNoStore) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.response_headers = "cache-control: no-store\n";

  // initial load
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // try loading again; it should result in a network fetch
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  disk_cache::Entry* entry;
  bool exists = cache.disk_cache()->OpenEntry(transaction.url, &entry);
  EXPECT_FALSE(exists);
}

TEST(HttpCache, CacheControlNoStore2) {
  // this test is similar to the above test, except that the initial response
  // is cachable, but when it is validated, no-store is received causing the
  // cached document to be deleted.
  MockHttpCache cache;

  ScopedMockTransaction transaction(kETagGET_Transaction);

  // initial load
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // try loading again; it should result in a network fetch
  transaction.load_flags = net::LOAD_VALIDATE_CACHE;
  transaction.response_headers = "cache-control: no-store\n";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  disk_cache::Entry* entry;
  bool exists = cache.disk_cache()->OpenEntry(transaction.url, &entry);
  EXPECT_FALSE(exists);
}

TEST(HttpCache, CacheControlNoStore3) {
  // this test is similar to the above test, except that the response is a 304
  // instead of a 200.  this should never happen in practice, but it seems like
  // a good thing to verify that we still destroy the cache entry.
  MockHttpCache cache;

  ScopedMockTransaction transaction(kETagGET_Transaction);

  // initial load
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // try loading again; it should result in a network fetch
  transaction.load_flags = net::LOAD_VALIDATE_CACHE;
  transaction.response_headers = "cache-control: no-store\n";
  transaction.status = "HTTP/1.1 304 Not Modified";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  disk_cache::Entry* entry;
  bool exists = cache.disk_cache()->OpenEntry(transaction.url, &entry);
  EXPECT_FALSE(exists);
}

// Ensure that we don't cache requests served over bad HTTPS.
TEST(HttpCache, SimpleGET_SSLError) {
  MockHttpCache cache;

  MockTransaction transaction = kSimpleGET_Transaction;
  transaction.cert_status = net::CERT_STATUS_REVOKED;
  ScopedMockTransaction scoped_transaction(transaction);

  // write to the cache
  RunTransactionTest(cache.http_cache(), transaction);

  // Test that it was not cached.
  transaction.load_flags |= net::LOAD_ONLY_FROM_CACHE;

  MockHttpRequest request(transaction);
  TestCompletionCallback callback;

  scoped_ptr<net::HttpTransaction> trans(
      cache.http_cache()->CreateTransaction());
  ASSERT_TRUE(trans.get());

  int rv = trans->Start(&request, &callback);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  ASSERT_EQ(net::ERR_CACHE_MISS, rv);
}

// Ensure that we don't crash by if left-behind transactions.
TEST(HttpCache, OutlivedTransactions) {
  MockHttpCache* cache = new MockHttpCache;

  net::HttpTransaction* trans = cache->http_cache()->CreateTransaction();
  delete cache;
  delete trans;
}

// Test that the disabled mode works.
TEST(HttpCache, CacheDisabledMode) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // go into disabled mode
  cache.http_cache()->set_mode(net::HttpCache::DISABLE);

  // force this transaction to write to the cache again
  MockTransaction transaction(kSimpleGET_Transaction);

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}
