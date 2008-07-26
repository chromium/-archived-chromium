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

// This file contains unit tests for ServiceResolverThunk.

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "sandbox/src/resolver.h"
#include "sandbox/src/sandbox_utils.h"
#include "sandbox/src/service_resolver.h"
#include "sandbox/src/wow64.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// This is the concrete resolver used to perform service-call type functions
// inside ntdll.dll.
template<typename T>
class ResolverThunkTest : public T {
 public:
  // The service resolver needs a child process to write to.
  explicit ResolverThunkTest(bool relaxed)
      : T(::GetCurrentProcess(), relaxed) {}

  // Sets the interception target to the desired address.
  void set_target(void* target) {
    fake_target_ = target;
  }

 protected:
  // Overrides Resolver::Init
  virtual NTSTATUS Init(const void* target_module,
                        const void* interceptor_module,
                        const char* target_name,
                        const char* interceptor_name,
                        const void* interceptor_entry_point,
                        void* thunk_storage,
                        uint32 storage_bytes) {
    NTSTATUS ret = STATUS_SUCCESS;
    ret = ResolverThunk::Init(target_module, interceptor_module, target_name,
                              interceptor_name, interceptor_entry_point,
                              thunk_storage, storage_bytes);
    EXPECT_EQ(STATUS_SUCCESS, ret);

    target_ = fake_target_;
    ntdll_base_ = ::GetModuleHandle(L"ntdll.dll");
    return ret;
  };

 private:
  // Holds the address of the fake target.
  void* fake_target_;

  DISALLOW_EVIL_CONSTRUCTORS(ResolverThunkTest);
};

typedef ResolverThunkTest<sandbox::Win2kResolverThunk> Win2kResolverTest;
typedef ResolverThunkTest<sandbox::ServiceResolverThunk> WinXpResolverTest;
typedef ResolverThunkTest<sandbox::Wow64ResolverThunk> Wow64ResolverTest;

NTSTATUS PatchNtdll(const char* function, bool relaxed) {
  HMODULE ntdll_base = ::GetModuleHandle(L"ntdll.dll");
  EXPECT_TRUE(NULL != ntdll_base);

  void* target = ::GetProcAddress(ntdll_base, function);
  EXPECT_TRUE(NULL != target);
  if (NULL == target)
    return STATUS_UNSUCCESSFUL;

  char service[50];
  memcpy(service, target, sizeof(service));
  sandbox::Wow64 WowHelper(NULL, ntdll_base);

  sandbox::ServiceResolverThunk* resolver;
  if (WowHelper.IsWow64())
    resolver = new Wow64ResolverTest(relaxed);
  else if (!sandbox::IsXPSP2OrLater())
    resolver = new Win2kResolverTest(relaxed);
  else
    resolver = new WinXpResolverTest(relaxed);

  static_cast<WinXpResolverTest*>(resolver)->set_target(service);

  // Any pointer will do as an interception_entry_point
  void* function_entry = resolver;
  size_t thunk_size = resolver->GetThunkSize();
  scoped_ptr<char> thunk(new char[thunk_size]);
  uint32 used;

  NTSTATUS ret = resolver->Setup(ntdll_base, NULL, function, NULL,
                                 function_entry, thunk.get(), thunk_size,
                                 &used);
  if (NT_SUCCESS(ret)) {
    EXPECT_EQ(thunk_size, used);
    EXPECT_NE(0, memcmp(service, target, sizeof(service)));

    if (relaxed) {
      // It's already patched, let's patch again.
      ret = resolver->Setup(ntdll_base, NULL, function, NULL, function_entry,
                            thunk.get(), thunk_size, &used);
    }
  }

  delete resolver;

  return ret;
}

TEST(ServiceResolverTest, PatchesServices) {
  NTSTATUS ret = PatchNtdll("NtClose", false);
  EXPECT_EQ(STATUS_SUCCESS, ret) << "NtClose, last error: " << ::GetLastError();

  ret = PatchNtdll("NtCreateFile", false);
  EXPECT_EQ(STATUS_SUCCESS, ret) << "NtCreateFile, last error: " <<
    ::GetLastError();

  ret = PatchNtdll("NtCreateMutant", false);
  EXPECT_EQ(STATUS_SUCCESS, ret) << "NtCreateMutant, last error: " <<
    ::GetLastError();

  ret = PatchNtdll("NtMapViewOfSection", false);
  EXPECT_EQ(STATUS_SUCCESS, ret) << "NtMapViewOfSection, last error: " <<
    ::GetLastError();
}

TEST(ServiceResolverTest, FailsIfNotService) {
  NTSTATUS ret = PatchNtdll("RtlUlongByteSwap", false);
  EXPECT_NE(STATUS_SUCCESS, ret);

  ret = PatchNtdll("LdrLoadDll", false);
  EXPECT_NE(STATUS_SUCCESS, ret);
}

TEST(ServiceResolverTest, PatchesPatchedServices) {
  NTSTATUS ret = PatchNtdll("NtClose", true);
  EXPECT_EQ(STATUS_SUCCESS, ret) << "NtClose, last error: " << ::GetLastError();

  ret = PatchNtdll("NtCreateFile", true);
  EXPECT_EQ(STATUS_SUCCESS, ret) << "NtCreateFile, last error: " <<
    ::GetLastError();

  ret = PatchNtdll("NtCreateMutant", true);
  EXPECT_EQ(STATUS_SUCCESS, ret) << "NtCreateMutant, last error: " <<
    ::GetLastError();

  ret = PatchNtdll("NtMapViewOfSection", true);
  EXPECT_EQ(STATUS_SUCCESS, ret) << "NtMapViewOfSection, last error: " <<
    ::GetLastError();
}

}  // namespace
