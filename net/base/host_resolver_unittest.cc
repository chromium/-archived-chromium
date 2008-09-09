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

#include "net/base/address_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(HostResolverTest, NumericAddresses) {
  // Stevens says dotted quads with AI_UNSPEC resolve to a single sockaddr_in.

  net::HostResolver host_resolver;
  net::AddressList adrlist;
  const int kPortnum = 5555;
  int err = host_resolver.Resolve("127.0.0.1", kPortnum, &adrlist, NULL);
  EXPECT_EQ(0, err);

  const struct addrinfo* ainfo = adrlist.head();
  EXPECT_EQ(static_cast<addrinfo*>(NULL), ainfo->ai_next);
  EXPECT_EQ(sizeof(struct sockaddr_in), ainfo->ai_addrlen);

  const struct sockaddr* sa = ainfo->ai_addr;
  const struct sockaddr_in* sa_in = (const struct sockaddr_in*) sa;
  EXPECT_EQ(htons(kPortnum), sa_in->sin_port);
  EXPECT_EQ(htonl(0x7f000001), sa_in->sin_addr.s_addr);
}

} // namespace
