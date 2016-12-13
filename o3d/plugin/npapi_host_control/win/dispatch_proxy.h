/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// File declaring a class providing a wrapper between the NPAPI NPObject
// interface, and COM's IDispatchEx interface.

#ifndef O3D_PLUGIN_NPAPI_HOST_CONTROL_WIN_DISPATCH_PROXY_H_
#define O3D_PLUGIN_NPAPI_HOST_CONTROL_WIN_DISPATCH_PROXY_H_

#include <atlctl.h>
#include <dispex.h>

#include "third_party/npapi/files/include/npupp.h"
#include "plugin/npapi_host_control/win/np_browser_proxy.h"

class NPBrowserProxy;

// Class implementing a NPAPI interface wrapper around IDispatchEx automation
// objects.
class DispatchProxy : public NPObject {
 public:
  DispatchProxy(IDispatchEx* dispatch,
                NPBrowserProxy* browser_proxy);
  virtual ~DispatchProxy();

  // Returns the NPAPI interface for accessing the instance of the object.
  static NPClass* GetNPClass() {
    return &kNPClass;
  }

  CComPtr<IDispatchEx> GetDispatchEx() const {
    return dispatch_;
  }

  void SetBrowserProxy(NPBrowserProxy* browser_proxy) {
    browser_proxy_ = browser_proxy;
  }

 private:
  DispatchProxy() : browser_proxy_(NULL) {}

  // COM object of which this is a proxy.
  CComPtr<IDispatchEx> dispatch_;

  // Back pointer to the NPAPI browser environment in which the plugin resides.
  NPBrowserProxy* browser_proxy_;

  // Function to convert NPAPI automation identifiers to COM dispatch ID's.
  // Parameters:
  //    name:  NPAPI identifier for which we are to find the corresponding
  //           dispatch ID.
  //    flags: Flags to pass to GetDispID.
  // Returns:
  //    A valid dispatch-id if the corresponding member or property exists on
  //    the hosted automation object, -1 otherwise.
  DISPID GetDispatchId(NPIdentifier name, DWORD flags) const;

  // The following set of static methods implement the NPAPI object interface
  // for DispatchProxy objects.

  // Function to determine the existance of a scriptable method.
  // Parameters:
  //    header:  'this' for the function call.
  //    name:  NPAPI dispatch identifier associated with the method name.
  // Returns:
  //    true if the method is found.
  static bool HasMethod(NPObject *header, NPIdentifier name);

  // Invokes a scriptable method on the object with the given arguments, and
  // returns a variant result.
  // Parameters:
  //    header:  'this' for the function call.
  //    name:  NPAPI dispatch identifier associated with the method name.
  //    args:  Array of NPVariant objects used as arguments for the method call.
  //    arg_count:  Number of arguments in args.
  //    result:  Pointer to variant receiving the return value of the method.
  // Returns:
  //    true on successfull invocation of an existing method, false otherwise.
  static bool InvokeEntry(NPObject *header, NPIdentifier name,
                          const NPVariant *args, uint32_t arg_count,
                          NPVariant *result);

  // Invokes the object with the given arguments, and returns a variant result.
  // Parameters:
  //    header:  function object to call.
  //    args:  Array of NPVariant objects used as arguments for the method call.
  //    arg_count:  Number of arguments in args.
  //    result:  Pointer to variant receiving the return value of the method.
  // Returns:
  //    true on successfull invocation of an existing method, false otherwise.
  static bool InvokeDefault(NPObject *header, const NPVariant *args,
                            uint32_t argument_count, NPVariant *result);

  // Invokes an object as a constructor function with the given arguments, and
  // returns a variant result.
  // Parameters:
  //    header:  function object to call.
  //    args:  Array of NPVariant objects used as arguments for the method call.
  //    arg_count:  Number of arguments in args.
  //    result:  Pointer to variant receiving the return value of the method.
  // Returns:
  //    true on successfull invocation of an existing method, false otherwise.
  static bool Construct(NPObject *header, const NPVariant *args,
                        uint32_t argument_count, NPVariant *result);

  // Determines the existence of a scriptable property on the object instance.
  // Parameters:
  //    header:  'this' for the function call.
  //    name:  NPAPI dispatch identifier associated with the propertyname.
  // Returns:
  //    true if the property exists.
  static bool HasProperty(NPObject *header, NPIdentifier name);

  // Returns the value of a scriptable property.
  // Parameters:
  //    header:  'this' for the function call.
  //    name:  NPAPI dispatch identifier associated with the propertyname.
  //    variant:  Contains the value of the property upon return.
  // Returns:
  //    true if the property is successfully assigned.
  static bool GetPropertyEntry(NPObject *header, NPIdentifier name,
                               NPVariant *variant);

  // Assigns the value of a scriptable property.
  // Parameters:
  //    header:  'this' for the function call.
  //    name:  NPAPI dispatch identifier associated with the propertyname.
  //    variant:  Value to which the property is to be assigned.
  // Returns:
  //    true if the property is successfully assigned.
  static bool SetPropertyEntry(NPObject *header, NPIdentifier name,
                               const NPVariant *variant);

  // Removes a scriptable property.
  // Parameters:
  //    header:  object to remove property from.
  //    name:  NPAPI dispatch identifier associated with the propertyname.
  // Returns:
  //    true if the property is successfully assigned.
  static bool RemovePropertyEntry(NPObject *header, NPIdentifier name);

  // Returns an enumeration of all properties present on the instance object.
  // Parameters:
  //    header:  'this' for the function call.
  //    value:  Returned pointer to an array of NPidentifers for all of the
  //            properties on the object.
  //    count:  The number of elements in the value array.
  // Returns:
  //    false always.  This routine is not implemented.
  static bool EnumeratePropertyEntries(NPObject *header, NPIdentifier **value,
                                       uint32_t *count);

  // Custom class allocation routine.
  // Parameters:
  //    npp:  The plugin instance data.
  //    class_functions:  The NPClass function table for the class to construct.
  static NPObject * Allocate(NPP npp, NPClass *class_functions);

  // Custom destruction routine.
  static void Deallocate(NPObject *obj);

  // Static V-Table instance for the NPAPI interface for DispatchProxy objects.
  static NPClass kNPClass;

  DISALLOW_COPY_AND_ASSIGN(DispatchProxy);
};

#endif  // O3D_PLUGIN_NPAPI_HOST_CONTROL_WIN_DISPATCH_PROXY_H_
