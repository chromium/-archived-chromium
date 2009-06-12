// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/address_list.h"

#if defined(OS_WIN)
#include <ws2tcpip.h>
#include <wspiapi.h>  // Needed for Win2k compat.
#elif defined(OS_POSIX)
#include <netdb.h>
#include <sys/socket.h>
#endif

#include "base/string_util.h"
#include "net/base/net_util.h"
#if defined(OS_WIN)
#include "net/base/winsock_init.h"
#endif
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Use getaddrinfo() to allocate an addrinfo structure.
void CreateAddressList(net::AddressList* addrlist, int port) {
#if defined(OS_WIN)
  net::EnsureWinsockInit();
#endif
  std::string portstr = IntToString(port);

  struct addrinfo* result = NULL;
  struct addrinfo hints = {0};
  hints.ai_family = AF_UNSPEC;
  hints.ai_flags = AI_NUMERICHOST;
  hints.ai_socktype = SOCK_STREAM;

  int err = getaddrinfo("192.168.1.1", portstr.c_str(), &hints, &result);
  EXPECT_EQ(0, err);
  addrlist->Adopt(result);
}

TEST(AddressListTest, GetPort) {
  net::AddressList addrlist;
  CreateAddressList(&addrlist, 81);
  EXPECT_EQ(81, addrlist.GetPort());

  addrlist.SetPort(83);
  EXPECT_EQ(83, addrlist.GetPort());
}

TEST(AddressListTest, Assignment) {
  net::AddressList addrlist1;
  CreateAddressList(&addrlist1, 85);
  EXPECT_EQ(85, addrlist1.GetPort());

  // Should reference the same data as addrlist1 -- so when we change addrlist1
  // both are changed.
  net::AddressList addrlist2 = addrlist1;
  EXPECT_EQ(85, addrlist2.GetPort());

  addrlist1.SetPort(80);
  EXPECT_EQ(80, addrlist1.GetPort());
  EXPECT_EQ(80, addrlist2.GetPort());
}

TEST(AddressListTest, Copy) {
  net::AddressList addrlist1;
  CreateAddressList(&addrlist1, 85);
  EXPECT_EQ(85, addrlist1.GetPort());

  net::AddressList addrlist2;
  addrlist2.Copy(addrlist1.head());

  // addrlist1 is the same as addrlist2 at this point.
  EXPECT_EQ(85, addrlist1.GetPort());
  EXPECT_EQ(85, addrlist2.GetPort());

  // Changes to addrlist1 are not reflected in addrlist2.
  addrlist1.SetPort(70);
  addrlist2.SetPort(90);

  EXPECT_EQ(70, addrlist1.GetPort());
  EXPECT_EQ(90, addrlist2.GetPort());
}

}  // namespace
