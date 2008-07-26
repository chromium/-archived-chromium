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

#include "sandbox/src/policy_engine_params.h"
#include "sandbox/src/policy_engine_processor.h"
#include "testing/gtest/include/gtest/gtest.h"

#define POLPARAMS_BEGIN(x) sandbox::ParameterSet x[] = {
#define POLPARAM(p) sandbox::ParamPickerMake(p),
#define POLPARAMS_END }

namespace sandbox {

bool SetupNtdllImports();

TEST(PolicyEngineTest, Rules1) {
  SetupNtdllImports();

  // Construct two policy rules that say:
  //
  // #1
  // If the path is c:\\documents and settings\\* AND
  // If the creation mode is 'open existing' AND
  // If the security descriptor is null THEN
  // Ask the broker.
  //
  // #2
  // If the security descriptor is null AND
  // If the path ends with *.txt AND
  // If the creation mode is not 'create new' THEN
  // return Access Denied.

  enum FileCreateArgs {
    FileNameArg,
    CreationDispositionArg,
    FlagsAndAttributesArg,
    SecurityAttributes
  };

  const size_t policy_sz = 1024;
  PolicyBuffer* policy = reinterpret_cast<PolicyBuffer*>(new char[policy_sz]);
  OpcodeFactory opcode_maker(policy, policy_sz - 0x40);

  // Add rule set #1
  opcode_maker.MakeOpWStringMatch(FileNameArg,
                                  L"c:\\documents and settings\\",
                                  0, CASE_INSENSITIVE, kPolNone);
  opcode_maker.MakeOpNumberMatch(CreationDispositionArg, OPEN_EXISTING,
                                 kPolNone);
  opcode_maker.MakeOpVoidPtrMatch(SecurityAttributes, (void*)NULL,
                                 kPolNone);
  opcode_maker.MakeOpAction(ASK_BROKER, kPolNone);

  // Add rule set #2
  opcode_maker.MakeOpWStringMatch(FileNameArg, L".TXT",
                                  kSeekToEnd, CASE_INSENSITIVE, kPolNone);
  opcode_maker.MakeOpNumberMatch(CreationDispositionArg, CREATE_NEW,
                                 kPolNegateEval);
  opcode_maker.MakeOpAction(FAKE_ACCESS_DENIED, kPolNone);
  policy->opcode_count = 7;

  wchar_t* filename = L"c:\\Documents and Settings\\Microsoft\\BLAH.txt";
  unsigned long creation_mode = OPEN_EXISTING;
  unsigned long flags = FILE_ATTRIBUTE_NORMAL;
  void* security_descriptor = NULL;

  POLPARAMS_BEGIN(eval_params)
    POLPARAM(filename)
    POLPARAM(creation_mode)
    POLPARAM(flags)
    POLPARAM(security_descriptor)
  POLPARAMS_END;

  PolicyResult pr;
  PolicyProcessor pol_ev(policy);

  // Test should match the first rule set.
  pr = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, pr);
  EXPECT_EQ(ASK_BROKER, pol_ev.GetAction());

  // Test should still match the first rule set.
  pr = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, pr);
  EXPECT_EQ(ASK_BROKER, pol_ev.GetAction());

  // Changing creation_mode such that evaluation should not match any rule.
  creation_mode = CREATE_NEW;
  pr = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, pr);

  // Changing creation_mode such that evaluation should match rule #2.
  creation_mode = OPEN_ALWAYS;
  pr = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, pr);
  EXPECT_EQ(FAKE_ACCESS_DENIED, pol_ev.GetAction());

  delete [] reinterpret_cast<char*>(policy);
}

}  // namespace sandbox
