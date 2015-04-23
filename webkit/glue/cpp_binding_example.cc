// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the definition for CppBindingExample, a usage example
// that is not actually used anywhere.  See cpp_binding_example.h.

#include "config.h"

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
