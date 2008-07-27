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

// This file contains the definition for CppBindingExample, a usage example
// that is not actually used anywhere.  See cpp_binding_example.h.

#include "cpp_binding_example.h"

CppBindingExample::CppBindingExample() {
  // Map properties.  It's recommended, but not required, that the JavaScript
  // names (used as the keys in this map) match the names of the member
  // variables exposed through those names.
  BindProperty("my_value", &my_value);
  BindProperty("my_other_value", &my_other_value);

  // Map methods.  See comment above about names.
  BindMethod("echoValue", &CppBindingExample::echoValue);
  BindMethod("echoType",  &CppBindingExample::echoType);
  BindMethod("plus",      &CppBindingExample::plus);
  
  // The fallback method is called when a nonexistent method is called on an
  // object. If none is specified, calling a nonexistent method causes an
  // exception to be thrown and the JavaScript execution is stopped.
  BindFallbackMethod(&CppBindingExample::fallbackMethod);

  my_value.Set(10);
  my_other_value.Set("Reinitialized!");
}

void CppBindingExample::echoValue(const CppArgumentList& args,
                                  CppVariant* result) {
  if (args.size() < 1) {
    result->SetNull();
    return;
  }
  result->Set(args[0]);
}

void CppBindingExample::echoType(const CppArgumentList& args, 
                                 CppVariant* result) {   
  if (args.size() < 1) {
    result->SetNull();
    return;
  }  
  // Note that if args[0] is a string, the following assignment implicitly
  // makes a copy of that string, which may have an undesirable impact on
  // performance.
  CppVariant arg1 = args[0];
  if (arg1.isBool())
    result->Set(true);
  else if (arg1.isInt32())
    result->Set(7);
  else if (arg1.isDouble())
    result->Set(3.14159);
  else if (arg1.isString())
    result->Set("Success!");
}

void CppBindingExample::plus(const CppArgumentList& args, 
                             CppVariant* result) {
  if (args.size() < 2) {
    result->SetNull();
    return;
  }

  CppVariant arg1 = args[0];
  CppVariant arg2 = args[1];

  if (!arg1.isNumber() || !arg2.isNumber()) {
    result->SetNull();
    return;
  }

  // The value of a CppVariant may be read directly from its NPVariant struct.
  // (However, it should only be set using one of the Set() functions.)
  double sum = 0.;
  if (arg1.isDouble())
    sum += arg1.value.doubleValue;
  else if (arg1.isInt32())
    sum += arg1.value.intValue;

  if (arg2.isDouble())
    sum += arg2.value.doubleValue;
  else if (arg2.isInt32())
    sum += arg2.value.intValue;

  result->Set(sum);
}

void CppBindingExample::fallbackMethod(const CppArgumentList& args,
                                       CppVariant* result) {
  printf("Error: unknown JavaScript method invoked.\n");
}
