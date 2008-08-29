// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/hmac.h"
#include "testing/gtest/include/gtest/gtest.h"

static const int kKeySize = 16;
static const int kDigestSize = 20;

// Client key.
const unsigned char kClientKey[kKeySize] =
    { 0xbf, 0xf6, 0x83, 0x4b, 0x3e, 0xa3, 0x23, 0xdd,
      0x96, 0x78, 0x70, 0x8e, 0xa1, 0x9d, 0x3b, 0x40 };

// Expected HMAC result using kMessage and kClientKey.
const unsigned char kReceivedHmac[kDigestSize] =
    { 0xb9, 0x3c, 0xd6, 0xf0, 0x49, 0x47, 0xe2, 0x52,
      0x59, 0x7a, 0xbd, 0x1f, 0x2b, 0x4c, 0x83, 0xad,
      0x86, 0xd2, 0x48, 0x85 };

const char kMessage[] =
"n:1896\ni:goog-malware-shavar\nu:s.ytimg.com/safebrowsing/rd/goog-malware-shav"
"ar_s_445-450\nu:s.ytimg.com/safebrowsing/rd/goog-malware-shavar_s_439-444\nu:s"
".ytimg.com/safebrowsing/rd/goog-malware-shavar_s_437\nu:s.ytimg.com/safebrowsi"
"ng/rd/goog-malware-shavar_s_436\nu:s.ytimg.com/safebrowsing/rd/goog-malware-sh"
"avar_s_433-435\nu:s.ytimg.com/safebrowsing/rd/goog-malware-shavar_s_431\nu:s.y"
"timg.com/safebrowsing/rd/goog-malware-shavar_s_430\nu:s.ytimg.com/safebrowsing"
"/rd/goog-malware-shavar_s_429\nu:s.ytimg.com/safebrowsing/rd/goog-malware-shav"
"ar_s_428\nu:s.ytimg.com/safebrowsing/rd/goog-malware-shavar_s_426\nu:s.ytimg.c"
"om/safebrowsing/rd/goog-malware-shavar_s_424\nu:s.ytimg.com/safebrowsing/rd/go"
"og-malware-shavar_s_423\nu:s.ytimg.com/safebrowsing/rd/goog-malware-shavar_s_4"
"22\nu:s.ytimg.com/safebrowsing/rd/goog-malware-shavar_s_420\nu:s.ytimg.com/saf"
"ebrowsing/rd/goog-malware-shavar_s_419\nu:s.ytimg.com/safebrowsing/rd/goog-mal"
"ware-shavar_s_414\nu:s.ytimg.com/safebrowsing/rd/goog-malware-shavar_s_409-411"
"\nu:s.ytimg.com/safebrowsing/rd/goog-malware-shavar_s_405\nu:s.ytimg.com/safeb"
"rowsing/rd/goog-malware-shavar_s_404\nu:s.ytimg.com/safebrowsing/rd/goog-malwa"
"re-shavar_s_402\nu:s.ytimg.com/safebrowsing/rd/goog-malware-shavar_s_401\nu:s."
"ytimg.com/safebrowsing/rd/goog-malware-shavar_a_973-978\nu:s.ytimg.com/safebro"
"wsing/rd/goog-malware-shavar_a_937-972\nu:s.ytimg.com/safebrowsing/rd/goog-mal"
"ware-shavar_a_931-936\nu:s.ytimg.com/safebrowsing/rd/goog-malware-shavar_a_925"
"-930\nu:s.ytimg.com/safebrowsing/rd/goog-malware-shavar_a_919-924\ni:goog-phis"
"h-shavar\nu:s.ytimg.com/safebrowsing/rd/goog-phish-shavar_a_2633\nu:s.ytimg.co"
"m/safebrowsing/rd/goog-phish-shavar_a_2632\nu:s.ytimg.com/safebrowsing/rd/goog"
"-phish-shavar_a_2629-2631\nu:s.ytimg.com/safebrowsing/rd/goog-phish-shavar_a_2"
"626-2628\nu:s.ytimg.com/safebrowsing/rd/goog-phish-shavar_a_2625\n";

TEST(HMACTest, HmacSafeBrowsingResponseTest) {
  std::string message_data(kMessage);

  HMAC hmac(HMAC::SHA1, kClientKey, kKeySize);
  unsigned char calculated_hmac[kDigestSize];

  EXPECT_TRUE(hmac.Sign(message_data, calculated_hmac, kDigestSize));
  EXPECT_EQ(memcmp(kReceivedHmac, calculated_hmac, kDigestSize), 0);
}
