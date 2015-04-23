// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_comptr_win.h"

#include <shlobj.h>

#include "base/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct Dummy {
  Dummy() : adds(0), releases(0) { }
  void AddRef() { ++adds; }
  void Release() { ++releases; }

  int adds;
  int releases;
};

extern const IID dummy_iid;
const IID dummy_iid = { 0x12345678u, 0x1234u, 0x5678u, 01, 23, 45, 67, 89,
                        01, 23, 45 };

}  // namespace

TEST(ScopedComPtrTest, ScopedComPtr) {
  EXPECT_TRUE(memcmp(&ScopedComPtr<IUnknown>::iid(), &IID_IUnknown,
                     sizeof(IID)) == 0);

  EXPECT_TRUE(SUCCEEDED(::CoInitialize(NULL)));

  {
    ScopedComPtr<IUnknown> unk;
    EXPECT_TRUE(SUCCEEDED(unk.CreateInstance(CLSID_ShellLink)));
    ScopedComPtr<IUnknown> unk2;
    unk2.Attach(unk.Detach());
    EXPECT_TRUE(unk == NULL);
    EXPECT_TRUE(unk2 != NULL);

    ScopedComPtr<IMalloc> mem_alloc;
    EXPECT_TRUE(SUCCEEDED(CoGetMalloc(1, mem_alloc.Receive())));

    ScopedComPtr<IUnknown> qi_test;
    EXPECT_HRESULT_SUCCEEDED(mem_alloc.QueryInterface(IID_IUnknown,
        reinterpret_cast<void**>(qi_test.Receive())));
    EXPECT_TRUE(qi_test.get() != NULL);
    qi_test.Release();

    // test ScopedComPtr& constructor
    ScopedComPtr<IMalloc> copy1(mem_alloc);
    EXPECT_TRUE(copy1.IsSameObject(mem_alloc));
    EXPECT_FALSE(copy1.IsSameObject(unk2));  // unk2 is valid but different
    EXPECT_FALSE(copy1.IsSameObject(unk));  // unk is NULL

    IMalloc* naked_copy = copy1.Detach();
    copy1 = naked_copy;  // Test the =(T*) operator.
    naked_copy->Release();

    copy1.Release();
    EXPECT_FALSE(copy1.IsSameObject(unk2));  // unk2 is valid, copy1 is not

    // test Interface* constructor
    ScopedComPtr<IMalloc> copy2(static_cast<IMalloc*>(mem_alloc));
    EXPECT_TRUE(copy2.IsSameObject(mem_alloc));

    EXPECT_TRUE(SUCCEEDED(unk.QueryFrom(mem_alloc)));
    EXPECT_TRUE(unk != NULL);
    unk.Release();
    EXPECT_TRUE(unk == NULL);
    EXPECT_TRUE(unk.IsSameObject(copy1));  // both are NULL
  }

  ::CoUninitialize();
}

TEST(ScopedComPtrTest, ScopedComPtrVector) {
  // Verify we don't get error C2558.
  typedef ScopedComPtr<Dummy, &dummy_iid> Ptr;
  std::vector<Ptr> bleh;

  scoped_ptr<Dummy> p(new Dummy);
  {
    Ptr p2(p.get());
    EXPECT_EQ(p->adds, 1);
    EXPECT_EQ(p->releases, 0);
    Ptr p3 = p2;
    EXPECT_EQ(p->adds, 2);
    EXPECT_EQ(p->releases, 0);
    p3 = p2;
    EXPECT_EQ(p->adds, 3);
    EXPECT_EQ(p->releases, 1);
    // To avoid hitting a reallocation.
    bleh.reserve(1);
    bleh.push_back(p2);
    EXPECT_EQ(p->adds, 4);
    EXPECT_EQ(p->releases, 1);
    EXPECT_EQ(bleh[0], p.get());
    bleh.pop_back();
    EXPECT_EQ(p->adds, 4);
    EXPECT_EQ(p->releases, 2);
  }
  EXPECT_EQ(p->adds, 4);
  EXPECT_EQ(p->releases, 4);
}
