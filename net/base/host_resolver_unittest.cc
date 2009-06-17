// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_resolver.h"

#if defined(OS_WIN)
#include <ws2tcpip.h>
#include <wspiapi.h>
#elif defined(OS_POSIX)
#include <netdb.h>
#endif

#include <string>

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/host_resolver_unittest.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::RuleBasedHostMapper;
using net::ScopedHostMapper;
using net::WaitingHostMapper;

// TODO(eroman):
//  - Test mixing async with sync (in particular how does sync update the
//    cache while an async is already pending).

namespace {

// A variant of WaitingHostMapper that pushes each host mapped into a list.
// (and uses a manual-reset event rather than auto-reset).
class CapturingHostMapper : public net::HostMapper {
 public:
  CapturingHostMapper() : event_(true, false) {
  }

  void Signal() {
    event_.Signal();
  }

  virtual std::string Map(const std::string& host) {
    event_.Wait();
    {
      AutoLock l(lock_);
      capture_list_.push_back(host);
    }
    return MapUsingPrevious(host);
  }

  std::vector<std::string> GetCaptureList() const {
    std::vector<std::string> copy;
    {
      AutoLock l(lock_);
      copy = capture_list_;
    }
    return copy;
  }

 private:
  std::vector<std::string> capture_list_;
  mutable Lock lock_;
  base::WaitableEvent event_;
};

// Helper that represents a single Resolve() result, used to inspect all the
// resolve results by forwarding them to Delegate.
class ResolveRequest {
 public:
  // Delegate interface, for notification when the ResolveRequest completes.
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnCompleted(ResolveRequest* resolve) = 0;
  };

  ResolveRequest(net::HostResolver* resolver,
                 const std::string& hostname,
                 int port,
                 Delegate* delegate)
      : info_(hostname, port), resolver_(resolver), delegate_(delegate),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            callback_(this, &ResolveRequest::OnLookupFinished)) {
    // Start the request.
    int err = resolver->Resolve(info_, &addrlist_, &callback_, &req_);
    EXPECT_EQ(net::ERR_IO_PENDING, err);
  }

  ResolveRequest(net::HostResolver* resolver,
                 const net::HostResolver::RequestInfo& info,
                 Delegate* delegate)
      : info_(info), resolver_(resolver), delegate_(delegate),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            callback_(this, &ResolveRequest::OnLookupFinished)) {
    // Start the request.
    int err = resolver->Resolve(info, &addrlist_, &callback_, &req_);
    EXPECT_EQ(net::ERR_IO_PENDING, err);
  }

  void Cancel() {
    resolver_->CancelRequest(req_);
  }

  const std::string& hostname() const {
    return info_.hostname();
  }

  int port() const {
    return info_.port();
  }

  int result() const {
    return result_;
  }

  const net::AddressList& addrlist() const {
    return addrlist_;
  }

  net::HostResolver* resolver() const {
    return resolver_;
  }

 private:
  void OnLookupFinished(int result) {
    result_ = result;
    delegate_->OnCompleted(this);
  }

  // The request details.
  net::HostResolver::RequestInfo info_;
  net::HostResolver::Request* req_;

  // The result of the resolve.
  int result_;
  net::AddressList addrlist_;

  net::HostResolver* resolver_;

  Delegate* delegate_;
  net::CompletionCallbackImpl<ResolveRequest> callback_;

  DISALLOW_COPY_AND_ASSIGN(ResolveRequest);
};

class HostResolverTest : public testing::Test {
 public:
  HostResolverTest()
      : callback_called_(false),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            callback_(this, &HostResolverTest::OnLookupFinished)) {
  }

 protected:
  bool callback_called_;
  int callback_result_;
  net::CompletionCallbackImpl<HostResolverTest> callback_;

 private:
  void OnLookupFinished(int result) {
    callback_called_ = true;
    callback_result_ = result;
    MessageLoop::current()->Quit();
  }
};

TEST_F(HostResolverTest, SynchronousLookup) {
  net::HostResolver host_resolver;
  net::AddressList adrlist;
  const int kPortnum = 80;

  scoped_refptr<RuleBasedHostMapper> mapper = new RuleBasedHostMapper();
  mapper->AddRule("just.testing", "192.168.1.42");
  ScopedHostMapper scoped_mapper(mapper.get());

  net::HostResolver::RequestInfo info("just.testing", kPortnum);
  int err = host_resolver.Resolve(info, &adrlist, NULL, NULL);
  EXPECT_EQ(net::OK, err);

  const struct addrinfo* ainfo = adrlist.head();
  EXPECT_EQ(static_cast<addrinfo*>(NULL), ainfo->ai_next);
  EXPECT_EQ(sizeof(struct sockaddr_in), ainfo->ai_addrlen);

  const struct sockaddr* sa = ainfo->ai_addr;
  const struct sockaddr_in* sa_in = (const struct sockaddr_in*) sa;
  EXPECT_TRUE(htons(kPortnum) == sa_in->sin_port);
  EXPECT_TRUE(htonl(0xc0a8012a) == sa_in->sin_addr.s_addr);
}

TEST_F(HostResolverTest, AsynchronousLookup) {
  net::HostResolver host_resolver;
  net::AddressList adrlist;
  const int kPortnum = 80;

  scoped_refptr<RuleBasedHostMapper> mapper = new RuleBasedHostMapper();
  mapper->AddRule("just.testing", "192.168.1.42");
  ScopedHostMapper scoped_mapper(mapper.get());

  net::HostResolver::RequestInfo info("just.testing", kPortnum);
  int err = host_resolver.Resolve(info, &adrlist, &callback_, NULL);
  EXPECT_EQ(net::ERR_IO_PENDING, err);

  MessageLoop::current()->Run();

  ASSERT_TRUE(callback_called_);
  ASSERT_EQ(net::OK, callback_result_);

  const struct addrinfo* ainfo = adrlist.head();
  EXPECT_EQ(static_cast<addrinfo*>(NULL), ainfo->ai_next);
  EXPECT_EQ(sizeof(struct sockaddr_in), ainfo->ai_addrlen);

  const struct sockaddr* sa = ainfo->ai_addr;
  const struct sockaddr_in* sa_in = (const struct sockaddr_in*) sa;
  EXPECT_TRUE(htons(kPortnum) == sa_in->sin_port);
  EXPECT_TRUE(htonl(0xc0a8012a) == sa_in->sin_addr.s_addr);
}

TEST_F(HostResolverTest, CanceledAsynchronousLookup) {
  scoped_refptr<WaitingHostMapper> mapper = new WaitingHostMapper();
  ScopedHostMapper scoped_mapper(mapper.get());

  {
    net::HostResolver host_resolver;
    net::AddressList adrlist;
    const int kPortnum = 80;

    net::HostResolver::RequestInfo info("just.testing", kPortnum);
    int err = host_resolver.Resolve(info, &adrlist, &callback_, NULL);
    EXPECT_EQ(net::ERR_IO_PENDING, err);

    // Make sure we will exit the queue even when callback is not called.
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                            new MessageLoop::QuitTask(),
                                            1000);
    MessageLoop::current()->Run();
  }

  mapper->Signal();

  EXPECT_FALSE(callback_called_);
}

TEST_F(HostResolverTest, NumericIPv4Address) {
  // Stevens says dotted quads with AI_UNSPEC resolve to a single sockaddr_in.

  scoped_refptr<RuleBasedHostMapper> mapper = new RuleBasedHostMapper();
  mapper->AllowDirectLookup("*");
  ScopedHostMapper scoped_mapper(mapper.get());

  net::HostResolver host_resolver;
  net::AddressList adrlist;
  const int kPortnum = 5555;
  net::HostResolver::RequestInfo info("127.1.2.3", kPortnum);
  int err = host_resolver.Resolve(info, &adrlist, NULL, NULL);
  EXPECT_EQ(net::OK, err);

  const struct addrinfo* ainfo = adrlist.head();
  EXPECT_EQ(static_cast<addrinfo*>(NULL), ainfo->ai_next);
  EXPECT_EQ(sizeof(struct sockaddr_in), ainfo->ai_addrlen);

  const struct sockaddr* sa = ainfo->ai_addr;
  const struct sockaddr_in* sa_in = (const struct sockaddr_in*) sa;
  EXPECT_TRUE(htons(kPortnum) == sa_in->sin_port);
  EXPECT_TRUE(htonl(0x7f010203) == sa_in->sin_addr.s_addr);
}

TEST_F(HostResolverTest, NumericIPv6Address) {
  scoped_refptr<RuleBasedHostMapper> mapper = new RuleBasedHostMapper();
  mapper->AllowDirectLookup("*");
  ScopedHostMapper scoped_mapper(mapper.get());

  // Resolve a plain IPv6 address.  Don't worry about [brackets], because
  // the caller should have removed them.
  net::HostResolver host_resolver;
  net::AddressList adrlist;
  const int kPortnum = 5555;
  net::HostResolver::RequestInfo info("2001:db8::1", kPortnum);
  int err = host_resolver.Resolve(info, &adrlist, NULL, NULL);
  // On computers without IPv6 support, getaddrinfo cannot convert IPv6
  // address literals to addresses (getaddrinfo returns EAI_NONAME).  So this
  // test has to allow host_resolver.Resolve to fail.
  if (err == net::ERR_NAME_NOT_RESOLVED)
    return;
  EXPECT_EQ(net::OK, err);

  const struct addrinfo* ainfo = adrlist.head();
  EXPECT_EQ(static_cast<addrinfo*>(NULL), ainfo->ai_next);
  EXPECT_EQ(sizeof(struct sockaddr_in6), ainfo->ai_addrlen);

  const struct sockaddr* sa = ainfo->ai_addr;
  const struct sockaddr_in6* sa_in6 = (const struct sockaddr_in6*) sa;
  EXPECT_TRUE(htons(kPortnum) == sa_in6->sin6_port);

  const uint8 expect_addr[] = {
    0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
  };
  for (int i = 0; i < 16; i++) {
    EXPECT_EQ(expect_addr[i], sa_in6->sin6_addr.s6_addr[i]);
  }
}

TEST_F(HostResolverTest, EmptyHost) {
  scoped_refptr<RuleBasedHostMapper> mapper = new RuleBasedHostMapper();
  mapper->AllowDirectLookup("*");
  ScopedHostMapper scoped_mapper(mapper.get());

  net::HostResolver host_resolver;
  net::AddressList adrlist;
  const int kPortnum = 5555;
  net::HostResolver::RequestInfo info("", kPortnum);
  int err = host_resolver.Resolve(info, &adrlist, NULL, NULL);
  EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, err);
}

// Helper class used by HostResolverTest.DeDupeRequests. It receives request
// completion notifications for all the resolves, so it can tally up and
// determine when we are done.
class DeDupeRequestsVerifier : public ResolveRequest::Delegate {
 public:
  explicit DeDupeRequestsVerifier(CapturingHostMapper* mapper)
      : count_a_(0), count_b_(0), mapper_(mapper) {}

  // The test does 5 resolves (which can complete in any order).
  virtual void OnCompleted(ResolveRequest* resolve) {
    // Tally up how many requests we have seen.
    if (resolve->hostname() == "a") {
      count_a_++;
    } else if (resolve->hostname() == "b") {
      count_b_++;
    } else {
      FAIL() << "Unexpected hostname: " << resolve->hostname();
    }

    // Check that the port was set correctly.
    EXPECT_EQ(resolve->port(), resolve->addrlist().GetPort());

    // Check whether all the requests have finished yet.
    int total_completions = count_a_ + count_b_;
    if (total_completions == 5) {
      EXPECT_EQ(2, count_a_);
      EXPECT_EQ(3, count_b_);

      // The mapper should have been called only twice -- once with "a", once
      // with "b".
      std::vector<std::string> capture_list = mapper_->GetCaptureList();
      EXPECT_EQ(2U, capture_list.size());

      // End this test, we are done.
      MessageLoop::current()->Quit();
    }
  }

 private:
  int count_a_;
  int count_b_;
  CapturingHostMapper* mapper_;

  DISALLOW_COPY_AND_ASSIGN(DeDupeRequestsVerifier);
};

TEST_F(HostResolverTest, DeDupeRequests) {
  // Use a capturing mapper, since the verifier needs to know what calls
  // reached Map().  Also, the capturing mapper is initially blocked.
  scoped_refptr<CapturingHostMapper> mapper = new CapturingHostMapper();
  ScopedHostMapper scoped_mapper(mapper.get());

  net::HostResolver host_resolver;

  // The class will receive callbacks for when each resolve completes. It
  // checks that the right things happened.
  DeDupeRequestsVerifier verifier(mapper.get());

  // Start 5 requests, duplicating hosts "a" and "b". Since the mapper is
  // blocked, these should all pile up until we signal it.

  ResolveRequest req1(&host_resolver, "a", 80, &verifier);
  ResolveRequest req2(&host_resolver, "b", 80, &verifier);
  ResolveRequest req3(&host_resolver, "b", 81, &verifier);
  ResolveRequest req4(&host_resolver, "a", 82, &verifier);
  ResolveRequest req5(&host_resolver, "b", 83, &verifier);

  // Ready, Set, GO!!!
  mapper->Signal();

  // |verifier| will send quit message once all the requests have finished.
  MessageLoop::current()->Run();
}

// Helper class used by HostResolverTest.CancelMultipleRequests.
class CancelMultipleRequestsVerifier : public ResolveRequest::Delegate {
 public:
  CancelMultipleRequestsVerifier() {}

  // The cancels kill all but one request.
  virtual void OnCompleted(ResolveRequest* resolve) {
    EXPECT_EQ("a", resolve->hostname());
    EXPECT_EQ(82, resolve->port());

    // Check that the port was set correctly.
    EXPECT_EQ(resolve->port(), resolve->addrlist().GetPort());

    // End this test, we are done.
    MessageLoop::current()->Quit();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CancelMultipleRequestsVerifier);
};

TEST_F(HostResolverTest, CancelMultipleRequests) {
  // Use a capturing mapper, since the verifier needs to know what calls
  // reached Map().  Also, the capturing mapper is initially blocked.
  scoped_refptr<CapturingHostMapper> mapper = new CapturingHostMapper();
  ScopedHostMapper scoped_mapper(mapper.get());

  net::HostResolver host_resolver;

  // The class will receive callbacks for when each resolve completes. It
  // checks that the right things happened.
  CancelMultipleRequestsVerifier verifier;

  // Start 5 requests, duplicating hosts "a" and "b". Since the mapper is
  // blocked, these should all pile up until we signal it.

  ResolveRequest req1(&host_resolver, "a", 80, &verifier);
  ResolveRequest req2(&host_resolver, "b", 80, &verifier);
  ResolveRequest req3(&host_resolver, "b", 81, &verifier);
  ResolveRequest req4(&host_resolver, "a", 82, &verifier);
  ResolveRequest req5(&host_resolver, "b", 83, &verifier);

  // Cancel everything except request 4.
  req1.Cancel();
  req2.Cancel();
  req3.Cancel();
  req5.Cancel();

  // Ready, Set, GO!!!
  mapper->Signal();

  // |verifier| will send quit message once all the requests have finished.
  MessageLoop::current()->Run();
}

// Helper class used by HostResolverTest.CancelWithinCallback.
class CancelWithinCallbackVerifier : public ResolveRequest::Delegate {
 public:
  CancelWithinCallbackVerifier()
      : req_to_cancel1_(NULL), req_to_cancel2_(NULL), num_completions_(0) {
  }

  virtual void OnCompleted(ResolveRequest* resolve) {
    num_completions_++;

    // Port 80 is the first request that the callback will be invoked for.
    // While we are executing within that callback, cancel the other requests
    // in the job and start another request.
    if (80 == resolve->port()) {
      EXPECT_EQ("a", resolve->hostname());

      req_to_cancel1_->Cancel();
      req_to_cancel2_->Cancel();

      // Start a request (so we can make sure the canceled requests don't
      // complete before "finalrequest" finishes.
      final_request_.reset(new ResolveRequest(
          resolve->resolver(), "finalrequest", 70, this));

    } else if (83 == resolve->port()) {
      EXPECT_EQ("a", resolve->hostname());
    } else if (resolve->hostname() == "finalrequest") {
      EXPECT_EQ(70, resolve->addrlist().GetPort());

      // End this test, we are done.
      MessageLoop::current()->Quit();
    } else {
      FAIL() << "Unexpected completion: " << resolve->hostname() << ", "
             << resolve->port();
    }
  }

  void SetRequestsToCancel(ResolveRequest* req_to_cancel1,
                           ResolveRequest* req_to_cancel2) {
    req_to_cancel1_ = req_to_cancel1;
    req_to_cancel2_ = req_to_cancel2;
  }

 private:
  scoped_ptr<ResolveRequest> final_request_;
  ResolveRequest* req_to_cancel1_;
  ResolveRequest* req_to_cancel2_;
  int num_completions_;
  DISALLOW_COPY_AND_ASSIGN(CancelWithinCallbackVerifier);
};

TEST_F(HostResolverTest, CancelWithinCallback) {
  // Use a capturing mapper, since the verifier needs to know what calls
  // reached Map().  Also, the capturing mapper is initially blocked.
  scoped_refptr<CapturingHostMapper> mapper = new CapturingHostMapper();
  ScopedHostMapper scoped_mapper(mapper.get());

  net::HostResolver host_resolver;

  // The class will receive callbacks for when each resolve completes. It
  // checks that the right things happened.
  CancelWithinCallbackVerifier verifier;

  // Start 4 requests, duplicating hosts "a". Since the mapper is
  // blocked, these should all pile up until we signal it.

  ResolveRequest req1(&host_resolver, "a", 80, &verifier);
  ResolveRequest req2(&host_resolver, "a", 81, &verifier);
  ResolveRequest req3(&host_resolver, "a", 82, &verifier);
  ResolveRequest req4(&host_resolver, "a", 83, &verifier);

  // Once "a:80" completes, it will cancel "a:81" and "a:82".
  verifier.SetRequestsToCancel(&req2, &req3);

  // Ready, Set, GO!!!
  mapper->Signal();

  // |verifier| will send quit message once all the requests have finished.
  MessageLoop::current()->Run();
}

// Helper class used by HostResolverTest.DeleteWithinCallback.
class DeleteWithinCallbackVerifier : public ResolveRequest::Delegate {
 public:
  DeleteWithinCallbackVerifier() {}

  virtual void OnCompleted(ResolveRequest* resolve) {
    EXPECT_EQ("a", resolve->hostname());
    EXPECT_EQ(80, resolve->port());
    delete resolve->resolver();

    // Quit after returning from OnCompleted (to give it a chance at
    // incorrectly running the cancelled tasks).
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeleteWithinCallbackVerifier);
};

TEST_F(HostResolverTest, DeleteWithinCallback) {
  // Use a capturing mapper, since the verifier needs to know what calls
  // reached Map().  Also, the capturing mapper is initially blocked.
  scoped_refptr<CapturingHostMapper> mapper = new CapturingHostMapper();
  ScopedHostMapper scoped_mapper(mapper.get());

  // This should be deleted by DeleteWithinCallbackVerifier -- if it leaks
  // then the test has failed.
  net::HostResolver* host_resolver = new net::HostResolver;

  // The class will receive callbacks for when each resolve completes. It
  // checks that the right things happened.
  DeleteWithinCallbackVerifier verifier;

  // Start 4 requests, duplicating hosts "a". Since the mapper is
  // blocked, these should all pile up until we signal it.

  ResolveRequest req1(host_resolver, "a", 80, &verifier);
  ResolveRequest req2(host_resolver, "a", 81, &verifier);
  ResolveRequest req3(host_resolver, "a", 82, &verifier);
  ResolveRequest req4(host_resolver, "a", 83, &verifier);

  // Ready, Set, GO!!!
  mapper->Signal();

  // |verifier| will send quit message once all the requests have finished.
  MessageLoop::current()->Run();
}

// Helper class used by HostResolverTest.StartWithinCallback.
class StartWithinCallbackVerifier : public ResolveRequest::Delegate {
 public:
  StartWithinCallbackVerifier() : num_requests_(0) {}

  virtual void OnCompleted(ResolveRequest* resolve) {
    EXPECT_EQ("a", resolve->hostname());

    if (80 == resolve->port()) {
      // On completing the first request, start another request for "a".
      // Since caching is disabled, this will result in another async request.
      final_request_.reset(new ResolveRequest(
        resolve->resolver(), "a", 70, this));
    }
    if (++num_requests_ == 5) {
      // Test is done.
      MessageLoop::current()->Quit();
    }
  }

 private:
  int num_requests_;
  scoped_ptr<ResolveRequest> final_request_;
  DISALLOW_COPY_AND_ASSIGN(StartWithinCallbackVerifier);
};

TEST_F(HostResolverTest, StartWithinCallback) {
  // Use a capturing mapper, since the verifier needs to know what calls
  // reached Map().  Also, the capturing mapper is initially blocked.
  scoped_refptr<CapturingHostMapper> mapper = new CapturingHostMapper();
  ScopedHostMapper scoped_mapper(mapper.get());

  // Turn off caching for this host resolver.
  net::HostResolver host_resolver(0, 0);

  // The class will receive callbacks for when each resolve completes. It
  // checks that the right things happened.
  StartWithinCallbackVerifier verifier;

  // Start 4 requests, duplicating hosts "a". Since the mapper is
  // blocked, these should all pile up until we signal it.

  ResolveRequest req1(&host_resolver, "a", 80, &verifier);
  ResolveRequest req2(&host_resolver, "a", 81, &verifier);
  ResolveRequest req3(&host_resolver, "a", 82, &verifier);
  ResolveRequest req4(&host_resolver, "a", 83, &verifier);

  // Ready, Set, GO!!!
  mapper->Signal();

  // |verifier| will send quit message once all the requests have finished.
  MessageLoop::current()->Run();
}

// Helper class used by HostResolverTest.BypassCache.
class BypassCacheVerifier : public ResolveRequest::Delegate {
 public:
  BypassCacheVerifier() {}

  virtual void OnCompleted(ResolveRequest* resolve) {
    EXPECT_EQ("a", resolve->hostname());
    net::HostResolver* resolver = resolve->resolver();

    if (80 == resolve->port()) {
      // On completing the first request, start another request for "a".
      // Since caching is enabled, this should complete synchronously.

      // Note that |junk_callback| shouldn't be used since we are going to
      // complete synchronously. We can't specify NULL though since that would
      // mean synchronous mode so we give it a value of 1.
      net::CompletionCallback* junk_callback =
          reinterpret_cast<net::CompletionCallback*> (1);
      net::AddressList addrlist;

      net::HostResolver::RequestInfo info("a", 70);
      int error = resolver->Resolve(info, &addrlist, junk_callback, NULL);
      EXPECT_EQ(net::OK, error);

      // Ok good. Now make sure that if we ask to bypass the cache, it can no
      // longer service the request synchronously.
      info = net::HostResolver::RequestInfo("a", 71);
      info.set_allow_cached_response(false);
      final_request_.reset(new ResolveRequest(resolver, info, this));
    } else if (71 == resolve->port()) {
      // Test is done.
      MessageLoop::current()->Quit();
    } else {
      FAIL() << "Unexpected port number";
    }
  }

 private:
  scoped_ptr<ResolveRequest> final_request_;
  DISALLOW_COPY_AND_ASSIGN(BypassCacheVerifier);
};

TEST_F(HostResolverTest, BypassCache) {
  net::HostResolver host_resolver;

  // The class will receive callbacks for when each resolve completes. It
  // checks that the right things happened.
  BypassCacheVerifier verifier;

  // Start a request.
  ResolveRequest req1(&host_resolver, "a", 80, &verifier);

  // |verifier| will send quit message once all the requests have finished.
  MessageLoop::current()->Run();
}

bool operator==(const net::HostResolver::RequestInfo& a,
                const net::HostResolver::RequestInfo& b) {
   return a.hostname() == b.hostname() &&
          a.port() == b.port() &&
          a.allow_cached_response() == b.allow_cached_response() &&
          a.is_speculative() == b.is_speculative() &&
          a.referrer() == b.referrer();
}

// Observer that just makes note of how it was called. The test code can then
// inspect to make sure it was called with the right parameters.
class CapturingObserver : public net::HostResolver::Observer {
 public:
  // DnsResolutionObserver methods:
  virtual void OnStartResolution(int id,
                                 const net::HostResolver::RequestInfo& info) {
    start_log.push_back(StartOrCancelEntry(id, info));
  }

  virtual void OnFinishResolutionWithStatus(
      int id,
      bool was_resolved,
      const net::HostResolver::RequestInfo& info) {
    finish_log.push_back(FinishEntry(id, was_resolved, info));
  }

  virtual void OnCancelResolution(int id,
                                  const net::HostResolver::RequestInfo& info) {
    cancel_log.push_back(StartOrCancelEntry(id, info));
  }

  // Tuple (id, info).
  struct StartOrCancelEntry {
    StartOrCancelEntry(int id, const net::HostResolver::RequestInfo& info)
        : id(id), info(info) {}

    bool operator==(const StartOrCancelEntry& other) const {
      return id == other.id && info == other.info;
    }

    int id;
    net::HostResolver::RequestInfo info;
  };

  // Tuple (id, was_resolved, info).
  struct FinishEntry {
    FinishEntry(int id, bool was_resolved,
                const net::HostResolver::RequestInfo& info)
        : id(id), was_resolved(was_resolved), info(info) {}

    bool operator==(const FinishEntry& other) const {
      return id == other.id &&
             was_resolved == other.was_resolved &&
             info == other.info;
    }

    int id;
    bool was_resolved;
    net::HostResolver::RequestInfo info;
  };

  std::vector<StartOrCancelEntry> start_log;
  std::vector<FinishEntry> finish_log;
  std::vector<StartOrCancelEntry> cancel_log;
};

// Test that registering, unregistering, and notifying of observers works.
// Does not test the cancellation notification since all resolves are
// synchronous.
TEST_F(HostResolverTest, Observers) {
  net::HostResolver host_resolver;

  CapturingObserver observer;

  host_resolver.AddObserver(&observer);

  net::AddressList addrlist;

  // Resolve "host1".
  net::HostResolver::RequestInfo info1("host1", 70);
  int rv = host_resolver.Resolve(info1, &addrlist, NULL, NULL);
  EXPECT_EQ(net::OK, rv);

  EXPECT_EQ(1U, observer.start_log.size());
  EXPECT_EQ(1U, observer.finish_log.size());
  EXPECT_EQ(0U, observer.cancel_log.size());
  EXPECT_TRUE(observer.start_log[0] ==
              CapturingObserver::StartOrCancelEntry(0, info1));
  EXPECT_TRUE(observer.finish_log[0] ==
              CapturingObserver::FinishEntry(0, true, info1));

  // Resolve "host1" again -- this time it  will be served from cache, but it
  // should still notify of completion.
  TestCompletionCallback callback;
  rv = host_resolver.Resolve(info1, &addrlist, &callback, NULL);
  ASSERT_EQ(net::OK, rv);  // Should complete synchronously.

  EXPECT_EQ(2U, observer.start_log.size());
  EXPECT_EQ(2U, observer.finish_log.size());
  EXPECT_EQ(0U, observer.cancel_log.size());
  EXPECT_TRUE(observer.start_log[1] ==
              CapturingObserver::StartOrCancelEntry(1, info1));
  EXPECT_TRUE(observer.finish_log[1] ==
              CapturingObserver::FinishEntry(1, true, info1));

  // Resolve "host2", setting referrer to "http://foobar.com"
  net::HostResolver::RequestInfo info2("host2", 70);
  info2.set_referrer(GURL("http://foobar.com"));
  rv = host_resolver.Resolve(info2, &addrlist, NULL, NULL);
  EXPECT_EQ(net::OK, rv);

  EXPECT_EQ(3U, observer.start_log.size());
  EXPECT_EQ(3U, observer.finish_log.size());
  EXPECT_EQ(0U, observer.cancel_log.size());
  EXPECT_TRUE(observer.start_log[2] ==
              CapturingObserver::StartOrCancelEntry(2, info2));
  EXPECT_TRUE(observer.finish_log[2] ==
              CapturingObserver::FinishEntry(2, true, info2));

  // Unregister the observer.
  host_resolver.RemoveObserver(&observer);

  // Resolve "host3"
  net::HostResolver::RequestInfo info3("host3", 70);
  host_resolver.Resolve(info3, &addrlist, NULL, NULL);

  // No effect this time, since observer was removed.
  EXPECT_EQ(3U, observer.start_log.size());
  EXPECT_EQ(3U, observer.finish_log.size());
  EXPECT_EQ(0U, observer.cancel_log.size());
}

// Tests that observers are sent OnCancelResolution() whenever a request is
// cancelled. There are two ways to cancel a request:
//  (1) Delete the HostResolver while job is outstanding.
//  (2) Call HostResolver::CancelRequest() while a request is outstanding.
TEST_F(HostResolverTest, CancellationObserver) {
  CapturingObserver observer;
  {
    // Create a host resolver and attach an observer.
    net::HostResolver host_resolver;
    host_resolver.AddObserver(&observer);

    TestCompletionCallback callback;

    EXPECT_EQ(0U, observer.start_log.size());
    EXPECT_EQ(0U, observer.finish_log.size());
    EXPECT_EQ(0U, observer.cancel_log.size());

    // Start an async resolve for (host1:70).
    net::HostResolver::RequestInfo info1("host1", 70);
    net::HostResolver::Request* req = NULL;
    net::AddressList addrlist;
    int rv = host_resolver.Resolve(info1, &addrlist, &callback, &req);
    EXPECT_EQ(net::ERR_IO_PENDING, rv);
    EXPECT_TRUE(NULL != req);

    EXPECT_EQ(1U, observer.start_log.size());
    EXPECT_EQ(0U, observer.finish_log.size());
    EXPECT_EQ(0U, observer.cancel_log.size());

    EXPECT_TRUE(observer.start_log[0] ==
                CapturingObserver::StartOrCancelEntry(0, info1));

    // Cancel the request (host mapper is blocked so it cant be finished yet).
    host_resolver.CancelRequest(req);

    EXPECT_EQ(1U, observer.start_log.size());
    EXPECT_EQ(0U, observer.finish_log.size());
    EXPECT_EQ(1U, observer.cancel_log.size());

    EXPECT_TRUE(observer.cancel_log[0] ==
                CapturingObserver::StartOrCancelEntry(0, info1));

    // Start an async request for (host2:60)
    net::HostResolver::RequestInfo info2("host2", 60);
    rv = host_resolver.Resolve(info2, &addrlist, &callback, NULL);
    EXPECT_EQ(net::ERR_IO_PENDING, rv);
    EXPECT_TRUE(NULL != req);

    EXPECT_EQ(2U, observer.start_log.size());
    EXPECT_EQ(0U, observer.finish_log.size());
    EXPECT_EQ(1U, observer.cancel_log.size());

    EXPECT_TRUE(observer.start_log[1] ==
                CapturingObserver::StartOrCancelEntry(1, info2));

    // Upon exiting this scope, HostResolver is destroyed, so all requests are
    // implicitly cancelled.
  }

  // Check that destroying the HostResolver sent a notification for
  // cancellation of host2:60 request.

  EXPECT_EQ(2U, observer.start_log.size());
  EXPECT_EQ(0U, observer.finish_log.size());
  EXPECT_EQ(2U, observer.cancel_log.size());

  net::HostResolver::RequestInfo info("host2", 60);
  EXPECT_TRUE(observer.cancel_log[1] ==
              CapturingObserver::StartOrCancelEntry(1, info));
}

}  // namespace
