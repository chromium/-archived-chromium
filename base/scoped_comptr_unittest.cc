// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_comptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "chrome/common/win_util.h"

#if defined(OS_WIN)
#include <shlobj.h>

TEST(ScopedComPtrTest, ScopedComPtr) {
  EXPECT_TRUE(memcmp(&scoped_comptr<IUnknown>::iid(), &IID_IUnknown,
                     sizeof(IID)) == 0);

  EXPECT_TRUE(SUCCEEDED(::CoInitialize(NULL)));

  {
    scoped_comptr<IUnknown> unk;
    EXPECT_TRUE(SUCCEEDED(unk.CreateInstance(CLSID_ShellLink)));
    scoped_comptr<IUnknown> unk2;
    unk2.Attach(unk.Detach());
    EXPECT_TRUE(unk == NULL);
    EXPECT_TRUE(unk2 != NULL);

    scoped_comptr<IMalloc> mem_alloc;
    EXPECT_TRUE(SUCCEEDED(CoGetMalloc(1, mem_alloc.Receive())));

    // test scoped_comptr& constructor
    scoped_comptr<IMalloc> copy1(mem_alloc);
    EXPECT_TRUE(copy1.IsSameObject(mem_alloc));
    EXPECT_FALSE(copy1.IsSameObject(unk2));  // unk2 is valid but different
    EXPECT_FALSE(copy1.IsSameObject(unk));  // unk is NULL
    copy1.Release();
    EXPECT_FALSE(copy1.IsSameObject(unk2));  // unk2 is valid, copy1 is not

    // test Interface* constructor
    scoped_comptr<IMalloc> copy2(static_cast<IMalloc*>(mem_alloc));
    EXPECT_TRUE(copy2.IsSameObject(mem_alloc));

    EXPECT_TRUE(SUCCEEDED(unk.QueryFrom(mem_alloc)));
    EXPECT_TRUE(unk != NULL);
    unk.Release();
    EXPECT_TRUE(unk == NULL);
    EXPECT_TRUE(unk.IsSameObject(copy1));  // both are NULL
  }

  ::CoUninitialize();
}

#endif  // defined(OS_WIN)