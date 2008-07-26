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

#ifndef SANDBOX_SRC_IPC_TAGS_H__
#define SANDBOX_SRC_IPC_TAGS_H__

namespace sandbox {

enum {
  IPC_UNUSED_TAG = 0,
  IPC_PING1_TAG,  // Takes a cookie in parameters and returns the cookie
                  // multiplied by 2 and the tick_count. Used for testing only.
  IPC_PING2_TAG,  // Takes an in/out cookie in parameters and modify the cookie
                  // to be multiplied by 3. Used for testing only.
  IPC_NTCREATEFILE_TAG,
  IPC_NTOPENFILE_TAG,
  IPC_NTQUERYATTRIBUTESFILE_TAG,
  IPC_NTQUERYFULLATTRIBUTESFILE_TAG,
  IPC_NTSETINFO_RENAME_TAG,
  IPC_CREATENAMEDPIPEW_TAG,
  IPC_NTOPENTHREAD_TAG,
  IPC_NTOPENPROCESS_TAG,
  IPC_NTOPENPROCESSTOKEN_TAG,
  IPC_NTOPENPROCESSTOKENEX_TAG,
  IPC_CREATEPROCESSW_TAG,
  IPC_CREATEEVENT_TAG,
  IPC_OPENEVENT_TAG,
  IPC_NTCREATEKEY_TAG,
  IPC_NTOPENKEY_TAG,
  IPC_LAST_TAG
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_IPC_TAGS_H__
