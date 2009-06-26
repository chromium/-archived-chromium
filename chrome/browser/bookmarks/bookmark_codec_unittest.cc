// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_codec.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_test_utils.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const wchar_t kUrl1Title[] = L"url1";
const wchar_t kUrl1Url[] = L"http://www.url1.com";
const wchar_t kUrl2Title[] = L"url2";
const wchar_t kUrl2Url[] = L"http://www.url2.com";
const wchar_t kUrl3Title[] = L"url3";
const wchar_t kUrl3Url[] = L"http://www.url3.com";
const wchar_t kUrl4Title[] = L"url4";
const wchar_t kUrl4Url[] = L"http://www.url4.com";
const wchar_t kGroup1Title[] = L"group1";
const wchar_t kGroup2Title[] = L"group2";

// Helper to get a mutable bookmark node.
static BookmarkNode* AsMutable(const BookmarkNode* node) {
  return const_cast<BookmarkNode*>(node);
}

}  // anonymous namespace

class BookmarkCodecTest : public testing::Test {
 protected:
  // Helpers to create bookmark models with different data.
  BookmarkModel* CreateTestModel1() {
    scoped_ptr<BookmarkModel> model(new BookmarkModel(NULL));
    const BookmarkNode* bookmark_bar = model->GetBookmarkBarNode();
    model->AddURL(bookmark_bar, 0, kUrl1Title, GURL(kUrl1Url));
    return model.release();
  }
  BookmarkModel* CreateTestModel2() {
    scoped_ptr<BookmarkModel> model(new BookmarkModel(NULL));
    const BookmarkNode* bookmark_bar = model->GetBookmarkBarNode();
    model->AddURL(bookmark_bar, 0, kUrl1Title, GURL(kUrl1Url));
    model->AddURL(bookmark_bar, 1, kUrl2Title, GURL(kUrl2Url));
    return model.release();
  }
  BookmarkModel* CreateTestModel3() {
    scoped_ptr<BookmarkModel> model(new BookmarkModel(NULL));
    const BookmarkNode* bookmark_bar = model->GetBookmarkBarNode();
    model->AddURL(bookmark_bar, 0, kUrl1Title, GURL(kUrl1Url));
    const BookmarkNode* group1 = model->AddGroup(bookmark_bar, 1, kGroup1Title);
    model->AddURL(group1, 0, kUrl2Title, GURL(kUrl2Url));
    return model.release();
  }

  void GetBookmarksBarChildValue(Value* value,
                                 size_t index,
                                 DictionaryValue** result_value) {
    ASSERT_EQ(Value::TYPE_DICTIONARY, value->GetType());

    DictionaryValue* d_value = static_cast<DictionaryValue*>(value);
    Value* roots;
    ASSERT_TRUE(d_value->Get(BookmarkCodec::kRootsKey, &roots));
    ASSERT_EQ(Value::TYPE_DICTIONARY, roots->GetType());

    DictionaryValue* roots_d_value = static_cast<DictionaryValue*>(roots);
    Value* bb_value;
    ASSERT_TRUE(roots_d_value->Get(BookmarkCodec::kRootFolderNameKey,
                                   &bb_value));
    ASSERT_EQ(Value::TYPE_DICTIONARY, bb_value->GetType());

    DictionaryValue* bb_d_value = static_cast<DictionaryValue*>(bb_value);
    Value* bb_children_value;
    ASSERT_TRUE(bb_d_value->Get(BookmarkCodec::kChildrenKey,
                                &bb_children_value));
    ASSERT_EQ(Value::TYPE_LIST, bb_children_value->GetType());

    ListValue* bb_children_l_value = static_cast<ListValue*>(bb_children_value);
    Value* child_value;
    ASSERT_TRUE(bb_children_l_value->Get(index, &child_value));
    ASSERT_EQ(Value::TYPE_DICTIONARY, child_value->GetType());

    *result_value = static_cast<DictionaryValue*>(child_value);
  }

  Value* EncodeHelper(BookmarkModel* model, std::string* checksum) {
    BookmarkCodec encoder;
    // Computed and stored checksums should be empty.
    EXPECT_EQ("", encoder.computed_checksum());
    EXPECT_EQ("", encoder.stored_checksum());

    scoped_ptr<Value> value(encoder.Encode(model));
    const std::string& computed_checksum = encoder.computed_checksum();
    const std::string& stored_checksum = encoder.stored_checksum();

    // Computed and stored checksums should not be empty and should be equal.
    EXPECT_FALSE(computed_checksum.empty());
    EXPECT_FALSE(stored_checksum.empty());
    EXPECT_EQ(computed_checksum, stored_checksum);

    *checksum = computed_checksum;
    return value.release();
  }

  bool Decode(BookmarkCodec* codec, BookmarkModel* model, const Value& value) {
    int max_id;
    bool result = codec->Decode(AsMutable(model->GetBookmarkBarNode()),
                                AsMutable(model->other_node()),
                                &max_id, value);
    model->set_next_node_id(max_id);
    return result;
  }

  BookmarkModel* DecodeHelper(const Value& value,
                              const std::string& expected_stored_checksum,
                              std::string* computed_checksum,
                              bool expected_changes) {
    BookmarkCodec decoder;
    // Computed and stored checksums should be empty.
    EXPECT_EQ("", decoder.computed_checksum());
    EXPECT_EQ("", decoder.stored_checksum());

    scoped_ptr<BookmarkModel> model(new BookmarkModel(NULL));
    EXPECT_TRUE(Decode(&decoder, model.get(), value));

    *computed_checksum = decoder.computed_checksum();
    const std::string& stored_checksum = decoder.stored_checksum();

    // Computed and stored checksums should not be empty.
    EXPECT_FALSE(computed_checksum->empty());
    EXPECT_FALSE(stored_checksum.empty());

    // Stored checksum should be as expected.
    EXPECT_EQ(expected_stored_checksum, stored_checksum);

    // The two checksums should be equal if expected_changes is true; otherwise
    // they should be different.
    if (expected_changes)
      EXPECT_NE(*computed_checksum, stored_checksum);
    else
      EXPECT_EQ(*computed_checksum, stored_checksum);

    return model.release();
  }
};

TEST_F(BookmarkCodecTest, ChecksumEncodeDecodeTest) {
  scoped_ptr<BookmarkModel> model_to_encode(CreateTestModel1());
  std::string enc_checksum;
  scoped_ptr<Value> value(EncodeHelper(model_to_encode.get(), &enc_checksum));

  EXPECT_TRUE(value.get() != NULL);

  std::string dec_checksum;
  scoped_ptr<BookmarkModel> decoded_model(DecodeHelper(
      *value.get(), enc_checksum, &dec_checksum, false));
}

TEST_F(BookmarkCodecTest, ChecksumEncodeIdenticalModelsTest) {
  // Encode two identical models and make sure the check-sums are same as long
  // as the data is the same.
  scoped_ptr<BookmarkModel> model1(CreateTestModel1());
  std::string enc_checksum1;
  scoped_ptr<Value> value1(EncodeHelper(model1.get(), &enc_checksum1));
  EXPECT_TRUE(value1.get() != NULL);

  scoped_ptr<BookmarkModel> model2(CreateTestModel1());
  std::string enc_checksum2;
  scoped_ptr<Value> value2(EncodeHelper(model2.get(), &enc_checksum2));
  EXPECT_TRUE(value2.get() != NULL);

  ASSERT_EQ(enc_checksum1, enc_checksum2);
}

TEST_F(BookmarkCodecTest, ChecksumManualEditTest) {
  scoped_ptr<BookmarkModel> model_to_encode(CreateTestModel1());
  std::string enc_checksum;
  scoped_ptr<Value> value(EncodeHelper(model_to_encode.get(), &enc_checksum));

  EXPECT_TRUE(value.get() != NULL);

  // Change something in the encoded value before decoding it.
  DictionaryValue* child1_value;
  GetBookmarksBarChildValue(value.get(), 0, &child1_value);
  std::wstring title;
  ASSERT_TRUE(child1_value->GetString(BookmarkCodec::kNameKey, &title));
  ASSERT_TRUE(child1_value->SetString(BookmarkCodec::kNameKey, title + L"1"));

  std::string dec_checksum;
  scoped_ptr<BookmarkModel> decoded_model1(DecodeHelper(
      *value.get(), enc_checksum, &dec_checksum, true));

  // Undo the change and make sure the checksum is same as original.
  ASSERT_TRUE(child1_value->SetString(BookmarkCodec::kNameKey, title));
  scoped_ptr<BookmarkModel> decoded_model2(DecodeHelper(
      *value.get(), enc_checksum, &dec_checksum, false));
}

TEST_F(BookmarkCodecTest, PersistIDsTest) {
  scoped_ptr<BookmarkModel> model_to_encode(CreateTestModel3());
  BookmarkCodec encoder(true);
  scoped_ptr<Value> model_value(encoder.Encode(model_to_encode.get()));

  BookmarkModel decoded_model(NULL);
  BookmarkCodec decoder(true);
  ASSERT_TRUE(Decode(&decoder, &decoded_model, *model_value.get()));
  BookmarkModelTestUtils::AssertModelsEqual(model_to_encode.get(),
                                            &decoded_model,
                                            true);

  // Add a couple of more items to the decoded bookmark model and make sure
  // ID persistence is working properly.
  const BookmarkNode* bookmark_bar = decoded_model.GetBookmarkBarNode();
  decoded_model.AddURL(
      bookmark_bar, bookmark_bar->GetChildCount(), kUrl3Title, GURL(kUrl3Url));
  const BookmarkNode* group2_node = decoded_model.AddGroup(
      bookmark_bar, bookmark_bar->GetChildCount(), kGroup2Title);
  decoded_model.AddURL(group2_node, 0, kUrl4Title, GURL(kUrl4Url));

  BookmarkCodec encoder2(true);
  scoped_ptr<Value> model_value2(encoder2.Encode(&decoded_model));

  BookmarkModel decoded_model2(NULL);
  BookmarkCodec decoder2(true);
  ASSERT_TRUE(Decode(&decoder2, &decoded_model2, *model_value2.get()));
  BookmarkModelTestUtils::AssertModelsEqual(&decoded_model,
                                            &decoded_model2,
                                            true);
}

class UniqueIDGeneratorTest : public testing::Test {
 protected:
  void TestMixed(UniqueIDGenerator* gen) {
    // Few unique numbers.
    for (int i = 1; i <= 5; ++i) {
      EXPECT_EQ(i, gen->GetUniqueID(i));
    }

    // All numbers from 1 to 5 should produce numbers 6 to 10.
    for (int i = 1; i <= 5; ++i) {
      EXPECT_EQ(5 + i, gen->GetUniqueID(i));
    }

    // 10 should produce 11, then 11 should produce 12, and so on.
    for (int i = 1; i <= 5; ++i) {
      EXPECT_EQ(10 + i, gen->GetUniqueID(9 + i));
    }

    // Any numbers between 1 and 15 should produce a new numbers in sequence.
    EXPECT_EQ(16, gen->GetUniqueID(10));
    EXPECT_EQ(17, gen->GetUniqueID(2));
    EXPECT_EQ(18, gen->GetUniqueID(14));
    EXPECT_EQ(19, gen->GetUniqueID(7));
    EXPECT_EQ(20, gen->GetUniqueID(4));

    // Numbers not yet generated should work.
    EXPECT_EQ(100, gen->GetUniqueID(100));
    EXPECT_EQ(21, gen->GetUniqueID(21));
    EXPECT_EQ(200, gen->GetUniqueID(200));

    // Now any existing number should produce numbers starting from 201.
    EXPECT_EQ(201, gen->GetUniqueID(1));
    EXPECT_EQ(202, gen->GetUniqueID(20));
    EXPECT_EQ(203, gen->GetUniqueID(21));
    EXPECT_EQ(204, gen->GetUniqueID(100));
    EXPECT_EQ(205, gen->GetUniqueID(200));
  }
};

TEST_F(UniqueIDGeneratorTest, SerialNumbersTest) {
  UniqueIDGenerator gen;
  for (int i = 1; i <= 10; ++i) {
    EXPECT_EQ(i, gen.GetUniqueID(i));
  }
}

TEST_F(UniqueIDGeneratorTest, UniqueSortedNumbersTest) {
  UniqueIDGenerator gen;
  for (int i = 1; i <= 10; i += 2) {
    EXPECT_EQ(i, gen.GetUniqueID(i));
  }
}

TEST_F(UniqueIDGeneratorTest, UniqueUnsortedConsecutiveNumbersTest) {
  UniqueIDGenerator gen;
  int numbers[] = {2, 10, 6, 3, 8, 5, 1, 7, 4, 9};
  for (int i = 0; i < ARRAYSIZE(numbers); ++i) {
    EXPECT_EQ(numbers[i], gen.GetUniqueID(numbers[i]));
  }
}

TEST_F(UniqueIDGeneratorTest, UniqueUnsortedNumbersTest) {
  UniqueIDGenerator gen;
  int numbers[] = {20, 100, 60, 30, 80, 50, 10, 70, 40, 90};
  for (int i = 0; i < ARRAYSIZE(numbers); ++i) {
    EXPECT_EQ(numbers[i], gen.GetUniqueID(numbers[i]));
  }
}

TEST_F(UniqueIDGeneratorTest, AllDuplicatesTest) {
  UniqueIDGenerator gen;
  for (int i = 1; i <= 10; ++i) {
    EXPECT_EQ(i, gen.GetUniqueID(1));
  }
}

TEST_F(UniqueIDGeneratorTest, MixedTest) {
  UniqueIDGenerator gen;
  TestMixed(&gen);
}

TEST_F(UniqueIDGeneratorTest, ResetTest) {
  UniqueIDGenerator gen;
  for (int i = 0; i < 5; ++i) {
    TestMixed(&gen);
    gen.Reset();
  }
}
