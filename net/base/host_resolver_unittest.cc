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
      : hostname_(hostname), port_(port), resolver_(resolver),
        delegate_(delegate),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            callback_(this, &ResolveRequest::OnLookupFinished)) {
    // Start the request.
    int err = resolver->Resolve(hostname, port, &addrlist_, &callback_, &req_);
    EXPECT_EQ(net::ERR_IO_PENDING, err);
  }

  void Cancel() {
    resolver_->CancelRequest(req_);
  }

  const std::string& hostname() const {
    return hostname_;
  }

  int port() const {
    return port_;
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
  std::string hostname_;
  int port_;
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

  int err = host_resolver.Resolve("just.testing", kPortnum, &adrlist, NULL,
                                  NULL);
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

  int err = host_resolver.Resolve("just.testing", kPortnum, &adrlist,
                                  &callback_, NULL);
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

    int err = host_resolver.Resolve("just.testing", kPortnum, &adrlist,
                                    &callback_, NULL);
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
  int err = host_resolver.Resolve("127.1.2.3", kPortnum, &adrlist, NULL, NULL);
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
  int err = host_resolver.Resolve("2001:db8::1", kPortnum, &adrlist, NULL,
                                  NULL);
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
  int err = host_resolver.Resolve("", kPortnum, &adrlist, NULL, NULL);
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

}  // namespace
