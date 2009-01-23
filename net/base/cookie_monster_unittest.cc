// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>

#include <string>

#include "base/basictypes.h"
#include "base/platform_thread.h"
#include "base/string_util.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "net/base/cookie_monster.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

namespace {
  class ParsedCookieTest : public testing::Test { };
  class CookieMonsterTest : public testing::Test { };
}


TEST(ParsedCookieTest, TestBasic) {
  net::CookieMonster::ParsedCookie pc("a=b");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_FALSE(pc.IsSecure());
  EXPECT_EQ("a", pc.Name());
  EXPECT_EQ("b", pc.Value());
}

TEST(ParsedCookieTest, TestQuoted) {
  // These are some quoting cases which the major browsers all
  // handle differently.  I've tested Internet Explorer 6, Opera 9.6,
  // Firefox 3, and Safari Windows 3.2.1.  We originally tried to match
  // Firefox closely, however we now match Internet Explorer and Safari.
  const char* values[] = {
    // Trailing whitespace after a quoted value.  The whitespace after
    // the quote is stripped in all browsers.
    "\"zzz \"  ",              "\"zzz \"",
    // Handling a quoted value with a ';', like FOO="zz;pp"  ;
    // IE and Safari: "zz;
    // Firefox and Opera: "zz;pp"
    "\"zz;pp\" ;",             "\"zz",
    // Handling a value with multiple quoted parts, like FOO="zzz "   "ppp" ;
    // IE and Safari: "zzz "   "ppp";
    // Firefox: "zzz ";
    // Opera: <rejects cookie>
    "\"zzz \"   \"ppp\" ",     "\"zzz \"   \"ppp\"",
    // A quote in a value that didn't start quoted.  like FOO=A"B ;
    // IE, Safari, and Firefox: A"B;
    // Opera: <rejects cookie>
    "A\"B",                    "A\"B",
  };

  for (size_t i = 0; i < arraysize(values); i += 2) {
    std::string input(values[i]);
    std::string expected(values[i + 1]);

    net::CookieMonster::ParsedCookie pc(
        "aBc=" + input + " ; path=\"/\"  ; httponly ");
    EXPECT_TRUE(pc.IsValid());
    EXPECT_FALSE(pc.IsSecure());
    EXPECT_TRUE(pc.IsHttpOnly());
    EXPECT_TRUE(pc.HasPath());
    EXPECT_EQ("aBc", pc.Name());
    EXPECT_EQ(expected, pc.Value());

    // If a path was quoted, the path attribute keeps the quotes.  This will
    // make the cookie effectively useless, but path parameters aren't supposed
    // to be quoted.  Bug 1261605.
    EXPECT_EQ("\"/\"", pc.Path());
  }
}

TEST(ParsedCookieTest, TestNameless) {
  net::CookieMonster::ParsedCookie pc("BLAHHH; path=/; secure;");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/", pc.Path());
  EXPECT_EQ("", pc.Name());
  EXPECT_EQ("BLAHHH", pc.Value());
}

TEST(ParsedCookieTest, TestAttributeCase) {
  net::CookieMonster::ParsedCookie pc("BLAHHH; Path=/; sECuRe; httpONLY");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_TRUE(pc.IsHttpOnly());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/", pc.Path());
  EXPECT_EQ("", pc.Name());
  EXPECT_EQ("BLAHHH", pc.Value());
  EXPECT_EQ(3U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, TestDoubleQuotedNameless) {
  net::CookieMonster::ParsedCookie pc("\"BLA\\\"HHH\"; path=/; secure;");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/", pc.Path());
  EXPECT_EQ("", pc.Name());
  EXPECT_EQ("\"BLA\\\"HHH\"", pc.Value());
  EXPECT_EQ(2U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, QuoteOffTheEnd) {
  net::CookieMonster::ParsedCookie pc("a=\"B");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("a", pc.Name());
  EXPECT_EQ("\"B", pc.Value());
  EXPECT_EQ(0U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, MissingName) {
  net::CookieMonster::ParsedCookie pc("=ABC");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("", pc.Name());
  EXPECT_EQ("ABC", pc.Value());
  EXPECT_EQ(0U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, MissingValue) {
  net::CookieMonster::ParsedCookie pc("ABC=;  path = /wee");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("ABC", pc.Name());
  EXPECT_EQ("", pc.Value());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/wee", pc.Path());
  EXPECT_EQ(1U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, Whitespace) {
  net::CookieMonster::ParsedCookie pc("  A  = BC  ;secure;;;   httponly");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("A", pc.Name());
  EXPECT_EQ("BC", pc.Value());
  EXPECT_FALSE(pc.HasPath());
  EXPECT_FALSE(pc.HasDomain());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_TRUE(pc.IsHttpOnly());
  // We parse anything between ; as attributes, so we end up with two
  // attributes with an empty string name and value.
  EXPECT_EQ(4U, pc.NumberOfAttributes());
}
TEST(ParsedCookieTest, MultipleEquals) {
  net::CookieMonster::ParsedCookie pc("  A=== BC  ;secure;;;   httponly");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("A", pc.Name());
  EXPECT_EQ("== BC", pc.Value());
  EXPECT_FALSE(pc.HasPath());
  EXPECT_FALSE(pc.HasDomain());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_TRUE(pc.IsHttpOnly());
  EXPECT_EQ(4U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, QuotedTrailingWhitespace) {
  net::CookieMonster::ParsedCookie pc("ANCUUID=\"zohNumRKgI0oxyhSsV3Z7D\"  ; "
                                      "expires=Sun, 18-Apr-2027 21:06:29 GMT ; "
                                      "path=/  ;  ");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("ANCUUID", pc.Name());
  // Stripping whitespace after the quotes matches all other major browsers.
  EXPECT_EQ("\"zohNumRKgI0oxyhSsV3Z7D\"", pc.Value());
  EXPECT_TRUE(pc.HasExpires());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/", pc.Path());
  EXPECT_EQ(2U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, TrailingWhitespace) {
  net::CookieMonster::ParsedCookie pc("ANCUUID=zohNumRKgI0oxyhSsV3Z7D  ; "
                                      "expires=Sun, 18-Apr-2027 21:06:29 GMT ; "
                                      "path=/  ;  ");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("ANCUUID", pc.Name());
  EXPECT_EQ("zohNumRKgI0oxyhSsV3Z7D", pc.Value());
  EXPECT_TRUE(pc.HasExpires());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/", pc.Path());
  EXPECT_EQ(2U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, TooManyPairs) {
  std::string blankpairs;
  blankpairs.resize(net::CookieMonster::ParsedCookie::kMaxPairs - 1, ';');

  net::CookieMonster::ParsedCookie pc1(blankpairs + "secure");
  EXPECT_TRUE(pc1.IsValid());
  EXPECT_TRUE(pc1.IsSecure());

  net::CookieMonster::ParsedCookie pc2(blankpairs + ";secure");
  EXPECT_TRUE(pc2.IsValid());
  EXPECT_FALSE(pc2.IsSecure());
}

// TODO some better test cases for invalid cookies.
TEST(ParsedCookieTest, InvalidWhitespace) {
  net::CookieMonster::ParsedCookie pc("    ");
  EXPECT_FALSE(pc.IsValid());
}

TEST(ParsedCookieTest, InvalidTooLong) {
  std::string maxstr;
  maxstr.resize(net::CookieMonster::ParsedCookie::kMaxCookieSize, 'a');

  net::CookieMonster::ParsedCookie pc1(maxstr);
  EXPECT_TRUE(pc1.IsValid());

  net::CookieMonster::ParsedCookie pc2(maxstr + "A");
  EXPECT_FALSE(pc2.IsValid());
}

TEST(ParsedCookieTest, InvalidEmpty) {
  net::CookieMonster::ParsedCookie pc("");
  EXPECT_FALSE(pc.IsValid());
}

TEST(ParsedCookieTest, EmbeddedTerminator) {
  net::CookieMonster::ParsedCookie pc1("AAA=BB\0ZYX");
  net::CookieMonster::ParsedCookie pc2("AAA=BB\rZYX");
  net::CookieMonster::ParsedCookie pc3("AAA=BB\nZYX");
  EXPECT_TRUE(pc1.IsValid());
  EXPECT_EQ("AAA", pc1.Name());
  EXPECT_EQ("BB", pc1.Value());
  EXPECT_TRUE(pc2.IsValid());
  EXPECT_EQ("AAA", pc2.Name());
  EXPECT_EQ("BB", pc2.Value());
  EXPECT_TRUE(pc3.IsValid());
  EXPECT_EQ("AAA", pc3.Name());
  EXPECT_EQ("BB", pc3.Value());
}

static const char kUrlGoogle[] = "http://www.google.izzle";
static const char kUrlGoogleSecure[] = "https://www.google.izzle";
static const char kUrlFtp[] = "ftp://ftp.google.izzle/";
static const char kValidCookieLine[] = "A=B; path=/";
static const char kValidDomainCookieLine[] = "A=B; path=/; domain=google.izzle";

TEST(CookieMonsterTest, DomainTest) {
  GURL url_google(kUrlGoogle);

  net::CookieMonster cm;
  EXPECT_TRUE(cm.SetCookie(url_google, "A=B"));
  EXPECT_EQ("A=B", cm.GetCookies(url_google));
  EXPECT_TRUE(cm.SetCookie(url_google, "C=D; domain=.google.izzle"));
  EXPECT_EQ("A=B; C=D", cm.GetCookies(url_google));

  // Verify that A=B was set as a host cookie rather than a domain
  // cookie -- should not be accessible from a sub sub-domain.
  EXPECT_EQ("C=D", cm.GetCookies(GURL("http://foo.www.google.izzle")));

  // Test and make sure we find domain cookies on the same domain.
  EXPECT_TRUE(cm.SetCookie(url_google, "E=F; domain=.www.google.izzle"));
  EXPECT_EQ("A=B; C=D; E=F", cm.GetCookies(url_google));

  // Test setting a domain= that doesn't start w/ a dot, should
  // treat it as a domain cookie, as if there was a pre-pended dot.
  EXPECT_TRUE(cm.SetCookie(url_google, "G=H; domain=www.google.izzle"));
  EXPECT_EQ("A=B; C=D; E=F; G=H", cm.GetCookies(url_google));

  // Test domain enforcement, should fail on a sub-domain or something too deep.
  EXPECT_FALSE(cm.SetCookie(url_google, "I=J; domain=.izzle"));
  EXPECT_EQ("", cm.GetCookies(GURL("http://a.izzle")));
  EXPECT_FALSE(cm.SetCookie(url_google, "K=L; domain=.bla.www.google.izzle"));
  EXPECT_EQ("C=D; E=F; G=H",
            cm.GetCookies(GURL("http://bla.www.google.izzle")));
  EXPECT_EQ("A=B; C=D; E=F; G=H", cm.GetCookies(url_google));
}

// FireFox recognizes domains containing trailing periods as valid.
// IE and Safari do not. Assert the expected policy here.
TEST(CookieMonsterTest, DomainWithTrailingDotTest) {
  net::CookieMonster cm;
  GURL url_google("http://www.google.com");

  EXPECT_FALSE(cm.SetCookie(url_google, "a=1; domain=.www.google.com."));
  EXPECT_FALSE(cm.SetCookie(url_google, "b=2; domain=.www.google.com.."));
  EXPECT_EQ("", cm.GetCookies(url_google));
}

// Test that cookies can bet set on higher level domains.
// http://b/issue?id=896491
TEST(CookieMonsterTest, ValidSubdomainTest) {
  net::CookieMonster cm;
  GURL url_abcd("http://a.b.c.d.com");
  GURL url_bcd("http://b.c.d.com");
  GURL url_cd("http://c.d.com");
  GURL url_d("http://d.com");

  EXPECT_TRUE(cm.SetCookie(url_abcd, "a=1; domain=.a.b.c.d.com"));
  EXPECT_TRUE(cm.SetCookie(url_abcd, "b=2; domain=.b.c.d.com"));
  EXPECT_TRUE(cm.SetCookie(url_abcd, "c=3; domain=.c.d.com"));
  EXPECT_TRUE(cm.SetCookie(url_abcd, "d=4; domain=.d.com"));

  EXPECT_EQ("a=1; b=2; c=3; d=4", cm.GetCookies(url_abcd));
  EXPECT_EQ("b=2; c=3; d=4", cm.GetCookies(url_bcd));
  EXPECT_EQ("c=3; d=4", cm.GetCookies(url_cd));
  EXPECT_EQ("d=4", cm.GetCookies(url_d));

  // Check that the same cookie can exist on different sub-domains.
  EXPECT_TRUE(cm.SetCookie(url_bcd, "X=bcd; domain=.b.c.d.com"));
  EXPECT_TRUE(cm.SetCookie(url_bcd, "X=cd; domain=.c.d.com"));
  EXPECT_EQ("b=2; c=3; d=4; X=bcd; X=cd", cm.GetCookies(url_bcd));
  EXPECT_EQ("c=3; d=4; X=cd", cm.GetCookies(url_cd));
}

// Test that setting a cookie which specifies an invalid domain has
// no side-effect. An invalid domain in this context is one which does
// not match the originating domain.
// http://b/issue?id=896472
TEST(CookieMonsterTest, InvalidDomainTest) {
  {
    net::CookieMonster cm;
    GURL url_foobar("http://foo.bar.com");

    // More specific sub-domain than allowed.
    EXPECT_FALSE(cm.SetCookie(url_foobar, "a=1; domain=.yo.foo.bar.com"));

    EXPECT_FALSE(cm.SetCookie(url_foobar, "b=2; domain=.foo.com"));
    EXPECT_FALSE(cm.SetCookie(url_foobar, "c=3; domain=.bar.foo.com"));

    // Different TLD, but the rest is a substring.
    EXPECT_FALSE(cm.SetCookie(url_foobar, "d=4; domain=.foo.bar.com.net"));

    // A substring that isn't really a parent domain.
    EXPECT_FALSE(cm.SetCookie(url_foobar, "e=5; domain=ar.com"));

    // Completely invalid domains:
    EXPECT_FALSE(cm.SetCookie(url_foobar, "f=6; domain=."));
    EXPECT_FALSE(cm.SetCookie(url_foobar, "g=7; domain=/"));
    EXPECT_FALSE(cm.SetCookie(url_foobar, "h=8; domain=http://foo.bar.com"));
    EXPECT_FALSE(cm.SetCookie(url_foobar, "i=9; domain=..foo.bar.com"));
    EXPECT_FALSE(cm.SetCookie(url_foobar, "j=10; domain=..bar.com"));

    // Make sure there isn't something quirky in the domain canonicalization
    // that supports full URL semantics.
    EXPECT_FALSE(cm.SetCookie(url_foobar, "k=11; domain=.foo.bar.com?blah"));
    EXPECT_FALSE(cm.SetCookie(url_foobar, "l=12; domain=.foo.bar.com/blah"));
    EXPECT_FALSE(cm.SetCookie(url_foobar, "m=13; domain=.foo.bar.com:80"));
    EXPECT_FALSE(cm.SetCookie(url_foobar, "n=14; domain=.foo.bar.com:"));
    EXPECT_FALSE(cm.SetCookie(url_foobar, "o=15; domain=.foo.bar.com#sup"));

    EXPECT_EQ("", cm.GetCookies(url_foobar));
  }

  {
    // Make sure the cookie code hasn't gotten its subdomain string handling
    // reversed, missed a suffix check, etc.  It's important here that the two
    // hosts below have the same domain + registry.
    net::CookieMonster cm;
    GURL url_foocom("http://foo.com.com");
    EXPECT_FALSE(cm.SetCookie(url_foocom, "a=1; domain=.foo.com.com.com"));
    EXPECT_EQ("", cm.GetCookies(url_foocom));
  }
}

// Test the behavior of omitting dot prefix from domain, should
// function the same as FireFox.
// http://b/issue?id=889898
TEST(CookieMonsterTest, DomainWithoutLeadingDotTest) {
  {  // The omission of dot results in setting a domain cookie.
    net::CookieMonster cm;
    GURL url_hosted("http://manage.hosted.filefront.com");
    GURL url_filefront("http://www.filefront.com");
    EXPECT_TRUE(cm.SetCookie(url_hosted, "sawAd=1; domain=filefront.com"));
    EXPECT_EQ("sawAd=1", cm.GetCookies(url_hosted));
    EXPECT_EQ("sawAd=1", cm.GetCookies(url_filefront));
  }

  {  // Even when the domains match exactly, don't consider it host cookie.
    net::CookieMonster cm;
    GURL url("http://www.google.com");
    EXPECT_TRUE(cm.SetCookie(url, "a=1; domain=www.google.com"));
    EXPECT_EQ("a=1", cm.GetCookies(url));
    EXPECT_EQ("a=1", cm.GetCookies(GURL("http://sub.www.google.com")));
    EXPECT_EQ("", cm.GetCookies(GURL("http://something-else.com")));
  }
}

// Test that the domain specified in cookie string is treated case-insensitive
// http://b/issue?id=896475.
TEST(CookieMonsterTest, CaseInsensitiveDomainTest) {
  net::CookieMonster cm;
  GURL url_google("http://www.google.com");
  EXPECT_TRUE(cm.SetCookie(url_google, "a=1; domain=.GOOGLE.COM"));
  EXPECT_TRUE(cm.SetCookie(url_google, "b=2; domain=.wWw.gOOgLE.coM"));
  EXPECT_EQ("a=1; b=2", cm.GetCookies(url_google));
}

TEST(CookieMonsterTest, TestIpAddress) {
  GURL url_ip("http://1.2.3.4/weee");
  {
    net::CookieMonster cm;
    EXPECT_TRUE(cm.SetCookie(url_ip, kValidCookieLine));
    EXPECT_EQ("A=B", cm.GetCookies(url_ip));
  }

  {  // IP addresses should not be able to set domain cookies.
    net::CookieMonster cm;
    EXPECT_FALSE(cm.SetCookie(url_ip, "b=2; domain=.1.2.3.4"));
    EXPECT_FALSE(cm.SetCookie(url_ip, "c=3; domain=.3.4"));
    EXPECT_EQ("", cm.GetCookies(url_ip));
    // It should be allowed to set a cookie if domain= matches the IP address
    // exactly.  This matches IE/Firefox, even though it seems a bit wrong.
    EXPECT_FALSE(cm.SetCookie(url_ip, "b=2; domain=1.2.3.3"));
    EXPECT_EQ("", cm.GetCookies(url_ip));
    EXPECT_TRUE(cm.SetCookie(url_ip, "b=2; domain=1.2.3.4"));
    EXPECT_EQ("b=2", cm.GetCookies(url_ip));
  }
}

// Test host cookies, and setting of cookies on TLD.
TEST(CookieMonsterTest, TestNonDottedAndTLD) {
  {
    net::CookieMonster cm;
    GURL url("http://com/");
    // Allow setting on "com", (but only as a host cookie).
    EXPECT_TRUE(cm.SetCookie(url, "a=1"));
    EXPECT_FALSE(cm.SetCookie(url, "b=2; domain=.com"));
    EXPECT_FALSE(cm.SetCookie(url, "c=3; domain=com"));
    EXPECT_EQ("a=1", cm.GetCookies(url));
    // Make sure it doesn't show up for a normal .com, it should be a host
    // not a domain cookie.
    EXPECT_EQ("", cm.GetCookies(GURL("http://hopefully-no-cookies.com/")));
    EXPECT_EQ("", cm.GetCookies(GURL("http://.com/")));
  }

  {  // http://com. should be treated the same as http://com.
    net::CookieMonster cm;
    GURL url("http://com./index.html");
    EXPECT_TRUE(cm.SetCookie(url, "a=1"));
    EXPECT_EQ("a=1", cm.GetCookies(url));
    EXPECT_EQ("", cm.GetCookies(GURL("http://hopefully-no-cookies.com./")));
  }

  {  // Should not be able to set host cookie from a subdomain.
    net::CookieMonster cm;
    GURL url("http://a.b");
    EXPECT_FALSE(cm.SetCookie(url, "a=1; domain=.b"));
    EXPECT_FALSE(cm.SetCookie(url, "b=2; domain=b"));
    EXPECT_EQ("", cm.GetCookies(url));
  }

  {  // Same test as above, but explicitly on a known TLD (com).
    net::CookieMonster cm;
    GURL url("http://google.com");
    EXPECT_FALSE(cm.SetCookie(url, "a=1; domain=.com"));
    EXPECT_FALSE(cm.SetCookie(url, "b=2; domain=com"));
    EXPECT_EQ("", cm.GetCookies(url));
  }

  {  // Make sure can't set cookie on TLD which is dotted.
    net::CookieMonster cm;
    GURL url("http://google.co.uk");
    EXPECT_FALSE(cm.SetCookie(url, "a=1; domain=.co.uk"));
    EXPECT_FALSE(cm.SetCookie(url, "b=2; domain=.uk"));
    EXPECT_EQ("", cm.GetCookies(url));
    EXPECT_EQ("", cm.GetCookies(GURL("http://something-else.co.uk")));
    EXPECT_EQ("", cm.GetCookies(GURL("http://something-else.uk")));
  }

  {  // Intranet URLs should only be able to set host cookies.
    net::CookieMonster cm;
    GURL url("http://b");
    EXPECT_TRUE(cm.SetCookie(url, "a=1"));
    EXPECT_FALSE(cm.SetCookie(url, "b=2; domain=.b"));
    EXPECT_FALSE(cm.SetCookie(url, "c=3; domain=b"));
    EXPECT_EQ("a=1", cm.GetCookies(url));
  }
}

// Test reading/writing cookies when the domain ends with a period,
// as in "www.google.com."
TEST(CookieMonsterTest, TestHostEndsWithDot) {
  net::CookieMonster cm;
  GURL url("http://www.google.com");
  GURL url_with_dot("http://www.google.com.");
  EXPECT_TRUE(cm.SetCookie(url, "a=1"));
  EXPECT_EQ("a=1", cm.GetCookies(url));

  // Do not share cookie space with the dot version of domain.
  // Note: this is not what FireFox does, but it _is_ what IE+Safari do.
  EXPECT_FALSE(cm.SetCookie(url, "b=2; domain=.www.google.com."));
  EXPECT_EQ("a=1", cm.GetCookies(url));

  EXPECT_TRUE(cm.SetCookie(url_with_dot, "b=2; domain=.google.com."));
  EXPECT_EQ("b=2", cm.GetCookies(url_with_dot));

  // Make sure there weren't any side effects.
  EXPECT_EQ(cm.GetCookies(GURL("http://hopefully-no-cookies.com/")), "");
  EXPECT_EQ("", cm.GetCookies(GURL("http://.com/")));
}

TEST(CookieMonsterTest, InvalidScheme) {
  net::CookieMonster cm;
  EXPECT_FALSE(cm.SetCookie(GURL(kUrlFtp), kValidCookieLine));
}

TEST(CookieMonsterTest, InvalidScheme_Read) {
  net::CookieMonster cm;
  EXPECT_TRUE(cm.SetCookie(GURL(kUrlGoogle), kValidDomainCookieLine));
  EXPECT_EQ("", cm.GetCookies(GURL(kUrlFtp)));
}

TEST(CookieMonsterTest, PathTest) {
  std::string url("http://www.google.izzle");
  net::CookieMonster cm;
  EXPECT_TRUE(cm.SetCookie(GURL(url), "A=B; path=/wee"));
  EXPECT_EQ("A=B", cm.GetCookies(GURL(url + "/wee")));
  EXPECT_EQ("A=B", cm.GetCookies(GURL(url + "/wee/")));
  EXPECT_EQ("A=B", cm.GetCookies(GURL(url + "/wee/war")));
  EXPECT_EQ("A=B", cm.GetCookies(GURL(url + "/wee/war/more/more")));
  EXPECT_EQ("", cm.GetCookies(GURL(url + "/weehee")));
  EXPECT_EQ("", cm.GetCookies(GURL(url + "/")));

  // If we add a 0 length path, it should default to /
  EXPECT_TRUE(cm.SetCookie(GURL(url), "A=C; path="));
  EXPECT_EQ("A=B; A=C", cm.GetCookies(GURL(url + "/wee")));
  EXPECT_EQ("A=C", cm.GetCookies(GURL(url + "/")));
}

TEST(CookieMonsterTest, HttpOnlyTest) {
  GURL url_google(kUrlGoogle);
  net::CookieMonster cm;
  net::CookieMonster::CookieOptions options;
  options.set_include_httponly();

  // Create a httponly cookie.
  EXPECT_TRUE(cm.SetCookieWithOptions(url_google, "A=B; httponly", options));
  
  // Check httponly read protection.
  EXPECT_EQ("", cm.GetCookies(url_google));
  EXPECT_EQ("A=B", cm.GetCookiesWithOptions(url_google, options));

  // Check httponly overwrite protection.
  EXPECT_FALSE(cm.SetCookie(url_google, "A=C"));
  EXPECT_EQ("", cm.GetCookies(url_google));
  EXPECT_EQ("A=B", cm.GetCookiesWithOptions(url_google, options));
  EXPECT_TRUE(cm.SetCookieWithOptions(url_google, "A=C", options));
  EXPECT_EQ("A=C", cm.GetCookies(url_google));

  // Check httponly create protection.
  EXPECT_FALSE(cm.SetCookie(url_google, "B=A; httponly"));
  EXPECT_EQ("A=C", cm.GetCookiesWithOptions(url_google, options));
  EXPECT_TRUE(cm.SetCookieWithOptions(url_google, "B=A; httponly", options));
  EXPECT_EQ("A=C; B=A", cm.GetCookiesWithOptions(url_google, options));
  EXPECT_EQ("A=C", cm.GetCookies(url_google));
}

namespace {

struct CookieDateParsingCase {
  const char* str;
  const bool valid;
  const time_t epoch;
};

}  // namespace

TEST(CookieMonsterTest, TestCookieDateParsing) {
  const CookieDateParsingCase tests[] = {
    { "Sat, 15-Apr-17 21:01:22 GMT",           true, 1492290082 },
    { "Thu, 19-Apr-2007 16:00:00 GMT",         true, 1176998400 },
    { "Wed, 25 Apr 2007 21:02:13 GMT",         true, 1177534933 },
    { "Thu, 19/Apr\\2007 16:00:00 GMT",        true, 1176998400 },
    { "Fri, 1 Jan 2010 01:01:50 GMT",          true, 1262307710 },
    { "Wednesday, 1-Jan-2003 00:00:00 GMT",    true, 1041379200 },
    { ", 1-Jan-2003 00:00:00 GMT",             true, 1041379200 },
    { " 1-Jan-2003 00:00:00 GMT",              true, 1041379200 },
    { "1-Jan-2003 00:00:00 GMT",               true, 1041379200 },
    { "Wed,18-Apr-07 22:50:12 GMT",            true, 1176936612 },
    { "WillyWonka  , 18-Apr-07 22:50:12 GMT",  true, 1176936612 },
    { "WillyWonka  , 18-Apr-07 22:50:12",      true, 1176936612 },
    { "WillyWonka  ,  18-apr-07   22:50:12",   true, 1176936612 },
    { "Mon, 18-Apr-1977 22:50:13 GMT",         true, 230251813 },
    { "Mon, 18-Apr-77 22:50:13 GMT",           true, 230251813 },
    // If the cookie came in with the expiration quoted (which in terms of
    // the RFC you shouldn't do), we will get string quoted.  Bug 1261605.
    { "\"Sat, 15-Apr-17\\\"21:01:22\\\"GMT\"", true, 1492290082 },
    // Test with full month names and partial names.
    { "Partyday, 18- April-07 22:50:12",       true, 1176936612 },
    { "Partyday, 18 - Apri-07 22:50:12",       true, 1176936612 },
    { "Wednes, 1-Januar-2003 00:00:00 GMT",    true, 1041379200 },
    // Test that we always take GMT even with other time zones or bogus
    // values.  The RFC says everything should be GMT, and in the worst case
    // we are 24 hours off because of zone issues.
    { "Sat, 15-Apr-17 21:01:22",               true, 1492290082 },
    { "Sat, 15-Apr-17 21:01:22 GMT-2",         true, 1492290082 },
    { "Sat, 15-Apr-17 21:01:22 GMT BLAH",      true, 1492290082 },
    { "Sat, 15-Apr-17 21:01:22 GMT-0400",      true, 1492290082 },
    { "Sat, 15-Apr-17 21:01:22 GMT-0400 (EDT)",true, 1492290082 },
    { "Sat, 15-Apr-17 21:01:22 DST",           true, 1492290082 },
    { "Sat, 15-Apr-17 21:01:22 -0400",         true, 1492290082 },
    { "Sat, 15-Apr-17 21:01:22 (hello there)", true, 1492290082 },
    // Test that if we encounter multiple : fields, that we take the first
    // that correctly parses.
    { "Sat, 15-Apr-17 21:01:22 11:22:33",      true, 1492290082 },
    { "Sat, 15-Apr-17 ::00 21:01:22",          true, 1492290082 },
    { "Sat, 15-Apr-17 boink:z 21:01:22",       true, 1492290082 },
    // We take the first, which in this case is invalid.
    { "Sat, 15-Apr-17 91:22:33 21:01:22",      false, 0 },
    // amazon.com formats their cookie expiration like this.
    { "Thu Apr 18 22:50:12 2007 GMT",          true, 1176936612 },
    // Test that hh:mm:ss can occur anywhere.
    { "22:50:12 Thu Apr 18 2007 GMT",          true, 1176936612 },
    { "Thu 22:50:12 Apr 18 2007 GMT",          true, 1176936612 },
    { "Thu Apr 22:50:12 18 2007 GMT",          true, 1176936612 },
    { "Thu Apr 18 22:50:12 2007 GMT",          true, 1176936612 },
    { "Thu Apr 18 2007 22:50:12 GMT",          true, 1176936612 },
    { "Thu Apr 18 2007 GMT 22:50:12",          true, 1176936612 },
    // Test that the day and year can be anywhere if they are unambigious.
    { "Sat, 15-Apr-17 21:01:22 GMT",           true, 1492290082 },
    { "15-Sat, Apr-17 21:01:22 GMT",           true, 1492290082 },
    { "15-Sat, Apr 21:01:22 GMT 17",           true, 1492290082 },
    { "15-Sat, Apr 21:01:22 GMT 2017",         true, 1492290082 },
    { "15 Apr 21:01:22 2017",                  true, 1492290082 },
    { "15 17 Apr 21:01:22",                    true, 1492290082 },
    { "Apr 15 17 21:01:22",                    true, 1492290082 },
    { "Apr 15 21:01:22 17",                    true, 1492290082 },
    { "2017 April 15 21:01:22",                true, 1492290082 },
    { "15 April 2017 21:01:22",                true, 1492290082 },
    // Some invalid dates
    { "98 April 17 21:01:22",                    false, 0 },
    { "Thu, 012-Aug-2008 20:49:07 GMT",          false, 0 },
    { "Thu, 12-Aug-31841 20:49:07 GMT",          false, 0 },
    { "Thu, 12-Aug-9999999999 20:49:07 GMT",     false, 0 },
    { "Thu, 999999999999-Aug-2007 20:49:07 GMT", false, 0 },
    { "Thu, 12-Aug-2007 20:61:99999999999 GMT",  false, 0 },
    { "IAintNoDateFool",                         false, 0 },
  };

  Time parsed_time;
  for (size_t i = 0; i < arraysize(tests); ++i) {
    parsed_time = net::CookieMonster::ParseCookieTime(tests[i].str);
    if (!tests[i].valid) {
      EXPECT_FALSE(!parsed_time.is_null()) << tests[i].str;
      continue;
    }
    EXPECT_TRUE(!parsed_time.is_null()) << tests[i].str;
    EXPECT_EQ(tests[i].epoch, parsed_time.ToTimeT()) << tests[i].str;
  }
}

TEST(CookieMonsterTest, TestCookieDeletion) {
  GURL url_google(kUrlGoogle);
  net::CookieMonster cm;

  // Create a session cookie.
  EXPECT_TRUE(cm.SetCookie(url_google, kValidCookieLine));
  EXPECT_EQ("A=B", cm.GetCookies(url_google));
  // Delete it via Max-Age.
  EXPECT_TRUE(cm.SetCookie(url_google,
                           std::string(kValidCookieLine) + "; max-age=0"));
  EXPECT_EQ("", cm.GetCookies(url_google));

  // Create a session cookie.
  EXPECT_TRUE(cm.SetCookie(url_google, kValidCookieLine));
  EXPECT_EQ("A=B", cm.GetCookies(url_google));
  // Delete it via Expires.
  EXPECT_TRUE(cm.SetCookie(url_google,
                           std::string(kValidCookieLine) +
                           "; expires=Mon, 18-Apr-1977 22:50:13 GMT"));
  EXPECT_EQ("", cm.GetCookies(url_google));

  // Create a persistent cookie.
  EXPECT_TRUE(cm.SetCookie(url_google,
                           std::string(kValidCookieLine) +
                           "; expires=Mon, 18-Apr-22 22:50:13 GMT"));
  EXPECT_EQ("A=B", cm.GetCookies(url_google));
  // Delete it via Max-Age.
  EXPECT_TRUE(cm.SetCookie(url_google,
                           std::string(kValidCookieLine) + "; max-age=0"));
  EXPECT_EQ("", cm.GetCookies(url_google));

  // Create a persistent cookie.
  EXPECT_TRUE(cm.SetCookie(url_google,
                           std::string(kValidCookieLine) +
                           "; expires=Mon, 18-Apr-22 22:50:13 GMT"));
  EXPECT_EQ("A=B", cm.GetCookies(url_google));
  // Delete it via Expires.
  EXPECT_TRUE(cm.SetCookie(url_google,
                           std::string(kValidCookieLine) +
                           "; expires=Mon, 18-Apr-1977 22:50:13 GMT"));
  EXPECT_EQ("", cm.GetCookies(url_google));
}

TEST(CookieMonsterTest, TestCookieDeleteAll) {
  GURL url_google(kUrlGoogle);
  net::CookieMonster cm;
  net::CookieMonster::CookieOptions options;
  options.set_include_httponly();

  EXPECT_TRUE(cm.SetCookie(url_google, kValidCookieLine));
  EXPECT_EQ("A=B", cm.GetCookies(url_google));

  EXPECT_TRUE(cm.SetCookieWithOptions(url_google, "C=D; httponly", options));
  EXPECT_EQ("A=B; C=D", cm.GetCookiesWithOptions(url_google, options));

  EXPECT_EQ(2, cm.DeleteAll(false));
  EXPECT_EQ("", cm.GetCookiesWithOptions(url_google, options));
}

TEST(CookieMonsterTest, TestCookieDeleteAllCreatedAfterTimestamp) {
  GURL url_google(kUrlGoogle);
  net::CookieMonster cm;
  Time now = Time::Now();

  // Nothing has been added so nothing should be deleted.
  EXPECT_EQ(0, cm.DeleteAllCreatedAfter(now - TimeDelta::FromDays(99), false));

  // Create 3 cookies with creation date of today, yesterday and the day before.
  EXPECT_TRUE(cm.SetCookieWithCreationTime(url_google, "T-0=Now", now));
  EXPECT_TRUE(cm.SetCookieWithCreationTime(url_google, "T-1=Yesterday",
                                           now - TimeDelta::FromDays(1)));
  EXPECT_TRUE(cm.SetCookieWithCreationTime(url_google, "T-2=DayBefore",
                                           now - TimeDelta::FromDays(2)));

  // Try to delete everything from now onwards.
  EXPECT_EQ(1, cm.DeleteAllCreatedAfter(now, false));
  // Now delete the one cookie created in the last day.
  EXPECT_EQ(1, cm.DeleteAllCreatedAfter(now - TimeDelta::FromDays(1), false));
  // Now effectively delete all cookies just created (1 is remaining).
  EXPECT_EQ(1, cm.DeleteAllCreatedAfter(now - TimeDelta::FromDays(99), false));

  // Make sure everything is gone.
  EXPECT_EQ(0, cm.DeleteAllCreatedAfter(Time(), false));
  // Really make sure everything is gone.
  EXPECT_EQ(0, cm.DeleteAll(false));
}

TEST(CookieMonsterTest, TestCookieDeleteAllCreatedBetweenTimestamps) {
  GURL url_google(kUrlGoogle);
  net::CookieMonster cm;
  Time now = Time::Now();

  // Nothing has been added so nothing should be deleted.
  EXPECT_EQ(0, cm.DeleteAllCreatedAfter(now - TimeDelta::FromDays(99), false));

  // Create 3 cookies with creation date of today, yesterday and the day before.
  EXPECT_TRUE(cm.SetCookieWithCreationTime(url_google, "T-0=Now", now));
  EXPECT_TRUE(cm.SetCookieWithCreationTime(url_google, "T-1=Yesterday",
                                           now - TimeDelta::FromDays(1)));
  EXPECT_TRUE(cm.SetCookieWithCreationTime(url_google, "T-2=DayBefore",
                                           now - TimeDelta::FromDays(2)));
  EXPECT_TRUE(cm.SetCookieWithCreationTime(url_google, "T-3=ThreeDays",
                                           now - TimeDelta::FromDays(3)));
  EXPECT_TRUE(cm.SetCookieWithCreationTime(url_google, "T-7=LastWeek",
                                           now - TimeDelta::FromDays(7)));

  // Try to delete threedays and the daybefore.
  EXPECT_EQ(2, cm.DeleteAllCreatedBetween(now - TimeDelta::FromDays(3),
                                          now - TimeDelta::FromDays(1),
                                          false));

  // Try to delete yesterday, also make sure that delete_end is not
  // inclusive.
  EXPECT_EQ(1, cm.DeleteAllCreatedBetween(now - TimeDelta::FromDays(2),
                                          now,
                                          false));

  // Make sure the delete_begin is inclusive.
  EXPECT_EQ(1, cm.DeleteAllCreatedBetween(now - TimeDelta::FromDays(7),
                                          now,
                                          false));

  // Delete the last (now) item.
  EXPECT_EQ(1, cm.DeleteAllCreatedAfter(Time(), false));

  // Really make sure everything is gone.
  EXPECT_EQ(0, cm.DeleteAll(false));
}

TEST(CookieMonsterTest, TestSecure) {
  GURL url_google(kUrlGoogle);
  GURL url_google_secure(kUrlGoogleSecure);
  net::CookieMonster cm;

  EXPECT_TRUE(cm.SetCookie(url_google, "A=B"));
  EXPECT_EQ("A=B", cm.GetCookies(url_google));
  EXPECT_EQ("A=B", cm.GetCookies(url_google_secure));

  EXPECT_TRUE(cm.SetCookie(url_google_secure, "A=B; secure"));
  // The secure should overwrite the non-secure.
  EXPECT_EQ("", cm.GetCookies(url_google));
  EXPECT_EQ("A=B", cm.GetCookies(url_google_secure));

  EXPECT_TRUE(cm.SetCookie(url_google_secure, "D=E; secure"));
  EXPECT_EQ("", cm.GetCookies(url_google));
  EXPECT_EQ("A=B; D=E", cm.GetCookies(url_google_secure));

  EXPECT_TRUE(cm.SetCookie(url_google_secure, "A=B"));
  // The non-secure should overwrite the secure.
  EXPECT_EQ("A=B", cm.GetCookies(url_google));
  EXPECT_EQ("D=E; A=B", cm.GetCookies(url_google_secure));
}

static Time GetFirstCookieAccessDate(net::CookieMonster* cm) {
  const net::CookieMonster::CookieList all_cookies(cm->GetAllCookies());
  return all_cookies.front().second.LastAccessDate();
}

static const int kLastAccessThresholdSeconds = 1;

TEST(CookieMonsterTest, TestLastAccess) {
  GURL url_google(kUrlGoogle);
  net::CookieMonster cm(kLastAccessThresholdSeconds);

  EXPECT_TRUE(cm.SetCookie(url_google, "A=B"));
  const Time last_access_date(GetFirstCookieAccessDate(&cm));

  // Reading the cookie again immediately shouldn't update the access date,
  // since we're inside the threshold.
  EXPECT_EQ("A=B", cm.GetCookies(url_google));
  EXPECT_TRUE(last_access_date == GetFirstCookieAccessDate(&cm));

  // Reading after a short wait should update the access date.
  PlatformThread::Sleep(1500);
  EXPECT_EQ("A=B", cm.GetCookies(url_google));
  EXPECT_FALSE(last_access_date == GetFirstCookieAccessDate(&cm));
}

static int CountInString(const std::string& str, char c) {
  int count = 0;
  for (std::string::const_iterator it = str.begin();
       it != str.end(); ++it) {
    if (*it == c)
      ++count;
  }
  return count;
}

TEST(CookieMonsterTest, TestHostGarbageCollection) {
  GURL url_google(kUrlGoogle);
  net::CookieMonster cm;
  // Add a bunch of cookies on a single host, should purge them.
  for (int i = 0; i < 101; i++) {
    std::string cookie = StringPrintf("a%03d=b", i);
    EXPECT_TRUE(cm.SetCookie(url_google, cookie));
    std::string cookies = cm.GetCookies(url_google);
    // Make sure we find it in the cookies.
    EXPECT_TRUE(cookies.find(cookie) != std::string::npos);
    // Count the number of cookies.
    EXPECT_LE(CountInString(cookies, '='), 70);
  }
}

TEST(CookieMonsterTest, TestTotalGarbageCollection) {
  net::CookieMonster cm(kLastAccessThresholdSeconds);
  // Add a bunch of cookies on a bunch of host, some should get purged.
  const GURL sticky_cookie("http://a0000.izzle");
  for (int i = 0; i < 2000; ++i) {
    GURL url(StringPrintf("http://a%04d.izzle", i));
    EXPECT_TRUE(cm.SetCookie(url, "a=b"));
    EXPECT_EQ("a=b", cm.GetCookies(url));

    // Keep touching the first cookie to ensure it's not purged (since it will
    // always have the most recent access time).
    if (!(i % 500)) {
      PlatformThread::Sleep(1500);  // Ensure the timestamps will be different
                                    // enough to update.
      EXPECT_EQ("a=b", cm.GetCookies(sticky_cookie));
    }
  }

  // Check that cookies that still exist.
  for (int i = 0; i < 2000; ++i) {
    GURL url(StringPrintf("http://a%04d.izzle", i));
    if ((i == 0) || (i > 1101)) {
      // Cookies should still be around.
      EXPECT_FALSE(cm.GetCookies(url).empty());
    } else if (i < 901) {
      // Cookies should have gotten purged.
      EXPECT_TRUE(cm.GetCookies(url).empty());
    }
  }
}

// Formerly NetUtilTest.CookieTest back when we used wininet's cookie handling.
TEST(CookieMonsterTest, NetUtilCookieTest) {
  const GURL test_url("http://mojo.jojo.google.izzle/");

  net::CookieMonster cm;

  EXPECT_TRUE(cm.SetCookie(test_url, "foo=bar"));
  std::string value = cm.GetCookies(test_url);
  EXPECT_EQ("foo=bar", value);

  // test that we can retrieve all cookies:
  EXPECT_TRUE(cm.SetCookie(test_url, "x=1"));
  EXPECT_TRUE(cm.SetCookie(test_url, "y=2"));

  std::string result = cm.GetCookies(test_url);
  EXPECT_FALSE(result.empty());
  EXPECT_NE(result.find("x=1"), std::string::npos) << result;
  EXPECT_NE(result.find("y=2"), std::string::npos) << result;
}

static bool FindAndDeleteCookie(net::CookieMonster& cm,
                                const std::string& domain,
                                const std::string& name) {
  net::CookieMonster::CookieList cookies = cm.GetAllCookies();
  for (net::CookieMonster::CookieList::iterator it = cookies.begin();
       it != cookies.end(); ++it)
    if (it->first == domain && it->second.Name() == name)
      return cm.DeleteCookie(domain, it->second, false);
  return false;
}

TEST(CookieMonsterTest, TestDeleteSingleCookie) {
  GURL url_google(kUrlGoogle);

  net::CookieMonster cm;
  EXPECT_TRUE(cm.SetCookie(url_google, "A=B"));
  EXPECT_TRUE(cm.SetCookie(url_google, "C=D"));
  EXPECT_TRUE(cm.SetCookie(url_google, "E=F"));
  EXPECT_EQ("A=B; C=D; E=F", cm.GetCookies(url_google));

  EXPECT_TRUE(FindAndDeleteCookie(cm, url_google.host(), "C"));
  EXPECT_EQ("A=B; E=F", cm.GetCookies(url_google));

  EXPECT_FALSE(FindAndDeleteCookie(cm, "random.host", "E"));
  EXPECT_EQ("A=B; E=F", cm.GetCookies(url_google));
}

// TODO test overwrite cookie

