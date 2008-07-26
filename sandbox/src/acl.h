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

#ifndef SANDBOX_SRC_ACL_H_
#define SANDBOX_SRC_ACL_H_

#include <windows.h>

#include "base/scoped_ptr.h"
#include "sandbox/src/sid.h"

namespace sandbox {

// Returns the default dacl from the token passed in.
bool GetDefaultDacl(HANDLE token, scoped_ptr<TOKEN_DEFAULT_DACL>* default_dacl);

// Appends an ACE represented by |sid| and |access| to |old_dacl|. If the
// function succeeds, new_dacl contains the new dacl and must be freed using
// LocalFree.
bool AddSidToDacl(const Sid& sid, ACL* old_dacl, ACCESS_MASK access,
                  ACL** new_dacl);

// Adds and ACE represented by |sid| and |access| to the default dacl present
// in the token.
bool AddSidToDefaultDacl(HANDLE token, const Sid& sid, ACCESS_MASK access);

// Adds an ACE represented by the user sid and |access| to the default dacl
// present in the token.
bool AddUserSidToDefaultDacl(HANDLE token, ACCESS_MASK access);

// Adds an ACE represented by |known_sid| and |access| to the dacl of the kernel
// object referenced by |object|.
bool AddKnownSidToKernelObject(HANDLE object, const Sid& sid,
                               ACCESS_MASK access);

}  // namespace sandbox


#endif  // SANDBOX_SRC_ACL_H
