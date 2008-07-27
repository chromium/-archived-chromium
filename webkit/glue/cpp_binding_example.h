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
