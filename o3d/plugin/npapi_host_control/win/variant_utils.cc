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


#include "plugin/npapi_host_control/win/variant_utils.h"
#include "base/scoped_ptr.h"
#include "plugin/npapi_host_control/win/dispatch_proxy.h"

void VariantToNPVariant(NPBrowserProxy* browser_proxy,
                        const VARIANT* source,
                        NPVariant* destination) {
  ATLASSERT(!(source->vt & VT_ARRAY));

  switch (source->vt) {
    case VT_EMPTY:
      VOID_TO_NPVARIANT(*destination);
      break;
    case VT_NULL:
      NULL_TO_NPVARIANT(*destination);
      break;
    case VT_I2:
      INT32_TO_NPVARIANT(source->iVal, *destination);
      break;
    case VT_I4:
      INT32_TO_NPVARIANT(source->intVal, *destination);
      break;
    case VT_R4:
      DOUBLE_TO_NPVARIANT(source->fltVal, *destination);
      break;
    case VT_R8:
      DOUBLE_TO_NPVARIANT(source->dblVal, *destination);
      break;
    case VT_CY:
    case VT_DATE:
      ATLASSERT(false);
      break;
    case VT_BSTR: {
      // BSTR objects may be NULL to indicate an empty string.
      if (source->bstrVal) {
        int required_size = WideCharToMultiByte(CP_UTF8, 0, source->bstrVal,
                                                -1, NULL, 0, NULL, NULL);
        ATLASSERT(required_size != 0);

        char* string_contents = static_cast<char*>(
            browser_proxy->GetBrowserFunctions()->memalloc(required_size));
        WideCharToMultiByte(CP_UTF8, 0, source->bstrVal, -1, string_contents,
                            required_size, NULL, NULL);
        STRINGN_TO_NPVARIANT(string_contents, required_size - 1,
                             *destination);
      } else {
        char* string_contents = static_cast<char*>(
            browser_proxy->GetBrowserFunctions()->memalloc(1));
        string_contents[0] = 0;
        STRINGN_TO_NPVARIANT(string_contents, 0, *destination);
      }
      break;
    }
    case VT_DISPATCH:
      OBJECT_TO_NPVARIANT(browser_proxy->GetNPObject(source->pdispVal),
                          *destination);
      break;
    case VT_ERROR:
      ATLASSERT(false);
      break;
    case VT_BOOL:
      BOOLEAN_TO_NPVARIANT(source->boolVal, *destination);
      break;
    case VT_VARIANT:
    case VT_UNKNOWN:
    case VT_DECIMAL:
      ATLASSERT(false);
      break;
    case VT_I1:
      INT32_TO_NPVARIANT(source->iVal, *destination);
      break;
    case VT_UI1:
      INT32_TO_NPVARIANT(source->iVal, *destination);
      break;
    case VT_UI2:
      INT32_TO_NPVARIANT(source->iVal, *destination);
      break;
    case VT_UI4:
      INT32_TO_NPVARIANT(source->iVal, *destination);
      break;
    case VT_I8:
    case VT_UI8:
      ATLASSERT(false);
      break;
    case VT_INT:
      INT32_TO_NPVARIANT(source->iVal, *destination);
      break;
    case VT_UINT:
      INT32_TO_NPVARIANT(source->iVal, *destination);
      break;
    case VT_VOID:
      VOID_TO_NPVARIANT(*destination);
      break;
    case VT_HRESULT:
    case VT_PTR:
    case VT_SAFEARRAY:
    case VT_CARRAY:
    case VT_USERDEFINED:
    case VT_LPSTR:
    case VT_LPWSTR:
    case VT_RECORD:
    case VT_INT_PTR:
    case VT_UINT_PTR:
    case VT_FILETIME:
    case VT_BLOB:
    case VT_STREAM:
    case VT_STORAGE:
    case VT_STREAMED_OBJECT:
    case VT_STORED_OBJECT:
    case VT_BLOB_OBJECT:
    case VT_CF:
    case VT_CLSID:
    case VT_VERSIONED_STREAM:
    case VT_BSTR_BLOB:
    case VT_VECTOR:
    case VT_ARRAY:
    case VT_BYREF:
    case VT_RESERVED:
    case VT_ILLEGAL:
      ATLASSERT(false);
      break;
    default:
      break;
  }
}

void NPVariantToVariant(NPBrowserProxy* browser_proxy,
                        const NPVariant* source,
                        CComVariant* destination) {
  if (!destination) {
    return;
  }

  switch (source->type) {
    case NPVariantType_Void:
      destination->ChangeType(VT_VOID, NULL);
      break;
    case NPVariantType_Null:
      destination->ChangeType(VT_NULL, NULL);
      break;
    case NPVariantType_Bool:
      *destination = source->value.boolValue;
      break;
    case NPVariantType_Int32:
      *destination = source->value.intValue;
      break;
    case NPVariantType_Double:
      *destination = source->value.doubleValue;
      break;
    case NPVariantType_String: {
      int required_size = 0;
      required_size = MultiByteToWideChar(
          CP_UTF8, 0,
          source->value.stringValue.utf8characters,
          source->value.stringValue.utf8length, NULL, 0);

      scoped_array<wchar_t> wide_value(new wchar_t[required_size + 1]);
      MultiByteToWideChar(
          CP_UTF8, 0,
          source->value.stringValue.utf8characters,
          source->value.stringValue.utf8length, wide_value.get(),
          required_size + 1);
      wide_value[required_size] = 0;

      *destination = wide_value.get();
      break;
    }
    case NPVariantType_Object:
      *destination = browser_proxy->GetDispatchObject(
          source->value.objectValue);
      break;
    default:
      ATLASSERT(false);
  }
}
