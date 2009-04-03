// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include <string>

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "CString.h"
#include "FormData.h"
#include "HistoryItem.h"
#include "PlatformString.h"
#include "ResourceRequest.h"
MSVC_POP_WARNING();

#undef LOG

#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/glue_serialize.h"

using namespace std;
using namespace WebCore;
using namespace webkit_glue;

// These exist only to support the gTest assertion macros, and
// shouldn't be used in normal program code.
static std::ostream& operator<<(std::ostream& out, const String& str)
{
  return out << str.latin1().data();
}

static std::ostream& operator<<(std::ostream& out, const FormDataElement& e)
{
  return out << e.m_type << ": " <<
    e.m_data.size() <<
    " <" << string(e.m_data.data() ? e.m_data.data() : "", e.m_data.size()) << "> " <<
    e.m_filename;
}

template<typename T>
static std::ostream& operator<<(std::ostream& out, const Vector<T>& v)
{
  for (size_t i = 0; i < v.size(); ++i)
    out << "[" << v[i] << "] ";

  return out;
}

static std::ostream& operator<<(std::ostream& out, const FormData& data)
{
  return out << data.elements();
}

static std::ostream& operator<<(std::ostream& out, const IntPoint& pt)
{
  return out << "(" << pt.x() << "," << pt.y() << ")";
}

namespace {
class GlueSerializeTest : public testing::Test {
 public:
  // Makes a FormData with some random data.
  PassRefPtr<FormData> MakeFormData() {
    RefPtr<FormData> form_data = FormData::create();

    char d1[] = "first data block";
    form_data->appendData(d1, sizeof(d1)-1);

    form_data->appendFile("file.txt");

    char d2[] = "data the second";
    form_data->appendData(d2, sizeof(d2)-1);

    return form_data.release();
  }

  // Constructs a HistoryItem with some random data and an optional child.
  PassRefPtr<HistoryItem> MakeHistoryItem(bool with_form_data, bool pregnant) {
    RefPtr<HistoryItem> item = HistoryItem::create();

    item->setURLString("urlString");
    item->setOriginalURLString("originalURLString");
    item->setTarget("target");
    item->setParent("parent");
    item->setTitle("title");
    item->setAlternateTitle("alternateTitle");
    item->setLastVisitedTime(13.37);
    item->setScrollPoint(IntPoint(42, -42));
    item->setIsTargetItem(true);
    item->setVisitCount(42*42);

    Vector<String> document_state;
    document_state.append("state1");
    document_state.append("state2");
    document_state.append("state AWESOME");
    item->setDocumentState(document_state);

    // Form Data
    ResourceRequest dummy_request;  // only way to initialize HistoryItem
    if (with_form_data) {
      dummy_request.setHTTPBody(MakeFormData());
      dummy_request.setHTTPContentType("formContentType");
      dummy_request.setHTTPReferrer("referrer");
      dummy_request.setHTTPMethod("POST");
    }
    item->setFormInfoFromRequest(dummy_request);

    // Setting the FormInfo causes the referrer to be set, so we set the
    // referrer after setting the form info.
    item->setReferrer("referrer");

    // Children
    if (pregnant)
      item->addChildItem(MakeHistoryItem(false, false));

    return item.release();
  }

  // Checks that a == b.
  void HistoryItemExpectEqual(HistoryItem* a, HistoryItem* b) {
    EXPECT_EQ(a->urlString(), b->urlString());
    EXPECT_EQ(a->originalURLString(), b->originalURLString());
    EXPECT_EQ(a->target(), b->target());
    EXPECT_EQ(a->parent(), b->parent());
    EXPECT_EQ(a->title(), b->title());
    EXPECT_EQ(a->alternateTitle(), b->alternateTitle());
    EXPECT_EQ(a->lastVisitedTime(), b->lastVisitedTime());
    EXPECT_EQ(a->scrollPoint(), b->scrollPoint());
    EXPECT_EQ(a->isTargetItem(), b->isTargetItem());
    EXPECT_EQ(a->visitCount(), b->visitCount());
    EXPECT_EQ(a->referrer(), b->referrer());
    EXPECT_EQ(a->documentState(), b->documentState());

    // Form Data
    EXPECT_EQ(a->formData() != NULL, b->formData() != NULL);
    if (a->formData() && b->formData())
      EXPECT_EQ(*a->formData(), *b->formData());
    EXPECT_EQ(a->formContentType(), b->formContentType());

    // Children
    EXPECT_EQ(a->children().size(), b->children().size());
    for (size_t i = 0, c = a->children().size(); i < c; ++i) {
      HistoryItemExpectEqual(a->children().at(i).get(),
                             b->children().at(i).get());
    }
  }
};

// Test old versions of serialized data to ensure that newer versions of code
// can still read history items written by previous versions.
TEST_F(GlueSerializeTest, BackwardsCompatibleTest) {
  RefPtr<HistoryItem> item = MakeHistoryItem(false, false);

  // Make sure version 3 (current version) can read versions 1 and 2.
  for (int i = 1; i <= 2; i++) {
    string serialized_item;
    HistoryItemToVersionedString(item.get(), i, &serialized_item);
    RefPtr<HistoryItem> deserialized_item = HistoryItemFromString(serialized_item);
    ASSERT_FALSE(item == NULL);
    ASSERT_FALSE(deserialized_item == NULL);
    HistoryItemExpectEqual(item.get(), deserialized_item.get());
  }
}

// Makes sure that a HistoryItem remains intact after being serialized and
// deserialized.
TEST_F(GlueSerializeTest, HistoryItemSerializeTest) {
  RefPtr<HistoryItem> item = MakeHistoryItem(true, true);
  string serialized_item;
  HistoryItemToString(item, &serialized_item);
  RefPtr<HistoryItem> deserialized_item = HistoryItemFromString(serialized_item);

  ASSERT_FALSE(item == NULL);
  ASSERT_FALSE(deserialized_item == NULL);
  HistoryItemExpectEqual(item.get(), deserialized_item.get());
}


} // namespace
