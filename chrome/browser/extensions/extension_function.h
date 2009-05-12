// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_H_

#include <string>

#include "base/values.h"
#include "base/scoped_ptr.h"

class ExtensionFunctionDispatcher;
class Profile;

#define EXTENSION_FUNCTION_VALIDATE(test) do { \
    if (!(test)) { \
      bad_message_ = true; \
      return false; \
    } \
  } while (0)

// Base class for an extension function.
// TODO(aa): This will have to become reference counted when we introduce APIs
// that live beyond a single stack frame.
class ExtensionFunction {
 public:
  ExtensionFunction() : bad_message_(false) {}
  virtual ~ExtensionFunction() {}

  void set_dispatcher(ExtensionFunctionDispatcher* dispatcher) {
    dispatcher_ = dispatcher;
  }
  void set_args(Value* args) { args_ = args; }

  void set_request_id(int request_id) { request_id_ = request_id; }
  int request_id() { return request_id_; }

  Value* result() { return result_.get(); }
  const std::string& error() { return error_; }

  void set_has_callback(bool has_callback) { has_callback_ = has_callback; } 
  bool has_callback() { return has_callback_; }

  // Execute the API. Clients should call set_args() and set_callback_id()
  // before calling this method. Derived classes should populate result_ and
  // error_ before returning.
  virtual void Run() = 0;

 protected:
  void SendResponse(bool success);

  std::string extension_id();

  Profile* profile();

  // The arguments to the API. Only non-null if argument were specfied.
  Value* args_;

  // The result of the API. This should be populated by the derived class before
  // Run() returns.
  scoped_ptr<Value> result_;

  // Any detailed error from the API. This should be populated by the derived
  // class before Run() returns.
  std::string error_;

  // Any class that gets a malformed message should set this to true before
  // returning.  The calling renderer process will be killed.
  bool bad_message_;

  // The dispatcher that will service this extension function call.
  ExtensionFunctionDispatcher* dispatcher_;

 private:
  // Id of this request, used to map the response back to the caller.
  int request_id_;

  // True if the js caller provides a callback function to receive the response
  // of this call.
  bool has_callback_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionFunction);
};


// A SyncExtensionFunction is an ExtensionFunction that runs synchronously
// *relative to the browser's UI thread*. Note that this has nothing to do with
// running synchronously relative to the extension process. From the extension
// process's point of view, the function is still asynchronous.
//
// This kind of function is convenient for implementing simple APIs that just
// need to interact with things on the browser UI thread.
class SyncExtensionFunction : public ExtensionFunction {
 public:
  SyncExtensionFunction() {}

  // Derived classes should implement this method to do their work and return
  // success/failure.
  virtual bool RunImpl() = 0;

  virtual void Run() {
    SendResponse(RunImpl());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncExtensionFunction);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_H_
