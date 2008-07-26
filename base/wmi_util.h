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

// WMI (Windows Management and Instrumentation) is a big, complex, COM-based
// API that can be used to perform all sorts of things. Sometimes is the best
// way to accomplish something under windows but its lack of an approachable
// C++ interface prevents its use. This collection of fucntions is a step in
// that direction.
// There are two classes; WMIUtil and WMIProcessUtil. The first
// one contain generic helpers and the second one contains the only
// functionality that is needed right now which is to use WMI to launch a
// process.
// To use any function on this header you must call CoInitialize or
// CoInitializeEx beforehand.
//
// For more information about WMI programming:
// http://msdn2.microsoft.com/en-us/library/aa384642(VS.85).aspx

#ifndef BASE_WMI_UTIL_H__
#define BASE_WMI_UTIL_H__

#include <string>
#include <wbemidl.h>

class WMIUtil {
 public:
  // Creates an instance of the WMI service connected to the local computer and
  // returns its COM interface. If 'set-blanket' is set to true, the basic COM
  // security blanket is applied to the returned interface. This is almost
  // always desirable unless you set the parameter to false and apply a custom
  // COM security blanket.
  // Returns true if succeeded and 'wmi_services': the pointer to the service.
  // When done with the interface you must call Release();
  static bool CreateLocalConnection(bool set_blanket,
                                    IWbemServices** wmi_services);

  // Creates a WMI method using from a WMI class named 'class_name' that
  // contains a method named 'method_name'. Only WMI classes that are CIM
  // classes can be created using this function.
  // Returns true if succeeded and 'class_instance' returns a pointer to the
  // WMI method that you can fill with parameter values using SetParameter.
  // When done with the interface you must call Release();
  static bool CreateClassMethodObject(IWbemServices* wmi_services,
                                      const std::wstring& class_name,
                                      const std::wstring& method_name,
                                      IWbemClassObject** class_instance);

  // Fills a single parameter given an instanced 'class_method'. Returns true
  // if operation succeeded. When all the parameters are set the method can
  // be executed using IWbemServices::ExecMethod().
  static bool SetParameter(IWbemClassObject* class_method,
                           const std::wstring& parameter_name,
                           VARIANT* parameter);
};

// This class contains functionality of the WMI class 'Win32_Process'
// more info: http://msdn2.microsoft.com/en-us/library/aa394372(VS.85).aspx
class WMIProcessUtil {
 public:
  // Creates a new process from 'command_line'. The advantage over CreateProcess
  // is that it allows you to always break out from a Job object that the caller
  // is attached to even if the Job object flags prevent that.
  // Returns true and the process id in process_id if the process is launched
  // successful. False otherwise.
  // Note that a fully qualified path must be specified in most cases unless
  // the program is not in the search path of winmgmt.exe.
  // Processes created this way are children of wmiprvse.exe and run with the
  // caller credentials.
  static bool Launch(const std::wstring& command_line, int* process_id);
};

#endif  // BASE_WMI_UTIL_H__
