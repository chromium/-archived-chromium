// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"

#include "webkit/glue/cpp_variant.h"
#include "testing/gtest/include/gtest/gtest.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "npruntime_priv.h"  // for NPN_InitializeVariantWithStringCopy
MSVC_POP_WARNING();

#if USE(JSC)
#define _NPN_InitializeVariantWithStringCopy NPN_InitializeVariantWithStringCopy
#endif

// Creates a std::string from an NPVariant of string type.  If the NPVariant
// is not a string, empties the std::string.
void MakeStdString(const NPVariant& np, std::string* std_string) {
  if (np.type == NPVariantType_String) {
    const char* chars =
        reinterpret_cast<const char*>(np.value.stringValue.UTF8Characters);
    (*std_string).assign(chars, np.value.stringValue.UTF8Length);
  } else {
    (*std_string).clear();
  }
}

// Verifies that the actual NPVariant is a string and that its value matches
// the expected_str.
void CheckString(const std::string& expected_str, const NPVariant& actual) {
  EXPECT_EQ(NPVariantType_String, actual.type);
  std::string actual_str;
  MakeStdString(actual, &actual_str);
  EXPECT_EQ(expected_str, actual_str);
}

// Verifies that both the actual and the expected NPVariants are strings and
// that their values match.
void CheckString(const NPVariant& expected, const NPVariant& actual) {
  EXPECT_EQ(NPVariantType_String, expected.type);
  std::string expected_str;
  MakeStdString(expected, &expected_str);
  CheckString(expected_str, actual);
}

int g_allocate_call_count = 0;
int g_deallocate_call_count = 0;

void CheckObject(const NPVariant& actual) {
  EXPECT_EQ(NPVariantType_Object, actual.type);
  EXPECT_TRUE(actual.value.objectValue);
  EXPECT_EQ(1U, actual.value.objectValue->referenceCount);
  EXPECT_EQ(1, g_allocate_call_count);
  EXPECT_EQ(0, g_deallocate_call_count);
}

NPObject* MockNPAllocate(NPP npp, NPClass* aClass) {
  // This is a mock allocate method that mimics the behavior
  // of NPN_CreateObject when allocate() is NULL

  ++g_allocate_call_count;
  // Ignore npp and NPClass
  return reinterpret_cast<NPObject*>(malloc(sizeof(NPObject)));
}

void MockNPDeallocate(NPObject* npobj) {
  // This is a mock deallocate method that mimics the behavior
  // of NPN_DeallocateObject when deallocate() is NULL

  ++g_deallocate_call_count;
  free(npobj);
}

static NPClass void_class = { NP_CLASS_STRUCT_VERSION,
                              MockNPAllocate,
                              MockNPDeallocate,
                              0, 0, 0, 0, 0, 0, 0, 0, 0 };

NPObject* MakeVoidObject() {
  g_allocate_call_count = 0;
  g_deallocate_call_count = 0;
  return NPN_CreateObject(NULL, &void_class);
}

TEST(CppVariantTest, NewVariantHasNullType) {
  CppVariant value;
  EXPECT_EQ(NPVariantType_Null, value.type);
}

TEST(CppVariantTest, SetNullSetsType) {
  CppVariant value;
  value.Set(17);
  value.SetNull();
  EXPECT_EQ(NPVariantType_Null, value.type);
}

TEST(CppVariantTest, CopyConstructorDoesDeepCopy) {
  CppVariant source;
  source.Set("test string");
  CppVariant dest = source;
  EXPECT_EQ(NPVariantType_String, dest.type);
  EXPECT_EQ(NPVariantType_String, source.type);

  // Ensure that the string was copied, not just the pointer.
  EXPECT_NE(source.value.stringValue.UTF8Characters,
    dest.value.stringValue.UTF8Characters);

  CheckString(source, dest);
}

TEST(CppVariantTest, CopyConstructorIncrementsRefCount) {
  CppVariant source;
  NPObject *object = MakeVoidObject();
  source.Set(object);
  // 2 references so far.
  EXPECT_EQ(2U, source.value.objectValue->referenceCount);

  CppVariant dest = source;
  EXPECT_EQ(3U, dest.value.objectValue->referenceCount);
  EXPECT_EQ(1, g_allocate_call_count);
  NPN_ReleaseObject(object);
  source.SetNull();
  CheckObject(dest);
}

TEST(CppVariantTest, AssignmentDoesDeepCopy) {
  CppVariant source;
  source.Set("test string");
  CppVariant dest;
  dest = source;
  EXPECT_EQ(NPVariantType_String, dest.type);
  EXPECT_EQ(NPVariantType_String, source.type);

  // Ensure that the string was copied, not just the pointer.
  EXPECT_NE(source.value.stringValue.UTF8Characters,
    dest.value.stringValue.UTF8Characters);

  CheckString(source, dest);
}

TEST(CppVariantTest, AssignmentIncrementsRefCount) {
  CppVariant source;
  NPObject *object = MakeVoidObject();
  source.Set(object);
  // 2 references so far.
  EXPECT_EQ(2U, source.value.objectValue->referenceCount);

  CppVariant dest;
  dest = source;
  EXPECT_EQ(3U, dest.value.objectValue->referenceCount);
  EXPECT_EQ(1, g_allocate_call_count);

  NPN_ReleaseObject(object);
  source.SetNull();
  CheckObject(dest);
}

TEST(CppVariantTest, DestroyingCopyDoesNotCorruptSource) {
  CppVariant source;
  source.Set("test string");
  std::string before;
  MakeStdString(source, &before);
  {
    CppVariant dest = source;
  }
  CheckString(before, source);

  NPObject *object = MakeVoidObject();
  source.Set(object);
  {
    CppVariant dest2 = source;
  }
  NPN_ReleaseObject(object);
  CheckObject(source);
}

TEST(CppVariantTest, CopiesTypeAndValueToNPVariant) {
  NPVariant np;
  CppVariant cpp;

  cpp.Set(true);
  cpp.CopyToNPVariant(&np);
  EXPECT_EQ(cpp.type, np.type);
  EXPECT_EQ(cpp.value.boolValue, np.value.boolValue);
  NPN_ReleaseVariantValue(&np);

  cpp.Set(17);
  cpp.CopyToNPVariant(&np);
  EXPECT_EQ(cpp.type, np.type);
  EXPECT_EQ(cpp.value.intValue, np.value.intValue);
  NPN_ReleaseVariantValue(&np);

  cpp.Set(3.1415);
  cpp.CopyToNPVariant(&np);
  EXPECT_EQ(cpp.type, np.type);
  EXPECT_EQ(cpp.value.doubleValue, np.value.doubleValue);
  NPN_ReleaseVariantValue(&np);

  cpp.Set("test value");
  cpp.CopyToNPVariant(&np);
  CheckString("test value", np);
  NPN_ReleaseVariantValue(&np);

  cpp.SetNull();
  cpp.CopyToNPVariant(&np);
  EXPECT_EQ(cpp.type, np.type);
  NPN_ReleaseVariantValue(&np);

  NPObject *object = MakeVoidObject();
  cpp.Set(object);
  cpp.CopyToNPVariant(&np);
  NPN_ReleaseObject(object);
  cpp.SetNull();
  CheckObject(np);
  NPN_ReleaseVariantValue(&np);
}

TEST(CppVariantTest, SetsTypeAndValueFromNPVariant) {
  NPVariant np;
  CppVariant cpp;

  VOID_TO_NPVARIANT(np);
  cpp.Set(np);
  EXPECT_EQ(np.type, cpp.type);
  NPN_ReleaseVariantValue(&np);

  NULL_TO_NPVARIANT(np);
  cpp.Set(np);
  EXPECT_EQ(np.type, cpp.type);
  NPN_ReleaseVariantValue(&np);

  BOOLEAN_TO_NPVARIANT(true, np);
  cpp.Set(np);
  EXPECT_EQ(np.type, cpp.type);
  EXPECT_EQ(np.value.boolValue, cpp.value.boolValue);
  NPN_ReleaseVariantValue(&np);

  INT32_TO_NPVARIANT(15, np);
  cpp.Set(np);
  EXPECT_EQ(np.type, cpp.type);
  EXPECT_EQ(np.value.intValue, cpp.value.intValue);
  NPN_ReleaseVariantValue(&np);

  DOUBLE_TO_NPVARIANT(2.71828, np);
  cpp.Set(np);
  EXPECT_EQ(np.type, cpp.type);
  EXPECT_EQ(np.value.doubleValue, cpp.value.doubleValue);
  NPN_ReleaseVariantValue(&np);

  NPString np_ascii_str = { "1st test value",
                            static_cast<uint32_t>(strlen("1st test value")) };
  _NPN_InitializeVariantWithStringCopy(&np, &np_ascii_str);
  cpp.Set(np);
  CheckString("1st test value", cpp);
  NPN_ReleaseVariantValue(&np);

  // Test characters represented in 2/3/4 bytes in UTF-8
  // Greek alpha, Chinese number 1 (horizontal bar),
  // Deseret letter (similar to 'O')
  NPString np_intl_str = { "\xce\xb1\xe4\xb8\x80\xf0\x90\x90\x84",
                           static_cast<uint32_t>(strlen(
                               "\xce\xb1\xe4\xb8\x80\xf0\x90\x90\x84")) };
  _NPN_InitializeVariantWithStringCopy(&np, &np_intl_str);
  cpp.Set(np);
  CheckString("\xce\xb1\xe4\xb8\x80\xf0\x90\x90\x84", cpp);
  NPN_ReleaseVariantValue(&np);

  NPObject *obj = MakeVoidObject();
  OBJECT_TO_NPVARIANT(obj, np);  // Doesn't make a copy.
  cpp.Set(np);
  NPN_ReleaseVariantValue(&np);  // or NPN_ReleaseObject but NOT both
  CheckObject(cpp);
}

TEST(CppVariantTest, SetsSimpleTypesAndValues) {
  CppVariant cpp;
  cpp.Set(true);
  EXPECT_EQ(NPVariantType_Bool, cpp.type);
  EXPECT_EQ(true, cpp.value.boolValue);

  cpp.Set(5);
  EXPECT_EQ(NPVariantType_Int32, cpp.type);
  EXPECT_EQ(5, cpp.value.intValue);

  cpp.Set(1.234);
  EXPECT_EQ(NPVariantType_Double, cpp.type);
  EXPECT_EQ(1.234, cpp.value.doubleValue);

  // C string
  cpp.Set("1st test string");
  CheckString("1st test string", cpp);

  // std::string
  std::string source("std test string");
  cpp.Set(source);
  CheckString("std test string", cpp);

  // NPString
  NPString np_ascii_str = { "test NPString",
                            static_cast<uint32_t>(strlen("test NPString")) };
  cpp.Set(np_ascii_str);
  std::string expected("test NPString");
  CheckString(expected, cpp);

  // Test characters represented in 2/3/4 bytes in UTF-8
  // Greek alpha, Chinese number 1 (horizontal bar),
  // Deseret letter (similar to 'O')
  NPString np_intl_str = { "\xce\xb1\xe4\xb8\x80\xf0\x90\x90\x84",
                           static_cast<uint32_t>(strlen(
                               "\xce\xb1\xe4\xb8\x80\xf0\x90\x90\x84")) };
  cpp.Set(np_intl_str);
  expected = std::string("\xce\xb1\xe4\xb8\x80\xf0\x90\x90\x84");
  CheckString(expected, cpp);

  NPObject* obj = MakeVoidObject();
  cpp.Set(obj);
  NPN_ReleaseObject(obj);
  CheckObject(cpp);
}

TEST(CppVariantTest, FreeDataSetsToVoid) {
  CppVariant cpp;
  EXPECT_EQ(NPVariantType_Null, cpp.type);
  cpp.Set(12);
  EXPECT_EQ(NPVariantType_Int32, cpp.type);
  cpp.FreeData();
  EXPECT_EQ(NPVariantType_Void, cpp.type);
}

TEST(CppVariantTest, FreeDataReleasesObject) {
  CppVariant cpp;
  NPObject* object = MakeVoidObject();
  cpp.Set(object);
  EXPECT_EQ(2U, object->referenceCount);
  cpp.FreeData();
  EXPECT_EQ(1U, object->referenceCount);
  EXPECT_EQ(0, g_deallocate_call_count);

  cpp.Set(object);
  NPN_ReleaseObject(object);
  EXPECT_EQ(0, g_deallocate_call_count);
  cpp.FreeData();
  EXPECT_EQ(1, g_deallocate_call_count);
}

TEST(CppVariantTest, IsTypeFunctionsWork) {
  CppVariant cpp;
  // These should not happen in practice, since voids are not supported
  // This test must be first since it just clobbers internal data without
  // releasing.
  VOID_TO_NPVARIANT(cpp);
  EXPECT_FALSE(cpp.isBool());
  EXPECT_FALSE(cpp.isInt32());
  EXPECT_FALSE(cpp.isDouble());
  EXPECT_FALSE(cpp.isNumber());
  EXPECT_FALSE(cpp.isString());
  EXPECT_TRUE(cpp.isVoid());
  EXPECT_FALSE(cpp.isNull());
  EXPECT_TRUE(cpp.isEmpty());

  cpp.Set(true);
  EXPECT_TRUE(cpp.isBool());
  EXPECT_FALSE(cpp.isInt32());
  EXPECT_FALSE(cpp.isDouble());
  EXPECT_FALSE(cpp.isNumber());
  EXPECT_FALSE(cpp.isString());
  EXPECT_FALSE(cpp.isVoid());
  EXPECT_FALSE(cpp.isNull());
  EXPECT_FALSE(cpp.isEmpty());
  EXPECT_FALSE(cpp.isObject());

  cpp.Set(12);
  EXPECT_FALSE(cpp.isBool());
  EXPECT_TRUE(cpp.isInt32());
  EXPECT_FALSE(cpp.isDouble());
  EXPECT_TRUE(cpp.isNumber());
  EXPECT_FALSE(cpp.isString());
  EXPECT_FALSE(cpp.isVoid());
  EXPECT_FALSE(cpp.isNull());
  EXPECT_FALSE(cpp.isEmpty());
  EXPECT_FALSE(cpp.isObject());

  cpp.Set(3.1415);
  EXPECT_FALSE(cpp.isBool());
  EXPECT_FALSE(cpp.isInt32());
  EXPECT_TRUE(cpp.isDouble());
  EXPECT_TRUE(cpp.isNumber());
  EXPECT_FALSE(cpp.isString());
  EXPECT_FALSE(cpp.isVoid());
  EXPECT_FALSE(cpp.isNull());
  EXPECT_FALSE(cpp.isEmpty());
  EXPECT_FALSE(cpp.isObject());

  cpp.Set("a string");
  EXPECT_FALSE(cpp.isBool());
  EXPECT_FALSE(cpp.isInt32());
  EXPECT_FALSE(cpp.isDouble());
  EXPECT_FALSE(cpp.isNumber());
  EXPECT_TRUE(cpp.isString());
  EXPECT_FALSE(cpp.isVoid());
  EXPECT_FALSE(cpp.isNull());
  EXPECT_FALSE(cpp.isEmpty());
  EXPECT_FALSE(cpp.isObject());

  cpp.SetNull();
  EXPECT_FALSE(cpp.isBool());
  EXPECT_FALSE(cpp.isInt32());
  EXPECT_FALSE(cpp.isDouble());
  EXPECT_FALSE(cpp.isNumber());
  EXPECT_FALSE(cpp.isString());
  EXPECT_FALSE(cpp.isVoid());
  EXPECT_TRUE(cpp.isNull());
  EXPECT_TRUE(cpp.isEmpty());
  EXPECT_FALSE(cpp.isObject());

  NPObject *obj = MakeVoidObject();
  cpp.Set(obj);
  EXPECT_FALSE(cpp.isBool());
  EXPECT_FALSE(cpp.isInt32());
  EXPECT_FALSE(cpp.isDouble());
  EXPECT_FALSE(cpp.isNumber());
  EXPECT_FALSE(cpp.isString());
  EXPECT_FALSE(cpp.isVoid());
  EXPECT_FALSE(cpp.isNull());
  EXPECT_FALSE(cpp.isEmpty());
  EXPECT_TRUE(cpp.isObject());
  NPN_ReleaseObject(obj);
  CheckObject(cpp);
}

