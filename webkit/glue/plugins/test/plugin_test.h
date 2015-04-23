// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PORT_PLUGINS_TEST_PLUGIN_TEST_H__
#define WEBKIT_PORT_PLUGINS_TEST_PLUGIN_TEST_H__

#include <string>

#include "base/string_util.h"
#include "webkit/glue/plugins/nphostapi.h"
#include "third_party/npapi/bindings/npapi.h"

namespace NPAPIClient {

// A PluginTest represents an instance of the plugin, which in
// our case is a test case.
class PluginTest {
 public:
  // Constructor.
  PluginTest(NPP id, NPNetscapeFuncs *host_functions);

  // Destructor
  virtual ~PluginTest() {}

  //
  // NPAPI Functions
  //
  virtual NPError New(uint16 mode, int16 argc, const char* argn[],
                      const char* argv[], NPSavedData* saved);
  virtual NPError SetWindow(NPWindow* pNPWindow);
  virtual NPError NewStream(NPMIMEType type, NPStream* stream,
                            NPBool seekable, uint16* stype);
  virtual int32   WriteReady(NPStream *stream);
  virtual int32   Write(NPStream *stream, int32 offset, int32 len,
                        void *buffer);
  virtual NPError DestroyStream(NPStream *stream, NPError reason);
  virtual void    StreamAsFile(NPStream* stream, const char* fname);
  virtual void    URLNotify(const char* url, NPReason reason, void* data);
  virtual int16   HandleEvent(void* event);
  // Returns true if the test has not had any errors.
  bool Succeeded() { return test_status_.length() == 0; }

  // Sets an error for the test case.  Appends the msg to the
  // error that will be returned from the test.
  void SetError(const std::string &msg);

  // Expect two string values are equal, and if not, logs an
  // appropriate error about it.
  void ExpectStringLowerCaseEqual(const std::string &val1, const std::string &val2) {
    if (!LowerCaseEqualsASCII(val1, val2.c_str())) {
      std::string err;
      err = "Expected Equal for '";
      err.append(val1);
      err.append("' and '");
      err.append(val2);
      err.append("'");
      SetError(err);
    }
  };

  // Expect two values to not be equal, and if they are
  // logs an appropriate error about it.
  void ExpectAsciiStringNotEqual(const char *val1, const char *val2) {
    if (val1 == val2) {
      std::string err;
      err = "Expected Not Equal for '";
      err.append(val1);
      err.append("' and '");
      err.append(val2);
      err.append("'");
      SetError(err);
    }
  }
  // Expect two integer values are equal, and if not, logs an
  // appropriate error about it.
  void ExpectIntegerEqual(int val1, int val2) {
    if (val1 != val2) {
      std::string err;
      err = "Expected Equal for '";
      err.append(IntToString(val1));
      err.append("' and '");
      err.append(IntToString(val2));
      err.append("'");
      SetError(err);
    }
  }


 protected:
  // Signals to the Test that invoked us that the test is
  // completed.  This is done by forcing the plugin to
  // set a cookie in the browser window, which the test program
  // is waiting for.  Note - because this is done by
  // using javascript, the browser must have the frame
  // setup before the plugin calls this function.  So plugin
  // tests MUST NOT call this function prior to having
  // received the SetWindow() callback from the browser.
  void SignalTestCompleted();

  // Helper function to lookup names in the input array.
  // If the name is found, returns the value, otherwise
  // returns NULL.
  const char *GetArgValue(const char *name, const int16 argc,
                          const char *argn[], const char *argv[]);

  // Access to the list of functions provided
  // by the NPAPI host.
  NPNetscapeFuncs *HostFunctions() { return host_functions_; }

  // The NPP Identifier for this plugin instance.
  NPP id() { return id_; }
  std::string test_id() { return test_id_; }
  std::string test_name() { return test_name_; }

 private:
  NPP                       id_;
  NPNetscapeFuncs *         host_functions_;
  std::string               test_name_;
  std::string               test_id_;
  std::string               test_status_;
};

} // namespace NPAPIClient

#endif  // WEBKIT_PORT_PLUGINS_TEST_PLUGIN_TEST_H__
