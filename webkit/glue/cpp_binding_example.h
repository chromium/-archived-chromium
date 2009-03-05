// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
  CppBindingExample class:
  This provides an example of how to use the CppBoundClass to create methods
  and properties that can be exposed to JavaScript by an appropriately built
  embedding client. It is also used by the CppBoundClass unit test.

  Typically, a class intended to be bound to JavaScript will define a
  constructor, any methods and properties to be exposed, and optionally a
  destructor.  An embedding client can then bind the class to a JavaScript
  object in a frame's window using the CppBoundClass::BindToJavascript() method,
  generally called from the WebView delegate's WindowObjectCleared().

  Once this class has been bound, say to the name "example", it might be called
  from JavaScript in the following way:

  <script>
    if (window.example) {
      document.writeln(example.echoValue(false));
      document.writeln(example.echoType("Hello world!"));
      document.writeln(example.plus(2, 3.1));

      example.my_value = 15;
      example.my_other_value = 2.1;
      document.writeln(example.plus(example.my_value, example.my_other_value));
    }
  </script>
*/

#ifndef CPP_BINDING_EXAMPLE_H__
#define CPP_BINDING_EXAMPLE_H__

#include "webkit/glue/cpp_bound_class.h"

class CppBindingExample : public CppBoundClass {
 public:
  // The default constructor initializes the property and method lists needed
  // to bind this class to a JS object.
  CppBindingExample();

  //
  // These public member variables and methods implement the methods and
  // properties that will be exposed to JavaScript. If needed, the class could
  // also contain other methods or variables, which will be hidden from JS
  // as long as they're not mapped in the property and method lists created in
  // the constructor.
  //
  // The signatures of any methods to be bound must match
  // CppBoundClass::Callback.
  //

  // Returns the value that was passed in as its first (only) argument.
  void echoValue(const CppArgumentList& args, CppVariant* result);

  // Returns a hard-coded value of the same type (bool, number (double),
  // string, or null) that was passed in as an argument.
  void echoType(const CppArgumentList& args, CppVariant* result);

  // Returns the sum of the (first) two arguments as a double, if they are both
  // numbers (integers or doubles).  Otherwise returns null.
  void plus(const CppArgumentList& args, CppVariant* result);

  // Invoked when a nonexistent method is called on this example object, this
  // prints an error message.
  void fallbackMethod(const CppArgumentList& args, CppVariant* result);

  // These properties will also be exposed to JavaScript.
  CppVariant my_value;
  CppVariant my_other_value;
};

#endif  // CPP_BINDING_EXAMPLE_H__

