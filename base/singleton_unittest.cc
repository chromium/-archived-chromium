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

#include "testing/gtest/include/gtest/gtest.h"
#include "base/singleton_dll_unittest.h"
#include "base/file_util.h"
#include "base/path_service.h"

class SingletonTest : public testing::Test {
 public:
  SingletonTest() {
  }

  virtual void SetUp() {
    module_ = NULL;
    non_leak_called_ = false;
    leaky_called_ = false;
  }

  virtual void TearDown() {
    ASSERT_FALSE(module_);
  }

  static bool IsTestCaseDisabled() {
    // Check if the dll exists beside the executable.
    std::wstring path;
    PathService::Get(base::DIR_EXE, &path);
    file_util::AppendToPath(&path, kLibrary);
    return !file_util::PathExists(path);
  }

 protected:
  void LoadLibrary() {
    ASSERT_FALSE(module_);
    module_ = ::LoadLibrary(kLibrary);
    ASSERT_TRUE(module_ != NULL);
  }

  void FreeLibrary() {
    ASSERT_TRUE(module_ != NULL);
    ASSERT_TRUE(::FreeLibrary(module_));
    module_ = NULL;
  }

  template<typename T>
  void GetProc(const char* function_name, T* function) {
    ASSERT_TRUE(module_ != NULL);
    *function = reinterpret_cast<T>(GetProcAddress(module_, function_name));
    ASSERT_TRUE(*function);
  }

  void VerifiesCallbacks() {
    EXPECT_TRUE(non_leak_called_);
    EXPECT_FALSE(leaky_called_);
    non_leak_called_ = false;
    leaky_called_ = false;
  }

  void VerifiesCallbacksNotCalled() {
    EXPECT_FALSE(non_leak_called_);
    EXPECT_FALSE(leaky_called_);
    non_leak_called_ = false;
    leaky_called_ = false;
  }

  static void WINAPI CallbackNoLeak() {
    non_leak_called_ = true;
  }

  static void WINAPI CallbackLeak() {
    leaky_called_ = true;
  }

 private:
  static const wchar_t* const kLibrary;
  HMODULE module_;
  static bool non_leak_called_;
  static bool leaky_called_;
};

bool SingletonTest::non_leak_called_ = false;
bool SingletonTest::leaky_called_ = false;

const wchar_t* const SingletonTest::kLibrary = L"singleton_dll_unittest.dll";

TEST_F(SingletonTest, Basic) {
  if (IsTestCaseDisabled())
    return;

  int* singleton_int_1;
  int* singleton_int_2;
  int* singleton_int_3;
  int* singleton_int_4;
  int* singleton_int_5;
  CallBackFunc* leaky_singleton;

  LoadLibrary();
  {
    SingletonIntFunc sut1;
    SingletonIntFunc sut2;
    SingletonIntFunc sut3;
    SingletonIntFunc sut4;
    SingletonIntFunc sut5;
    {
      GetProc("SingletonInt1", &sut1);
      singleton_int_1 = sut1();
    }
    // Ensure POD type initialization.
    EXPECT_EQ(*singleton_int_1, 0);
    *singleton_int_1 = 1;

    EXPECT_EQ(singleton_int_1, sut1());
    EXPECT_EQ(*singleton_int_1, 1);

    {
      GetProc("SingletonInt2", &sut2);
      singleton_int_2 = sut2();
    }
    // Same instance that 1.
    EXPECT_EQ(*singleton_int_2, 1);
    EXPECT_EQ(singleton_int_1, singleton_int_2);

    {
      GetProc("SingletonInt3", &sut3);
      singleton_int_3 = sut3();
    }
    // Different instance than 1 and 2.
    EXPECT_EQ(*singleton_int_3, 0);
    EXPECT_NE(singleton_int_1, singleton_int_3);
    *singleton_int_3 = 3;
    EXPECT_EQ(*singleton_int_1, 1);
    EXPECT_EQ(*singleton_int_2, 1);

    {
      GetProc("SingletonInt4", &sut4);
      singleton_int_4 = sut4();
    }
    // Use a lock for creation. Not really tested at length.
    EXPECT_EQ(*singleton_int_4, 0);
    *singleton_int_4 = 4;
    EXPECT_NE(singleton_int_1, singleton_int_4);
    EXPECT_NE(singleton_int_3, singleton_int_4);

    {
      GetProc("SingletonInt5", &sut5);
      singleton_int_5 = sut5();
    }
    // Is default initialized to 5.
    EXPECT_EQ(*singleton_int_5, 5);
    EXPECT_NE(singleton_int_1, singleton_int_5);
    EXPECT_NE(singleton_int_3, singleton_int_5);
    EXPECT_NE(singleton_int_4, singleton_int_5);
#ifdef _DEBUG
    // In release, the optimizer may make both exports use exactly the same
    // code.
    EXPECT_NE(sut1, sut2);
#endif
    EXPECT_NE(sut2, sut3);
    EXPECT_NE(sut3, sut4);
    EXPECT_NE(sut4, sut5);

    LeakySingletonFunc noleak;
    GetProc("SingletonNoLeak", &noleak);
    noleak(&CallbackNoLeak);
    LeakySingletonFunc leak;
    GetProc("SingletonLeak", &leak);
    leak(&CallbackLeak);
    GetLeakySingletonFunc get_leaky;
    GetProc("GetLeakySingleton", &get_leaky);
    leaky_singleton = get_leaky();
    EXPECT_TRUE(leaky_singleton);
  }
  FreeLibrary();

  // Verify that only the expected callback has been called.
  VerifiesCallbacks();
  // Delete the leaky singleton. It is interesting to note that Purify does
  // *not* detect the leak when this call is commented out. :(
  EXPECT_TRUE(CustomAllocTrait<CallBackFunc>::Delete(leaky_singleton));

  LoadLibrary();
  {
    // Verifiy that the variables were reset.
    {
      SingletonIntFunc sut1;
      GetProc("SingletonInt1", &sut1);
      singleton_int_1 = sut1();
      EXPECT_EQ(*singleton_int_1, 0);
    }
    {
      SingletonIntFunc sut5;
      GetProc("SingletonInt5", &sut5);
      singleton_int_5 = sut5();
      EXPECT_EQ(*singleton_int_5, 5);
    }
  }
  // The leaky singleton shouldn't leak since SingletonLeak has not been called.
  FreeLibrary();

  VerifiesCallbacksNotCalled();
}
