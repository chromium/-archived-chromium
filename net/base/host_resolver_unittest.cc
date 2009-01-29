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

namespace {

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

  int err = host_resolver.Resolve("just.testing", kPortnum, &adrlist, NULL);
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
                                  &callback_);
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
                                    &callback_);
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

TEST_F(HostResolverTest, NumericAddresses) {
  // Stevens says dotted quads with AI_UNSPEC resolve to a single sockaddr_in.

  net::HostResolver host_resolver;
  net::AddressList adrlist;
  const int kPortnum = 5555;
  int err = host_resolver.Resolve("127.0.0.1", kPortnum, &adrlist, NULL);
  EXPECT_EQ(net::OK, err);

  const struct addrinfo* ainfo = adrlist.head();
  EXPECT_EQ(static_cast<addrinfo*>(NULL), ainfo->ai_next);
  EXPECT_EQ(sizeof(struct sockaddr_in), ainfo->ai_addrlen);

  const struct sockaddr* sa = ainfo->ai_addr;
  const struct sockaddr_in* sa_in = (const struct sockaddr_in*) sa;
  EXPECT_TRUE(htons(kPortnum) == sa_in->sin_port);
  EXPECT_TRUE(htonl(0x7f000001) == sa_in->sin_addr.s_addr);
}

} // namespace
