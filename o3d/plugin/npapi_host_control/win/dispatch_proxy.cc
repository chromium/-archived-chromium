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


#include "plugin/npapi_host_control/win/dispatch_proxy.h"

#include <atlstr.h>
#include <dispex.h>
#include <oaidl.h>

#include <base/scoped_ptr.h>
#include <algorithm>
#include <vector>

#include "plugin/npapi_host_control/win/np_browser_proxy.h"
#include "plugin/npapi_host_control/win/variant_utils.h"

namespace {

// Helper routine that invokes an IDispatchEx interface with argument values
// provided by NPAPI variant objects.
HRESULT DispatchInvoke(NPBrowserProxy* browser_proxy,
                       IDispatchEx *dispatch,
                       DWORD flags,
                       DISPID member,
                       const NPVariant* arguments,
                       int arg_count,
                       NPVariant* result) {
  // Convert the NPAPI arguments to COM variant objects.
  scoped_array<CComVariant> local_args(new CComVariant[arg_count]);
  for (int x = 0; x < arg_count; ++x) {
    // Note that IDispatch expects arguments in reverse order.
    NPVariantToVariant(browser_proxy,
                       &arguments[x],
                       &local_args[arg_count - x - 1]);
  }

  HRESULT hr = S_OK;
  CComVariant return_arg;
  DISPPARAMS disp_arguments = {0};
  disp_arguments.cArgs = arg_count;
  disp_arguments.rgvarg = local_args.get();
  hr = dispatch->InvokeEx(member, LOCALE_SYSTEM_DEFAULT,
                          flags, &disp_arguments, &return_arg, NULL,
                          NULL);

  // If the invoke succeeded, then convert and store the return argument.
  if (SUCCEEDED(hr)) {
    VariantToNPVariant(browser_proxy, &return_arg, result);
  }

  return hr;
}

}  // unnamed namespace

DispatchProxy::DispatchProxy(IDispatchEx* dispatch,
                             NPBrowserProxy* browser_proxy)
    : dispatch_(dispatch),
      browser_proxy_(browser_proxy) {
  _class = &kNPClass;
  referenceCount = 1;
}

DispatchProxy::~DispatchProxy() {
  ATLASSERT(referenceCount == 0);
  CComPtr<IDispatchEx> dispatchex_object;
  if (browser_proxy_) {
    browser_proxy_->UnregisterDispatchProxy(dispatch_);
  }
}

DISPID DispatchProxy::GetDispatchId(NPIdentifier name, DWORD flags) const {
  DISPID dispatch_id = -1;

  NPUTF8* method_name =
      browser_proxy_->GetBrowserFunctions()->utf8fromidentifier(name);

  // Convert the UTF8-NPAPI string to a wide string.
  int required_size = 0;
  required_size = MultiByteToWideChar(CP_UTF8, 0, method_name,
                                      -1, NULL, 0);

  CComBSTR wide_name(required_size - 1);
  MultiByteToWideChar(CP_UTF8, 0, method_name,
                      -1, (BSTR) wide_name, required_size);
  dispatch_->GetDispID(wide_name, flags, &dispatch_id);

  browser_proxy_->GetBrowserFunctions()->memfree(method_name);

  return dispatch_id;
}

bool DispatchProxy::HasMethod(NPObject *header, NPIdentifier name) {
  DispatchProxy *proxy = static_cast<DispatchProxy*>(header);
  return proxy->GetDispatchId(name, 0) != -1;
}

bool DispatchProxy::InvokeEntry(NPObject *header,
                                NPIdentifier name,
                                const NPVariant *args,
                                uint32_t arg_count,
                                NPVariant *result) {
  DispatchProxy *proxy = static_cast<DispatchProxy*>(header);

  DISPID entry_dispid = proxy->GetDispatchId(name, 0);
  if (entry_dispid == -1) {
    return false;
  }

  HRESULT hr = DispatchInvoke(proxy->browser_proxy_, proxy->dispatch_,
                              DISPATCH_METHOD, entry_dispid, args, arg_count,
                              result);

  return SUCCEEDED(hr);
}

bool DispatchProxy::InvokeDefault(NPObject *header,
                                  const NPVariant *args,
                                  uint32_t arg_count,
                                  NPVariant *result) {
  DispatchProxy *proxy = static_cast<DispatchProxy*>(header);

  HRESULT hr = DispatchInvoke(proxy->browser_proxy_, proxy->dispatch_,
                              DISPATCH_METHOD, DISPID_VALUE, args,
                              arg_count, result);

  return SUCCEEDED(hr);
}

bool DispatchProxy::Construct(NPObject *header,
                              const NPVariant *args,
                              uint32_t arg_count,
                              NPVariant *result) {
  DispatchProxy *proxy = static_cast<DispatchProxy*>(header);

  HRESULT hr = DispatchInvoke(proxy->browser_proxy_, proxy->dispatch_,
                              DISPATCH_CONSTRUCT, DISPID_VALUE, args,
                              arg_count, result);

  return SUCCEEDED(hr);
}

bool DispatchProxy::HasProperty(NPObject *header, NPIdentifier name) {
  DispatchProxy *proxy = static_cast<DispatchProxy*>(header);
  return proxy->GetDispatchId(name, 0) != -1;
}

bool DispatchProxy::GetPropertyEntry(NPObject *header,
                                     NPIdentifier name,
                                     NPVariant *variant) {
  DispatchProxy *proxy = static_cast<DispatchProxy*>(header);
  DISPID dispatch_id = proxy->GetDispatchId(name, 0);
  if (dispatch_id == -1) {
    return false;
  }

  CComVariant result_value;
  DISPPARAMS invoke_args = {0};
  HRESULT hr = proxy->dispatch_->Invoke(dispatch_id, IID_NULL,
                                        LOCALE_SYSTEM_DEFAULT,
                                        DISPATCH_PROPERTYGET,
                                        &invoke_args,
                                        &result_value, NULL, 0);
  if (SUCCEEDED(hr)) {
    VariantToNPVariant(proxy->browser_proxy_, &result_value, variant);
  }

  return SUCCEEDED(hr);
}

bool DispatchProxy::SetPropertyEntry(NPObject *header,
                                     NPIdentifier name,
                                     const NPVariant *variant) {
  DispatchProxy *proxy = static_cast<DispatchProxy*>(header);
  DISPID dispatch_id = proxy->GetDispatchId(name, fdexNameEnsure);

  // Indicate failure if the property does not exist.
  if (dispatch_id == -1) {
    return false;
  }

  CComVariant dispatch_variant;
  NPVariantToVariant(proxy->browser_proxy_, const_cast<NPVariant*>(variant),
                     &dispatch_variant);

  // Prepare the dispatch arguments for the call.  Note that the named
  // argument DISPID_PROPERTYPUT is required.
  DISPID put_id = DISPID_PROPERTYPUT;
  DISPPARAMS invoke_args = {0};
  invoke_args.cArgs = 1;
  invoke_args.rgvarg = &dispatch_variant;
  invoke_args.cNamedArgs = 1;
  invoke_args.rgdispidNamedArgs = &put_id;

  CComVariant return_arg;
  HRESULT hr = proxy->dispatch_->Invoke(dispatch_id, IID_NULL,
                                        LOCALE_SYSTEM_DEFAULT,
                                        DISPATCH_PROPERTYPUT, &invoke_args,
                                        &return_arg, NULL, NULL);

  return SUCCEEDED(hr);
}

bool DispatchProxy::RemovePropertyEntry(NPObject *header,
                                        NPIdentifier name) {
  DispatchProxy *proxy = static_cast<DispatchProxy*>(header);
  DISPID dispatch_id = proxy->GetDispatchId(name, 0);
  if (dispatch_id == -1) {
    return true;
  }

  HRESULT hr = proxy->dispatch_->DeleteMemberByDispID(dispatch_id);

  return SUCCEEDED(hr);
}

bool DispatchProxy::EnumeratePropertyEntries(NPObject *header,
                                             NPIdentifier **result,
                                             uint32_t *count) {
  DispatchProxy *proxy = static_cast<DispatchProxy*>(header);
  *result = NULL;
  *count = 0;

  std::vector<NPIdentifier> np_identifiers;
  DISPID dispatch_id = DISPID_STARTENUM;
  for (;;) {
    HRESULT hr = proxy->dispatch_->GetNextDispID(fdexEnumAll, dispatch_id,
                                                 &dispatch_id);
    if (hr == S_FALSE) {
      *count = np_identifiers.size();
      *result = static_cast<NPIdentifier*>(
          proxy->browser_proxy_->GetBrowserFunctions()->memalloc(
                  np_identifiers.size() * sizeof(NPIdentifier)));
      std::copy(np_identifiers.begin(), np_identifiers.end(), *result);
      return true;
    }

    if (FAILED(hr))
      break;

    CComBSTR name_bstr;
    hr = proxy->dispatch_->GetMemberName(dispatch_id, &name_bstr);
    if (FAILED(hr))
      break;

    CString name_wide((BSTR) name_bstr);

    int utf8_bytes = WideCharToMultiByte(CP_UTF8, 0, name_wide.GetBuffer(),
                                         name_wide.GetLength() + 1,
                                         NULL, 0, NULL, NULL);
    scoped_array<NPUTF8> name_utf8(new NPUTF8[utf8_bytes]);
    WideCharToMultiByte(CP_UTF8, 0, name_wide.GetBuffer(),
                        name_wide.GetLength() + 1,
                        name_utf8.get(), utf8_bytes, NULL, NULL);

    NPIdentifier np_identifier =
        proxy->browser_proxy_->GetBrowserFunctions()->
        getstringidentifier(name_utf8.get());
    np_identifiers.push_back(np_identifier);
  }

  return false;
}

NPObject * DispatchProxy::Allocate(NPP npp, NPClass *aClass) {
  DispatchProxy *instance = new DispatchProxy();
  instance->_class = aClass;
  instance->referenceCount = 1;
  return instance;
}

void DispatchProxy::Deallocate(NPObject *obj) {
  DispatchProxy *proxy = static_cast<DispatchProxy*>(obj);
  ATLASSERT(proxy->referenceCount == 0);
  delete proxy;
}

NPClass DispatchProxy::kNPClass = {
  NP_CLASS_STRUCT_VERSION,
  Allocate,
  Deallocate,
  NULL,
  HasMethod,
  InvokeEntry,
  InvokeDefault,
  HasProperty,
  GetPropertyEntry,
  SetPropertyEntry,
  RemovePropertyEntry,
  EnumeratePropertyEntries,
  Construct
};
