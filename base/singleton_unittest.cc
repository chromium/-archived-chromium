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

#include "base/at_exit.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/singleton.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ShadowingAtExitManager : public base::AtExitManager {
 public:
  ShadowingAtExitManager() : AtExitManager(true) { }
};

COMPILE_ASSERT(DefaultSingletonTraits<int>::kRegisterAtExit == true, a);

template<typename Type>
struct LockTrait : public DefaultSingletonTraits<Type> {
};

struct Init5Trait : public DefaultSingletonTraits<int> {
  static int* New() {
    return new int(5);
  }
};

typedef void (*CallbackFunc)();

struct CallbackTrait : public DefaultSingletonTraits<CallbackFunc> {
  static void Delete(CallbackFunc* p) {
    if (*p)
      (*p)();
    DefaultSingletonTraits<CallbackFunc>::Delete(p);
  }
};

struct NoLeakTrait : public CallbackTrait {
};

struct LeakTrait : public CallbackTrait {
  static const bool kRegisterAtExit = false;
};

int* SingletonInt1() {
  return Singleton<int>::get();
}

int* SingletonInt2() {
  // Force to use a different singleton than SingletonInt1.
  return Singleton<int, DefaultSingletonTraits<int> >::get();
}

class DummyDifferentiatingClass {
};

int* SingletonInt3() {
  // Force to use a different singleton than SingletonInt1 and SingletonInt2.
  // Note that any type can be used; int, float, std::wstring...
  return Singleton<int, DefaultSingletonTraits<int>,
                   DummyDifferentiatingClass>::get();
}

int* SingletonInt4() {
  return Singleton<int, LockTrait<int> >::get();
}

int* SingletonInt5() {
  return Singleton<int, Init5Trait>::get();
}

void SingletonNoLeak(CallbackFunc CallOnQuit) {
  *Singleton<CallbackFunc, NoLeakTrait>::get() = CallOnQuit;
}

void SingletonLeak(CallbackFunc CallOnQuit) {
  *Singleton<CallbackFunc, LeakTrait>::get() = CallOnQuit;
}

CallbackFunc* GetLeakySingleton() {
  return Singleton<CallbackFunc, LeakTrait>::get();
}

}  // namespace

class SingletonTest : public testing::Test {
 public:
  SingletonTest() { }

  virtual void SetUp() {
    non_leak_called_ = false;
    leaky_called_ = false;
  }

 protected:
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

  static void CallbackNoLeak() {
    non_leak_called_ = true;
  }

  static void CallbackLeak() {
    leaky_called_ = true;
  }

 private:
  static bool non_leak_called_;
  static bool leaky_called_;
};

bool SingletonTest::non_leak_called_ = false;
bool SingletonTest::leaky_called_ = false;

TEST_F(SingletonTest, Basic) {
  int* singleton_int_1;
  int* singleton_int_2;
  int* singleton_int_3;
  int* singleton_int_4;
  int* singleton_int_5;
  CallbackFunc* leaky_singleton;

  {
    ShadowingAtExitManager sem;
    {
      singleton_int_1 = SingletonInt1();
    }
    // Ensure POD type initialization.
    EXPECT_EQ(*singleton_int_1, 0);
    *singleton_int_1 = 1;

    EXPECT_EQ(singleton_int_1, SingletonInt1());
    EXPECT_EQ(*singleton_int_1, 1);

    {
      singleton_int_2 = SingletonInt2();
    }
    // Same instance that 1.
    EXPECT_EQ(*singleton_int_2, 1);
    EXPECT_EQ(singleton_int_1, singleton_int_2);

    {
      singleton_int_3 = SingletonInt3();
    }
    // Different instance than 1 and 2.
    EXPECT_EQ(*singleton_int_3, 0);
    EXPECT_NE(singleton_int_1, singleton_int_3);
    *singleton_int_3 = 3;
    EXPECT_EQ(*singleton_int_1, 1);
    EXPECT_EQ(*singleton_int_2, 1);

    {
      singleton_int_4 = SingletonInt4();
    }
    // Use a lock for creation. Not really tested at length.
    EXPECT_EQ(*singleton_int_4, 0);
    *singleton_int_4 = 4;
    EXPECT_NE(singleton_int_1, singleton_int_4);
    EXPECT_NE(singleton_int_3, singleton_int_4);

    {
      singleton_int_5 = SingletonInt5();
    }
    // Is default initialized to 5.
    EXPECT_EQ(*singleton_int_5, 5);
    EXPECT_NE(singleton_int_1, singleton_int_5);
    EXPECT_NE(singleton_int_3, singleton_int_5);
    EXPECT_NE(singleton_int_4, singleton_int_5);

    SingletonNoLeak(&CallbackNoLeak);
    SingletonLeak(&CallbackLeak);
    leaky_singleton = GetLeakySingleton();
    EXPECT_TRUE(leaky_singleton);
  }

  // Verify that only the expected callback has been called.
  VerifiesCallbacks();
  // Delete the leaky singleton. It is interesting to note that Purify does
  // *not* detect the leak when this call is commented out. :(
  DefaultSingletonTraits<CallbackFunc>::Delete(leaky_singleton);

  {
    ShadowingAtExitManager sem;
    // Verifiy that the variables were reset.
    {
      singleton_int_1 = SingletonInt1();
      EXPECT_EQ(*singleton_int_1, 0);
    }
    {
      singleton_int_5 = SingletonInt5();
      EXPECT_EQ(*singleton_int_5, 5);
    }
  }
  // The leaky singleton shouldn't leak since SingletonLeak has not been called.
  VerifiesCallbacksNotCalled();
}
