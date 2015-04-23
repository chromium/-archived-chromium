// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_list.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test parsing from a PAC string.
TEST(ProxyListTest, SetFromPacString) {
  const struct {
    const char* pac_input;
    const char* pac_output;
  } tests[] = {
    // Valid inputs:
    {  "PROXY foopy:10",
       "PROXY foopy:10",
    },
    {  " DIRECT",  // leading space.
       "DIRECT",
    },
    {  "PROXY foopy1 ; proxy foopy2;\t DIRECT",
       "PROXY foopy1:80;PROXY foopy2:80;DIRECT",
    },
    {  "proxy foopy1 ; SOCKS foopy2",
       "PROXY foopy1:80;SOCKS foopy2:1080",
    },

    // Invalid inputs (parts which aren't understood get
    // silently discarded):
    {  "PROXY-foopy:10",
       "DIRECT",
    },
    {  "PROXY",
       "DIRECT",
    },
    {  "PROXY foopy1 ; JUNK ; JUNK ; SOCKS5 foopy2 ; ;",
       "PROXY foopy1:80;SOCKS5 foopy2:1080",
    },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    net::ProxyList list;
    list.SetFromPacString(tests[i].pac_input);
    EXPECT_EQ(tests[i].pac_output, list.ToPacString());
  }
}

TEST(ProxyListTest, RemoveProxiesWithoutScheme) {
  const struct {
    const char* pac_input;
    int filter;
    const char* filtered_pac_output;
  } tests[] = {
    {  "PROXY foopy:10 ; SOCKS5 foopy2 ; SOCKS foopy11 ; PROXY foopy3 ; DIRECT",
       // Remove anything that isn't HTTP or DIRECT.
       net::ProxyServer::SCHEME_DIRECT | net::ProxyServer::SCHEME_HTTP,
       "PROXY foopy:10;PROXY foopy3:80;DIRECT",
    },
    {  "PROXY foopy:10 | SOCKS5 foopy2",
       // Remove anything that isn't HTTP or SOCKS5.
       net::ProxyServer::SCHEME_DIRECT | net::ProxyServer::SCHEME_SOCKS4,
       "DIRECT",
    },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    net::ProxyList list;
    list.SetFromPacString(tests[i].pac_input);
    list.RemoveProxiesWithoutScheme(tests[i].filter);
    EXPECT_EQ(tests[i].filtered_pac_output, list.ToPacString());
  }
}
