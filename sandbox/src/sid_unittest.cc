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

// This file contains unit tests for the sid class.

#define _ATL_NO_EXCEPTIONS
#include <atlbase.h>
#include <atlsecurity.h>

#include "sandbox/src/sid.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

// Calls ::EqualSid. This function exists only to simplify the calls to
// ::EqualSid by removing the need to cast the input params.
BOOL EqualSid(const SID *sid1, const SID *sid2) {
  return ::EqualSid(const_cast<SID*>(sid1), const_cast<SID*>(sid2));
}

// Tests the creation if a Sid
TEST(SidTest, Constructors) {
  ATL::CSid sid_world = ATL::Sids::World();
  SID *sid_world_pointer = const_cast<SID*>(sid_world.GetPSID());

  // Check the SID* constructor
  Sid sid_sid_star(sid_world_pointer);
  ASSERT_TRUE(EqualSid(sid_world_pointer, sid_sid_star.GetPSID()));

  // Check the copy constructor
  Sid sid_copy(sid_sid_star);
  ASSERT_TRUE(EqualSid(sid_world_pointer, sid_copy.GetPSID()));

  // Note that the WELL_KNOWN_SID_TYPE constructor is tested in the GetPSID
  // test.
}

// Tests the method GetPSID
TEST(SidTest, GetPSID) {
  // Check for non-null result;
  ASSERT_NE(static_cast<SID*>(NULL), Sid(::WinLocalSid).GetPSID());
  ASSERT_NE(static_cast<SID*>(NULL), Sid(::WinCreatorOwnerSid).GetPSID());
  ASSERT_NE(static_cast<SID*>(NULL), Sid(::WinBatchSid).GetPSID());

  ASSERT_TRUE(EqualSid(Sid(::WinNullSid).GetPSID(),
                       ATL::Sids::Null().GetPSID()));

  ASSERT_TRUE(EqualSid(Sid(::WinWorldSid).GetPSID(),
                       ATL::Sids::World().GetPSID()));

  ASSERT_TRUE(EqualSid(Sid(::WinDialupSid).GetPSID(),
                       ATL::Sids::Dialup().GetPSID()));

  ASSERT_TRUE(EqualSid(Sid(::WinNetworkSid).GetPSID(),
                       ATL::Sids::Network().GetPSID()));

  ASSERT_TRUE(EqualSid(Sid(::WinBuiltinAdministratorsSid).GetPSID(),
                       ATL::Sids::Admins().GetPSID()));

  ASSERT_TRUE(EqualSid(Sid(::WinBuiltinUsersSid).GetPSID(),
                       ATL::Sids::Users().GetPSID()));

  ASSERT_TRUE(EqualSid(Sid(::WinBuiltinGuestsSid).GetPSID(),
                       ATL::Sids::Guests().GetPSID()));

  ASSERT_TRUE(EqualSid(Sid(::WinProxySid).GetPSID(),
                       ATL::Sids::Proxy().GetPSID()));
}

}  // namespace sandbox
