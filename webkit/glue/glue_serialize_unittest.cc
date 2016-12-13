// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/api/public/WebHTTPBody.h"
#include "webkit/api/public/WebPoint.h"
#include "webkit/api/public/WebVector.h"
#include "webkit/glue/glue_serialize.h"

using namespace std;
using namespace webkit_glue;

using WebKit::WebData;
using WebKit::WebHistoryItem;
using WebKit::WebHTTPBody;
using WebKit::WebPoint;
using WebKit::WebString;
using WebKit::WebUChar;
using WebKit::WebVector;

namespace {
class GlueSerializeTest : public testing::Test {
 public:
  // Makes a FormData with some random data.
  WebHTTPBody MakeFormData() {
    WebHTTPBody http_body;
    http_body.initialize();

    const char d1[] = "first data block";
    http_body.appendData(WebData(d1, sizeof(d1)-1));

    http_body.appendFile(WebString::fromUTF8("file.txt"));

    const char d2[] = "data the second";
    http_body.appendData(WebData(d2, sizeof(d2)-1));

    return http_body;
  }

  // Constructs a HistoryItem with some random data and an optional child.
  WebHistoryItem MakeHistoryItem(bool with_form_data, bool pregnant) {
    WebHistoryItem item;
    item.initialize();

    item.setURLString(WebString::fromUTF8("urlString"));
    item.setOriginalURLString(WebString::fromUTF8("originalURLString"));
    item.setTarget(WebString::fromUTF8("target"));
    item.setParent(WebString::fromUTF8("parent"));
    item.setTitle(WebString::fromUTF8("title"));
    item.setAlternateTitle(WebString::fromUTF8("alternateTitle"));
    item.setLastVisitedTime(13.37);
    item.setScrollOffset(WebPoint(42, -42));
    item.setIsTargetItem(true);
    item.setVisitCount(42*42);

    WebVector<WebString> document_state(size_t(3));
    document_state[0] = WebString::fromUTF8("state1");
    document_state[1] = WebString::fromUTF8("state2");
    document_state[2] = WebString::fromUTF8("state AWESOME");
    item.setDocumentState(document_state);

    // Form Data
    if (with_form_data) {
      item.setHTTPBody(MakeFormData());
      item.setHTTPContentType(WebString::fromUTF8("formContentType"));
    }

    // Setting the FormInfo causes the referrer to be set, so we set the
    // referrer after setting the form info.
    item.setReferrer(WebString::fromUTF8("referrer"));

    // Children
    if (pregnant)
      item.appendToChildren(MakeHistoryItem(false, false));

    return item;
  }

  // Checks that a == b.
  void HistoryItemExpectEqual(const WebHistoryItem& a,
                              const WebHistoryItem& b) {
    EXPECT_EQ(string16(a.urlString()), string16(b.urlString()));
    EXPECT_EQ(string16(a.originalURLString()), string16(b.originalURLString()));
    EXPECT_EQ(string16(a.target()), string16(b.target()));
    EXPECT_EQ(string16(a.parent()), string16(b.parent()));
    EXPECT_EQ(string16(a.title()), string16(b.title()));
    EXPECT_EQ(string16(a.alternateTitle()), string16(b.alternateTitle()));
    EXPECT_EQ(a.lastVisitedTime(), b.lastVisitedTime());
    EXPECT_EQ(a.scrollOffset(), b.scrollOffset());
    EXPECT_EQ(a.isTargetItem(), b.isTargetItem());
    EXPECT_EQ(a.visitCount(), b.visitCount());
    EXPECT_EQ(string16(a.referrer()), string16(b.referrer()));

    const WebVector<WebString>& a_docstate = a.documentState();
    const WebVector<WebString>& b_docstate = b.documentState();
    EXPECT_EQ(a_docstate.size(), b_docstate.size());
    for (size_t i = 0, c = a_docstate.size(); i < c; ++i)
      EXPECT_EQ(string16(a_docstate[i]), string16(b_docstate[i]));

    // Form Data
    const WebHTTPBody& a_body = a.httpBody();
    const WebHTTPBody& b_body = b.httpBody();
    EXPECT_EQ(!a_body.isNull(), !b_body.isNull());
    if (!a_body.isNull() && !b_body.isNull()) {
      EXPECT_EQ(a_body.elementCount(), b_body.elementCount());
      WebHTTPBody::Element a_elem, b_elem;
      for (size_t i = 0; a_body.elementAt(i, a_elem) &&
                         b_body.elementAt(i, b_elem); ++i) {
        EXPECT_EQ(a_elem.type, b_elem.type);
        if (a_elem.type == WebHTTPBody::Element::TypeData) {
          EXPECT_EQ(string(a_elem.data.data(), a_elem.data.size()),
                    string(b_elem.data.data(), b_elem.data.size()));
        } else {
          EXPECT_EQ(string16(a_elem.filePath), string16(b_elem.filePath));
        }
      }
    }
    EXPECT_EQ(string16(a.httpContentType()), string16(b.httpContentType()));

    // Children
    const WebVector<WebHistoryItem>& a_children = a.children();
    const WebVector<WebHistoryItem>& b_children = b.children();
    EXPECT_EQ(a_children.size(), b_children.size());
    for (size_t i = 0, c = a_children.size(); i < c; ++i)
      HistoryItemExpectEqual(a_children[i], b_children[i]);
  }
};

// Test old versions of serialized data to ensure that newer versions of code
// can still read history items written by previous versions.
TEST_F(GlueSerializeTest, BackwardsCompatibleTest) {
  const WebHistoryItem& item = MakeHistoryItem(false, false);

  // Make sure version 3 (current version) can read versions 1 and 2.
  for (int i = 1; i <= 2; i++) {
    string serialized_item;
    HistoryItemToVersionedString(item, i, &serialized_item);
    const WebHistoryItem& deserialized_item =
        HistoryItemFromString(serialized_item);
    ASSERT_FALSE(item.isNull());
    ASSERT_FALSE(deserialized_item.isNull());
    HistoryItemExpectEqual(item, deserialized_item);
  }
}

// Makes sure that a HistoryItem remains intact after being serialized and
// deserialized.
TEST_F(GlueSerializeTest, HistoryItemSerializeTest) {
  const WebHistoryItem& item = MakeHistoryItem(true, true);
  const string& serialized_item = HistoryItemToString(item);
  const WebHistoryItem& deserialized_item =
      HistoryItemFromString(serialized_item);

  ASSERT_FALSE(item.isNull());
  ASSERT_FALSE(deserialized_item.isNull());
  HistoryItemExpectEqual(item, deserialized_item);
}


} // namespace
