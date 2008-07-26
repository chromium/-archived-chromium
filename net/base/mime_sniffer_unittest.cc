// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"
#include "net/base/mime_sniffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  class MimeSnifferTest : public testing::Test {
  };
}

struct SnifferTest {
  const char* content;
  size_t content_len;
  std::string url;
  std::string type_hint;
  const char* mime_type;
};

static void TestArray(SnifferTest* tests, size_t count) {
  std::string mime_type;

  for (size_t i = 0; i < count; ++i) {
    mime_util::SniffMimeType(tests[i].content,
                             tests[i].content_len,
                             GURL(tests[i].url),
                             tests[i].type_hint,
                             &mime_type);
    EXPECT_EQ(tests[i].mime_type, mime_type);
  }
}

// TODO(evanm): convert other tests to use SniffMimeType instead of TestArray,
// so the error messages produced by test failures are more useful.
static std::string SniffMimeType(const std::string& content,
                                 const std::string& url,
                                 const std::string& mime_type_hint) {
  std::string mime_type;
  mime_util::SniffMimeType(content.data(), content.size(), GURL(url),
                           mime_type_hint, &mime_type);
  return mime_type;
}

TEST(MimeSnifferTest, BoundaryConditionsTest) {
  std::string mime_type;
  std::string type_hint;

  char buf[] = {
    'd', '\x1f', '\xFF'
  };

  GURL url;

  mime_util::SniffMimeType(buf, 0, url, type_hint, &mime_type);
  EXPECT_EQ("text/plain", mime_type);
  mime_util::SniffMimeType(buf, 1, url, type_hint, &mime_type);
  EXPECT_EQ("text/plain", mime_type);
  mime_util::SniffMimeType(buf, 2, url, type_hint, &mime_type);
  EXPECT_EQ("application/octet-stream", mime_type);
}

TEST(MimeSnifferTest, BasicSniffingTest) {
  SnifferTest tests[] = {
    { "<!DOCTYPE html PUBLIC", sizeof("<!DOCTYPE html PUBLIC")-1,
      "http://www.example.com/",
      "", "text/html" },
    { "<HtMl><Body></body></htMl>", sizeof("<HtMl><Body></body></htMl>")-1,
      "http://www.example.com/foo.gif",
      "application/octet-stream", "application/octet-stream" },
    { "GIF89a\x1F\x83\x94", sizeof("GIF89a\xAF\x83\x94")-1,
      "http://www.example.com/foo",
      "text/plain", "image/gif" },
    { "Gif87a\x1F\x83\x94", sizeof("Gif87a\xAF\x83\x94")-1,
      "http://www.example.com/foo?param=tt.gif",
      "", "application/octet-stream" },
    { "%!PS-Adobe-3.0", sizeof("%!PS-Adobe-3.0")-1,
      "http://www.example.com/foo",
      "text/plain", "text/plain" },
    { "\x89" "PNG\x0D\x0A\x1A\x0A", sizeof("\x89" "PNG\x0D\x0A\x1A\x0A")-1,
      "http://www.example.com/foo",
      "application/octet-stream", "image/png" },
    { "\xFF\xD8\xFF\x23\x49\xAF", sizeof("\xFF\xD8\xFF\x23\x49\xAF")-1,
      "http://www.example.com/foo",
      "", "image/jpeg" },
  };

  TestArray(tests, arraysize(tests));
}

TEST(MimeSnifferTest, MozillaCompatibleTest) {
  SnifferTest tests[] = {
    { " \n <hTmL>\n <hea", sizeof(" \n <hTmL>\n <hea")-1,
      "http://www.example.com/",
      "", "text/html" },
    { " \n <hTmL>\n <hea", sizeof(" \n <hTmL>\n <hea")-1,
      "http://www.example.com/",
      "text/plain", "text/plain" },
    { "BMjlakdsfk", sizeof("BMjlakdsfk")-1,
      "http://www.example.com/foo",
      "", "image/bmp" },
    { "\x00\x00\x20\x00", sizeof("\x00\x00\x30\x00")-1,
      "http://www.example.com/favicon",
      "", "image/x-icon" },
    { "\x00\x00\x30\x00", sizeof("\x00\x00\x30\x00")-1,
      "http://www.example.com/favicon.ico",
      "", "application/octet-stream" },
    { "#!/bin/sh\nls /\n", sizeof("#!/bin/sh\nls /\n")-1,
      "http://www.example.com/foo",
      "", "text/plain" },
    { "From: Fred\nTo: Bob\n\nHi\n.\n",
      sizeof("From: Fred\nTo: Bob\n\nHi\n.\n")-1,
      "http://www.example.com/foo",
      "", "text/plain" },
    { "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n",
      sizeof("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n")-1,
      "http://www.example.com/foo",
      "", "text/xml" },
    { "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n",
      sizeof("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n")-1,
      "http://www.example.com/foo",
      "application/octet-stream", "application/octet-stream" },
  };

  TestArray(tests, arraysize(tests));
}

TEST(MimeSnifferTest, DontAllowPrivilegeEscalationTest) {
  SnifferTest tests[] = {
    { "GIF87a\n<html>\n<body>"
        "<script>alert('haxorzed');\n</script>"
        "</body></html>\n",
      sizeof("GIF87a\n<html>\n<body>"
        "<script>alert('haxorzed');\n</script>"
        "</body></html>\n")-1,
      "http://www.example.com/foo",
      "", "image/gif" },
    { "GIF87a\n<html>\n<body>"
        "<script>alert('haxorzed');\n</script>"
        "</body></html>\n",
      sizeof("GIF87a\n<html>\n<body>"
        "<script>alert('haxorzed');\n</script>"
        "</body></html>\n")-1,
      "http://www.example.com/foo?q=ttt.html",
      "", "image/gif" },
    { "GIF87a\n<html>\n<body>"
        "<script>alert('haxorzed');\n</script>"
        "</body></html>\n",
      sizeof("GIF87a\n<html>\n<body>"
        "<script>alert('haxorzed');\n</script>"
        "</body></html>\n")-1,
      "http://www.example.com/foo#ttt.html",
      "", "image/gif" },
    { "a\n<html>\n<body>"
        "<script>alert('haxorzed');\n</script>"
        "</body></html>\n",
      sizeof("a\n<html>\n<body>"
        "<script>alert('haxorzed');\n</script>"
        "</body></html>\n")-1,
      "http://www.example.com/foo",
      "", "text/plain" },
    { "a\n<html>\n<body>"
        "<script>alert('haxorzed');\n</script>"
        "</body></html>\n",
      sizeof("a\n<html>\n<body>"
        "<script>alert('haxorzed');\n</script>"
        "</body></html>\n")-1,
      "http://www.example.com/foo?q=ttt.html",
      "", "text/plain" },
    { "a\n<html>\n<body>"
        "<script>alert('haxorzed');\n</script>"
        "</body></html>\n",
      sizeof("a\n<html>\n<body>"
        "<script>alert('haxorzed');\n</script>"
        "</body></html>\n")-1,
      "http://www.example.com/foo#ttt.html",
      "", "text/plain" },
    { "a\n<html>\n<body>"
        "<script>alert('haxorzed');\n</script>"
        "</body></html>\n",
      sizeof("a\n<html>\n<body>"
        "<script>alert('haxorzed');\n</script>"
        "</body></html>\n")-1,
      "http://www.example.com/foo.html",
      "", "text/plain" },
  };

  TestArray(tests, arraysize(tests));
}

TEST(MimeSnifferTest, UnicodeTest) {
  SnifferTest tests[] = {
    { "\xEF\xBB\xBF" "Hi there", sizeof("\xEF\xBB\xBF" "Hi there")-1,
      "http://www.example.com/foo",
      "", "text/plain" },
    { "\xEF\xBB\xBF\xED\x7A\xAD\x7A\x0D\x79",
      sizeof("\xEF\xBB\xBF\xED\x7A\xAD\x7A\x0D\x79")-1,
      "http://www.example.com/foo",
      "", "text/plain" },
    { "\xFE\xFF\xD0\xA5\xD0\xBE\xD0\xBB\xD1\x83\xD0\xB9",
      sizeof("\xFE\xFF\xD0\xA5\xD0\xBE\xD0\xBB\xD1\x83\xD0\xB9")-1,
      "http://www.example.com/foo",
      "", "text/plain" },
    { "\xFE\xFF\x00\x41\x00\x20\xD8\x00\xDC\x00\xD8\x00\xDC\x01",
      sizeof("\xFE\xFF\x00\x41\x00\x20\xD8\x00\xDC\x00\xD8\x00\xDC\x01")-1,
      "http://www.example.com/foo",
      "", "text/plain" },
  };

  TestArray(tests, arraysize(tests));
}

TEST(MimeSnifferTest, FlashTest) {
  SnifferTest tests[] = {
    { "CWSdd\x00\xB3", sizeof("CWSdd\x00\xB3")-1,
      "http://www.example.com/foo",
      "", "application/octet-stream" },
    { "FLVjdkl*(#)0sdj\x00", sizeof("FLVjdkl*(#)0sdj\x00")-1,
      "http://www.example.com/foo?q=ttt.swf",
      "", "application/octet-stream" },
    { "FWS3$9\r\b\x00", sizeof("FWS3$9\r\b\x00")-1,
      "http://www.example.com/foo#ttt.swf",
      "", "application/octet-stream" },
    { "FLVjdkl*(#)0sdj", sizeof("FLVjdkl*(#)0sdj")-1,
      "http://www.example.com/foo.swf",
      "", "text/plain" },
    { "FLVjdkl*(#)0s\x01dj", sizeof("FLVjdkl*(#)0s\x01dj")-1,
      "http://www.example.com/foo/bar.swf",
      "", "application/octet-stream" },
    { "FWS3$9\r\b\x1A", sizeof("FWS3$9\r\b\x1A")-1,
      "http://www.example.com/foo.swf?clickTAG=http://www.adnetwork.com/bar",
      "", "application/octet-stream" },
    { "FWS3$9\r\x1C\b", sizeof("FWS3$9\r\x1C\b")-1,
      "http://www.example.com/foo.swf?clickTAG=http://www.adnetwork.com/bar",
      "text/plain", "application/octet-stream" },
  };

  TestArray(tests, arraysize(tests));
}

TEST(MimeSnifferTest, XMLTest) {
  // An easy feed to identify.
  EXPECT_EQ("application/atom+xml",
            SniffMimeType("<?xml?><feed", "", "text/xml"));
  // Don't sniff out of plain text.
  EXPECT_EQ("text/plain",
            SniffMimeType("<?xml?><feed", "", "text/plain"));
  // Simple RSS.
  EXPECT_EQ("application/rss+xml",
            SniffMimeType("<?xml version='1.0'?>\r\n<rss", "", "text/xml"));

  // The top of CNN's RSS feed, which we'd like to recognize as RSS.
  static const char kCNNRSS[] =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<?xml-stylesheet href=\"http://rss.cnn.com/~d/styles/rss2full.xsl\" "
      "type=\"text/xsl\" media=\"screen\"?>"
      "<?xml-stylesheet href=\"http://rss.cnn.com/~d/styles/itemcontent.css\" "
      "type=\"text/css\" media=\"screen\"?>"
      "<rss xmlns:feedburner=\"http://rssnamespace.org/feedburner/ext/1.0\" "
      "version=\"2.0\">";
  // CNN's RSS
  EXPECT_EQ("application/rss+xml",
            SniffMimeType(kCNNRSS, "", "text/xml"));
  EXPECT_EQ("text/plain",
            SniffMimeType(kCNNRSS, "", "text/plain"));

  // Don't sniff random XML as something different.
  EXPECT_EQ("text/xml",
            SniffMimeType("<?xml?><notafeed", "", "text/xml"));
  // Don't sniff random plain-text as something different.
  EXPECT_EQ("text/plain",
            SniffMimeType("<?xml?><notafeed", "", "text/plain"));

  // Positive test for the two instances we upgrade to XHTML.
  EXPECT_EQ("application/xhtml+xml",
            SniffMimeType("<html xmlns=\"http://www.w3.org/1999/xhtml\">",
                          "", "text/xml"));
  EXPECT_EQ("application/xhtml+xml",
            SniffMimeType("<html xmlns=\"http://www.w3.org/1999/xhtml\">",
                          "", "application/xml"));

  // Following our behavior with HTML, don't call other mime types XHTML.
  EXPECT_EQ("text/plain",
            SniffMimeType("<html xmlns=\"http://www.w3.org/1999/xhtml\">",
                          "", "text/plain"));
  EXPECT_EQ("application/rss+xml",
            SniffMimeType("<html xmlns=\"http://www.w3.org/1999/xhtml\">",
                          "", "application/rss+xml"));

  // Don't sniff other HTML-looking bits as HTML.
  EXPECT_EQ("text/xml",
            SniffMimeType("<html><head>", "", "text/xml"));
  EXPECT_EQ("text/xml",
            SniffMimeType("<foo><html xmlns=\"http://www.w3.org/1999/xhtml\">",
                          "", "text/xml"));

}
