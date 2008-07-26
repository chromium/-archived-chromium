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

#ifndef SANDBOX_SRC_POLICY_PARAMS_H__
#define SANDBOX_SRC_POLICY_PARAMS_H__

#include "sandbox/src/policy_engine_params.h"

namespace sandbox {

class ParameterSet;

// Warning: The following macros store the address to the actual variables, in
// other words, the values are not copied.
#define POLPARAMS_BEGIN(type) class type { public: enum Args {
#define POLPARAM(arg) arg,
#define POLPARAMS_END(type) PolParamLast }; }; \
  typedef sandbox::ParameterSet type##Array [type::PolParamLast];

// Policy parameters for file open / create.
POLPARAMS_BEGIN(OpenFile)
  POLPARAM(NAME)
  POLPARAM(BROKER)   // TRUE if called from the broker.
  POLPARAM(ACCESS)
  POLPARAM(OPTIONS)
POLPARAMS_END(OpenFile)

// Policy parameter for name-based policies.
POLPARAMS_BEGIN(FileName)
  POLPARAM(NAME)
  POLPARAM(BROKER)   // TRUE if called from the broker.
POLPARAMS_END(FileName)

COMPILE_ASSERT(OpenFile::NAME == FileName::NAME, to_simplify_fs_policies);
COMPILE_ASSERT(OpenFile::BROKER == FileName::BROKER, to_simplify_fs_policies);

// Policy parameter for name-based policies.
POLPARAMS_BEGIN(NameBased)
  POLPARAM(NAME)
POLPARAMS_END(NameBased)

// Policy parameters for open event.
POLPARAMS_BEGIN(OpenEventParams)
  POLPARAM(NAME)
  POLPARAM(ACCESS)
POLPARAMS_END(OpenEventParams)

// Policy Parameters for reg open / create.
POLPARAMS_BEGIN(OpenKey)
  POLPARAM(NAME)
  POLPARAM(ACCESS)
POLPARAMS_END(OpenKey)

}  // namespace sandbox

#endif  // SANDBOX_SRC_POLICY_PARAMS_H__
