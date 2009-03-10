// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_V8_UNIT_TEST_H_
#define CHROME_TEST_V8_UNIT_TEST_H_

#include <string>

#include "base/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

// A superclass for unit tests that involve running JavaScript.  This class
// sets up V8 context and has methods that make it easy to execute scripts in
// this context as well as call functions in the context.
class V8UnitTest : public testing::Test {
 public:
  V8UnitTest() {}
  virtual void SetUp();

 protected:
  // Executes the given script source in the context.  The specified script
  // name is used when reporting errors.
  virtual void ExecuteScriptInContext(const StringPiece& script_source,
                                      const StringPiece& script_name);

  // Converts a v8::TryCatch into a human readable string.
  virtual std::string ExceptionToString(v8::TryCatch* try_catch);

  // Calls the specified function that resides in the global scope of the
  // context.  If the function throws an exception, FAIL() is called to
  // indicate a unit test failure.  This is useful for executing unit test
  // functions implemented in JavaScript.
  virtual void TestFunction(const std::string& function_name);

  // This method is bound to a global function "log" in the context.
  // Scripts running in the context can call this to print out logging
  // information to the console.
  static v8::Handle<v8::Value> Log(const v8::Arguments& args);

  // Handle scope that is used throughout the life of this class.
  v8::HandleScope handle_scope_;

  // Context for the JavaScript in the test.
  v8::Handle<v8::Context> context_;
};

#endif  // CHROME_TEST_V8_UNIT_TEST_H_
