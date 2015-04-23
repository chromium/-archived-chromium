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


#include <oaidl.h>
#include <atlstr.h>
#include <vector>

#include "base/scoped_ptr.h"
#include "plugin/npapi_host_control/win/np_object_proxy.h"
#include "plugin/npapi_host_control/win/np_browser_proxy.h"
#include "plugin/npapi_host_control/win/variant_utils.h"
#include "third_party/npapi/files/include/npupp.h"

namespace {

// Helper routine copying NPVariant object to a destination COM Variant.
// Both arguments may be null.  Argument com_result must be a properly
// initialized VARIANT instance.
void CopyToCOMResult(NPBrowserProxy* browser_proxy,
                     NPVariant* np_result,
                     VARIANT* com_result) {
  if (com_result && np_result) {
    CComVariant intermediate_result;
    NPVariantToVariant(browser_proxy, np_result, &intermediate_result);
    VariantCopy(com_result, &intermediate_result);
  }
}

}  // unnamed namespace


NPObjectProxy::NPObjectProxy()
    : hosted_(NULL),
      browser_proxy_(NULL) {
}

NPObjectProxy::~NPObjectProxy() {
  if (hosted_) {
    if (browser_proxy_) {
      browser_proxy_->UnregisterNPObjectProxy(hosted_);
    }
    NPBrowserProxy::GetBrowserFunctions()->releaseobject(hosted_);
  }
}

STDMETHODIMP NPObjectProxy::SetHostedObject(void* hosted) {
  ATLASSERT(hosted);
  if (hosted_) {
    NPBrowserProxy::GetBrowserFunctions()->releaseobject(hosted_);
  }
  hosted_ = static_cast<NPObject*>(hosted);
  NPBrowserProxy::GetBrowserFunctions()->retainobject(hosted_);
  return S_OK;
}

STDMETHODIMP NPObjectProxy::ReleaseHosted() {
  if (hosted_) {
    NPBrowserProxy::GetBrowserFunctions()->releaseobject(hosted_);
    hosted_ = NULL;
  }
  return S_OK;
}

STDMETHODIMP NPObjectProxy::GetTypeInfoCount(UINT* pctinfo) {
  // This class does not support type info.
  if (!pctinfo) {
    return E_POINTER;
  } else {
    *pctinfo = 0;
  }
  return S_OK;
}

STDMETHODIMP NPObjectProxy::GetTypeInfo(UINT itinfo,
                                        LCID lcid,
                                        ITypeInfo** pptinfo) {
  // This class does not support type info.
  return E_NOTIMPL;
}

STDMETHODIMP NPObjectProxy::GetIDsOfNames(REFIID riid,
                                          LPOLESTR* rgszNames,
                                          UINT cNames,
                                          LCID lcid,
                                          DISPID* rgdispid) {
  if (!hosted_) {
    return E_FAIL;
  }

  // Use the ids from the plugin to return dispatch ids
  NPIdentifier *supported_ids = NULL;
  uint32 id_count = 0;

  if (!hosted_->_class->enumerate(hosted_, &supported_ids, &id_count)) {
    return E_FAIL;
  }

  // Convert all of the wide string arguments to UTF-8.
  scoped_array<char *> utf8_names(new char*[cNames]);
  for (int x = 0; x < cNames; ++x) {
    size_t name_length = wcstombs(NULL, rgszNames[x], 0) + 1;
    utf8_names[x] = new char[name_length];
    wcstombs(utf8_names[x], rgszNames[x], name_length);
    rgdispid[x] = DISPID_UNKNOWN;
  }

  int ids_found = 0;
  // For each string in the input arguments, look for a match in the set of
  // ids supported by the object instance.
  NPUTF8 *string_id = NULL;
  for (int x = 0; x < id_count; ++x) {
    string_id = NPBrowserProxy::GetBrowserFunctions()->
                utf8fromidentifier(supported_ids[x]);
    ATLASSERT(string_id);
    for (int y = 0; y < cNames; ++y) {
      if (strcmp(utf8_names[y], string_id) == 0) {
        // Return the ADDRESS of the supported ids string as the DISPID
        // for the method.
        rgdispid[y] = reinterpret_cast<DISPID>(supported_ids[x]);
        ++ids_found;
        break;
      }
    }
    NPBrowserProxy::GetBrowserFunctions()->memfree(string_id);
  }

  // Free all intermediate string resources.
  for (int x = 0; x < cNames; ++x) {
    delete[] utf8_names[x];
  }
  NPBrowserProxy::GetBrowserFunctions()->memfree(supported_ids);

  return (ids_found == cNames) ? S_OK : DISP_E_UNKNOWNNAME;
}

STDMETHODIMP NPObjectProxy::Invoke(DISPID dispidMember,
                                   REFIID riid,
                                   LCID lcid,
                                   WORD wFlags,
                                   DISPPARAMS* pdispparams,
                                   VARIANT* pvarResult,
                                   EXCEPINFO* pexcepinfo,
                                   UINT* puArgErr) {
  if (!hosted_) {
    return E_FAIL;
  }

  return InvokeEx(dispidMember, lcid, wFlags, pdispparams, pvarResult,
                  pexcepinfo, NULL);
}

STDMETHODIMP NPObjectProxy::DeleteMemberByDispID(DISPID id) {
  if (!hosted_) {
    return E_FAIL;
  }

  NPIdentifier np_identifier = reinterpret_cast<NPIdentifier>(id);
  if (hosted_->_class->removeProperty != NULL &&
      hosted_->_class->removeProperty(hosted_, np_identifier)) {
    return S_OK;
  }
  return S_FALSE;
}

STDMETHODIMP NPObjectProxy::DeleteMemberByName(BSTR bstrName, DWORD grfdex) {
  if (!hosted_) {
    return E_FAIL;
  }
  DISPID id;
  HRESULT hr = GetDispID(bstrName, grfdex, &id);
  if (hr == DISP_E_UNKNOWNNAME) {
    // The semantics of JavaScript are that deleting a property that does not
    // exist succeeds.
    return S_OK;
  } else if (FAILED(hr)) {
    // Otherwise fail.
    return S_FALSE;
  } else {
    return DeleteMemberByDispID(id);
  }
}

STDMETHODIMP NPObjectProxy::GetDispID(BSTR bstrName,
                                      DWORD grfdex,
                                      DISPID* pid) {
  if (!hosted_) {
    return E_FAIL;
  }

  *pid = NULL;
  CString name(bstrName);
  int num_utf8_bytes = WideCharToMultiByte(CP_UTF8, 0, name.GetBuffer(),
                                           name.GetLength() + 1, NULL, 0,
                                           NULL, NULL);
  std::vector<NPUTF8> utf8_name(num_utf8_bytes);
  WideCharToMultiByte(CP_UTF8, 0, name.GetBuffer(), name.GetLength() + 1,
                      &utf8_name[0], num_utf8_bytes, NULL, NULL);

  NPIdentifier np_identifier =
      NPBrowserProxy::GetBrowserFunctions()->getstringidentifier(
          &utf8_name[0]);

  // This method can be called to determine whether an object has a property
  // with the given name. So check that before converting to an NPIdentifier.
  if (!HasPropertyOrMethod(np_identifier))
    return DISP_E_UNKNOWNNAME;

  *pid = reinterpret_cast<DISPID>(np_identifier);
  return S_OK;
}

STDMETHODIMP NPObjectProxy::GetMemberName(DISPID id,
                                          BSTR* pbstrName) {
  if (!hosted_) {
    return E_FAIL;
  }
  NPIdentifier np_identifier = reinterpret_cast<NPIdentifier>(id);

  // Make sure the id is valid on this object. It might have been deleted since
  // it was returned by GetDispID.
  if (!HasPropertyOrMethod(np_identifier))
    return DISP_E_UNKNOWNNAME;

  NPUTF8* utf8_name = NPBrowserProxy::GetBrowserFunctions()->utf8fromidentifier(
      np_identifier);
  int num_wide_chars = MultiByteToWideChar(CP_UTF8, 0, utf8_name, -1, NULL, 0);
  CString name;
  MultiByteToWideChar(CP_UTF8, 0, utf8_name, -1,
                      name.GetBuffer(num_wide_chars), num_wide_chars);
  name.ReleaseBuffer(num_wide_chars - 1);
  *pbstrName = name.AllocSysString();
  NPBrowserProxy::GetBrowserFunctions()->memfree(utf8_name);
  return S_OK;
}

STDMETHODIMP NPObjectProxy::GetMemberProperties(DISPID id,
                                                DWORD grfdexFetch,
                                                DWORD* pgrfdex) {
  if (!hosted_) {
    return E_FAIL;
  }

  // NPAPI does not provide a way to get all the information this function
  // expects to be returned. This is what IE7 returns for some native objects.
  return E_NOTIMPL;
}

STDMETHODIMP NPObjectProxy::GetNameSpaceParent(IUnknown** punk) {
  if (!hosted_) {
    return E_FAIL;
  }

  // JavaScript does not have namespaces. An alternative would be to return
  // an error code.
  *punk = NULL;
  return S_OK;
}

STDMETHODIMP NPObjectProxy::GetNextDispID(DWORD grfdex,
                                          DISPID id,
                                          DISPID* pid) {
  if (!hosted_) {
    return E_FAIL;
  }

  HRESULT hr = S_FALSE;
  NPIdentifier* ids;
  uint32_t num_ids = 0;
  if (hosted_->_class->enumerate != NULL &&
      hosted_->_class->enumerate(hosted_, &ids, &num_ids)) {
    if (id == DISPID_STARTENUM && num_ids > 0) {
      *pid = reinterpret_cast<DISPID>(ids[0]);
      hr = S_OK;
    } else {
      if (!ids) {
        return S_FALSE;
      }
      uint32_t i;
      for (i = 0; i != num_ids; ++i) {
        if (ids[i] == reinterpret_cast<NPIdentifier>(id))
          break;
      }
      if (i + 1 < num_ids) {
        *pid = reinterpret_cast<DISPID>(ids[i + 1]);
        hr = S_OK;
      }
    }
    NPBrowserProxy::GetBrowserFunctions()->memfree(ids);
  }
  return hr;
}

STDMETHODIMP NPObjectProxy::InvokeEx(DISPID id,
                                     LCID lcid,
                                     WORD wFlags,
                                     DISPPARAMS* pdb,
                                     VARIANT* pVarRes,
                                     EXCEPINFO* pei,
                                     IServiceProvider* pspCaller) {
  if (!hosted_) {
    return E_FAIL;
  }
  HRESULT hr = E_FAIL;
  NPIdentifier np_identifier = reinterpret_cast<NPIdentifier>(id);

  if (wFlags & (DISPATCH_METHOD | DISPATCH_CONSTRUCT)) {
    // Get the "this" pointer if provided or default to the hosted object.
    // We cannot support more general bindings for "this" through npruntime.
    // This might arise if the function is invoked through
    // my_function.call(my_this, args) from JScript.
    if (pdb->cNamedArgs == 1 && pdb->rgdispidNamedArgs[0] == DISPID_THIS) {
      NPVariant np_this_variant;
      VariantToNPVariant(
          browser_proxy_,
          &pdb->rgvarg[0],
          &np_this_variant);
      NPObject* np_this_object = NULL;
      if (NPVARIANT_IS_OBJECT(np_this_variant)) {
        np_this_object = NPVARIANT_TO_OBJECT(np_this_variant);
      }
      NPBrowserProxy::GetBrowserFunctions()->releasevariantvalue(
          &np_this_variant);
      if (np_this_object != hosted_) {
        return E_FAIL;
      }
    } else if (pdb->cNamedArgs != 0) {
      return DISP_E_NONAMEDARGS;
    }

    // Convert the arguments to NPVariants.
    int num_unnamed_arguments = pdb->cArgs - pdb->cNamedArgs;
    scoped_array<NPVariant> np_arguments(new NPVariant[num_unnamed_arguments]);
    for (int x = 0; x < num_unnamed_arguments; ++x) {
      // Note that IDispatch expects arguments in the reverse order.
      VariantToNPVariant(
          browser_proxy_,
          &pdb->rgvarg[pdb->cArgs - x - 1],
          &np_arguments[x]);
    }

    // IDispatch supports the notion of default methods with the DISPID value
    // DISPID_VALUE.
    NPVariant result;
    if (DISPID_VALUE == id) {
      if (wFlags & DISPATCH_CONSTRUCT) {
        if (hosted_->_class->construct != NULL &&
            hosted_->_class->construct(hosted_,
                                       np_arguments.get(),
                                       num_unnamed_arguments,
                                       &result)) {
          CopyToCOMResult(browser_proxy_, &result, pVarRes);
          NPBrowserProxy::GetBrowserFunctions()->releasevariantvalue(&result);
          hr = S_OK;
        }
      } else {
        if (hosted_->_class->invokeDefault != NULL &&
            hosted_->_class->invokeDefault(hosted_,
                                           np_arguments.get(),
                                           num_unnamed_arguments,
                                           &result)) {
          CopyToCOMResult(browser_proxy_, &result, pVarRes);
          NPBrowserProxy::GetBrowserFunctions()->releasevariantvalue(&result);
          hr = S_OK;
        }
      }
    } else if (hosted_->_class->hasMethod != NULL &&
               hosted_->_class->hasMethod(hosted_, np_identifier)) {
      if (hosted_->_class->invoke != NULL &&
          hosted_->_class->invoke(hosted_,
                                  np_identifier,
                                  np_arguments.get(),
                                  num_unnamed_arguments,
                                  &result)) {
        CopyToCOMResult(browser_proxy_, &result, pVarRes);
        NPBrowserProxy::GetBrowserFunctions()->releasevariantvalue(&result);
        hr = S_OK;
      }
    } else if (hosted_->_class->hasProperty != NULL &&
               hosted_->_class->hasProperty(hosted_, np_identifier)) {
      // If the object does not have a method with the given identifier,
      // it may have a property with that id that we can invoke the default
      // method upon.
      NPVariant np_property_variant;
      if (hosted_->_class->getProperty != NULL &&
          hosted_->_class->getProperty(hosted_, np_identifier,
                                       &np_property_variant)) {
        if (NPVARIANT_IS_OBJECT(np_property_variant)) {
          NPObject* np_property_object = NPVARIANT_TO_OBJECT(
              np_property_variant);
          if (np_property_object->_class->invokeDefault != NULL) {
            if (np_property_object->_class->invokeDefault(
                    np_property_object,
                    np_arguments.get(),
                    num_unnamed_arguments,
                    &result)) {
              CopyToCOMResult(browser_proxy_, &result, pVarRes);
              NPBrowserProxy::GetBrowserFunctions()->releasevariantvalue(
                  &result);
              hr = S_OK;
            }
          } else {
            hr = DISP_E_TYPEMISMATCH;
          }
        } else {
          hr = DISP_E_TYPEMISMATCH;
        }
        NPBrowserProxy::GetBrowserFunctions()->
            releasevariantvalue(&np_property_variant);
      }
    } else {
      hr = DISP_E_MEMBERNOTFOUND;
    }

    // Release all of the converted arguments.
    for (int x = 0; x < num_unnamed_arguments; ++x) {
      NPBrowserProxy::GetBrowserFunctions()->releasevariantvalue(
          &np_arguments[x]);
    }
  } else if (wFlags & DISPATCH_PROPERTYPUT) {
    if (pdb->cArgs == 1) {
      if (id == DISPID_VALUE) {
        hr = DISP_E_MEMBERNOTFOUND;
      } else {
        // Convert the COM variant to the corresponding NPVariant.
        NPVariant property_in;
        VariantToNPVariant(browser_proxy_, &pdb->rgvarg[0], &property_in);
        if (hosted_->_class->setProperty != NULL &&
            hosted_->_class->setProperty(hosted_,
                                         np_identifier,
                                         &property_in)) {
          hr = S_OK;
        }
        NPBrowserProxy::GetBrowserFunctions()->releasevariantvalue(
            &property_in);
      }
    } else {
      hr = DISP_E_BADPARAMCOUNT;
    }
  } else if (wFlags & DISPATCH_PROPERTYGET) {
    if (pdb->cArgs == 0) {
      // Sometimes JScript asks an object for its default value. Returning
      // itself appears to be ther right thing to do.
      if (id == DISPID_VALUE) {
        pVarRes->vt = VT_DISPATCH;
        pVarRes->pdispVal = this;
        AddRef();
        hr = S_OK;
      } else {
        NPVariant property_out;
        if (hosted_->_class->hasProperty != NULL &&
            !hosted_->_class->hasProperty(hosted_, np_identifier)) {
          hr = DISP_E_MEMBERNOTFOUND;
        } else if (hosted_->_class->getProperty != NULL &&
                   hosted_->_class->getProperty(hosted_,
                                                np_identifier,
                                                &property_out)) {
          CopyToCOMResult(browser_proxy_, &property_out, pVarRes);
          NPBrowserProxy::GetBrowserFunctions()->releasevariantvalue(
              &property_out);
          hr = S_OK;
        }
      }
    } else {
      hr = DISP_E_BADPARAMCOUNT;
    }
  }
  return hr;
}

STDMETHODIMP NPObjectProxy::GetNPObjectInstance(void **np_instance) {
  if (!hosted_) {
    return E_FAIL;
  }

  *np_instance = hosted_;
  NPBrowserProxy::GetBrowserFunctions()->retainobject(hosted_);
  return S_OK;
}

bool NPObjectProxy::HasPropertyOrMethod(NPIdentifier np_identifier) {
  if (!hosted_) {
    return E_FAIL;
  }

  return (hosted_->_class->hasProperty != NULL &&
          hosted_->_class->hasProperty(hosted_, np_identifier)) ||
      (hosted_->_class->hasMethod != NULL &&
       hosted_->_class->hasMethod(hosted_, np_identifier));
}
