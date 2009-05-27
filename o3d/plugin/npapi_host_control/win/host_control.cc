// Copyright 2009, Google Inc.  All rights reserved.
// Portions of this file were adapted from the Mozilla project.
// See https://developer.mozilla.org/en/ActiveX_Control_for_Hosting_Netscape_Plug-ins_in_IE
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adam Lock <adamlock@eircom.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "plugin/npapi_host_control/win/host_control.h"

#include <mshtml.h>

#include "plugin/npapi_host_control/win/np_plugin_proxy.h"
#include "plugin/npapi_host_control/win/np_object_proxy.h"
#include "plugin/npapi_host_control/win/stream_operation.h"

namespace {

// Helper function converting UTF-8 encoded input-string to a NULL-terminated
// wide-character array.
HRESULT ConvertMultiByteToWideChar(const char* input_string,
                                   scoped_array<wchar_t>* wide_string) {
  if (!wide_string || !input_string) {
    return E_INVALIDARG;
  }

  int required_size = 0;
  required_size = MultiByteToWideChar(CP_UTF8, 0, input_string, -1, NULL, 0);
  // Add one extra element so that we may explicitly null-terminate the string.
  wide_string->reset(new wchar_t[required_size + 1]);
  if (!wide_string->get()) {
    return E_FAIL;
  }
  MultiByteToWideChar(CP_UTF8, 0, input_string, -1, wide_string->get(),
                      required_size + 1);
  (*wide_string)[required_size] = 0;
  return S_OK;
}

// Helper function that checks a user agent string for 'MSIE', the indicator for
// Internet Explorer.
bool IsMSIE(const char* user_agent) {
  ATLASSERT(user_agent);
  if (!user_agent) {
    return false;
  }
  static const char* kMSIETag = "MSIE";
  return strstr(user_agent, kMSIETag) != NULL;
}

}  // unnamed namespace

CHostControl::CHostControl()
    : browser_proxy_(NULL),
      plugin_proxy_(NULL),
      embedded_name_(NULL),
      user_agent_(NULL) {
  // Request that this control be windowed.
  m_bWindowOnly = true;
}

CHostControl::~CHostControl() {
}

STDMETHODIMP CHostControl::InterfaceSupportsErrorInfo(REFIID riid) {
  static const IID* arr[] = {
    &IID_IHostControl,
  };

  for (int i = 0; i < sizeof(arr) / sizeof(arr[0]); i++) {
    if (InlineIsEqualGUID(*arr[i], riid)) {
      return S_OK;
    }
  }
  return S_FALSE;
}


const char* CHostControl::GetUserAgent() const {
  // Capture the user agent on the first call.
  if (!user_agent_.get()) {
    CComBSTR user_agent;
    HRESULT hr = E_FAIL;
    if (NULL != static_cast<IOmNavigator*>(navigator_)) {
      hr = navigator_->get_userAgent(&user_agent);
    }
    if (SUCCEEDED(hr)) {
      // Convert to UTF-8 and null terminate the string.
      int size_required = WideCharToMultiByte(CP_UTF8, 0, user_agent, -1,
                                              NULL, NULL, NULL, NULL);
      user_agent_.reset(new char[size_required + 1]);
      WideCharToMultiByte(CP_UTF8, 0, user_agent, -1,
                          user_agent_.get(), size_required + 1, NULL, NULL);
      user_agent_[size_required] = 0;
    }
  }

  return user_agent_.get();
}

LRESULT CHostControl::OnCreate(UINT uMsg,
                               WPARAM wParam,
                               LPARAM lParam,
                               BOOL &bHandled) {
  NPWindow window = {0};
  window.window = m_hWnd;
  CREATESTRUCT *create_struct = reinterpret_cast<CREATESTRUCT*>(lParam);
  window.type = NPWindowTypeWindow;
  window.x = create_struct->x;
  window.y = create_struct->y;
  window.width = create_struct->cx;
  window.height = create_struct->cy;

  // Get the web browser through the site the control is attached to.
  // Note: The control could be running in some other container than IE
  // so code shouldn't expect this function to work all the time.
  service_provider_ = m_spClientSite;
  if (service_provider_ == NULL)
    return -1;

  if (FAILED(service_provider_->QueryService(IID_IWebBrowserApp,
                                             &web_browser_app_))) {
      return -1;
  }

  // Navigate the ActiveX interface hierarchy to the IOmNavigator interface.
  CComPtr<IDispatch> dispatch;
  if (FAILED(web_browser_app_->get_Document(&dispatch))) {
    return -1;
  }

  if (FAILED(dispatch->QueryInterface(&document_dispatch_))) {
    return -1;
  }

  if (FAILED(document_dispatch_->QueryInterface(&html_document2_))) {
    return -1;
  }

  if (FAILED(document_dispatch_->QueryInterface(&html_document3_))) {
    return -1;
  }

  if (FAILED(html_document2_->get_parentWindow(&html_window_))) {
    return -1;
  }

  if (FAILED(html_window_->get_navigator(&navigator_))) {
    return -1;
  }

  // Only permit the control to create an instance of the hosted plug-in iff
  // we are presently running in Internet Explorer.
  if (!IsMSIE(GetUserAgent())) {
    TearDown();
    return -1;
  }

  if (FAILED(html_window_->QueryInterface(&window_dispatch_))) {
    return -1;
  }

  // Construct and cache a moniker for the url of the page where the plugin
  // is hosted.
  CComBSTR url_string;
  if (FAILED(html_document2_->get_URL(&url_string))) {
    return -1;
  }

  if (FAILED(CreateURLMonikerEx(NULL, url_string, &url_moniker_,
                                URL_MK_UNIFORM))) {
      return -1;
  }

  browser_proxy_.reset(new NPBrowserProxy(this, window_dispatch_));
  if (!plugin_proxy_->Init(browser_proxy_.get(),
                           window,
                           plugin_argument_names_,
                           plugin_argument_values_)) {
    browser_proxy_.reset();
    return -1;
  }

  return 0;
}

void CHostControl::TearDown() {
  if (embedded_name_) {
    SysFreeString(embedded_name_);
    embedded_name_ = NULL;
  }

  // Note:  We do not delete the plug-in instance here, because we can
  // re-initialize it on the subsequent WM_CREATE message.
  if (plugin_proxy_.get()) {
    plugin_proxy_->TearDown();
  }

  if (browser_proxy_.get()) {
    browser_proxy_->TearDown();
  }

  browser_proxy_.reset();
  user_agent_.reset();

  url_moniker_ = NULL;
  window_dispatch_ = NULL;
  navigator_ = NULL;
  html_window_ = NULL;
  html_document3_ = NULL;
  html_document2_ = NULL;
  document_dispatch_ = NULL;
  web_browser_app_ = NULL;
  service_provider_.Release();
}

LRESULT CHostControl::OnDestroy(UINT uMsg,
                                WPARAM wParam,
                                LPARAM lParam,
                                BOOL &bHandled) {
  // OnDestroy processing does not imply that the plug-in is to be permanently
  // destroyed - IE will send WM_CREATE, WM_DESTROY message pairs multiple times
  // to the same control instance as it is moved throughout the DOM.
  // We tear-down the object entirely here, so that it can be fully
  // reconstructed, if necessary, on the next WM_CREATE.
  TearDown();

  return 0;
}


HRESULT CHostControl::FinalConstruct() {
  return ConstructPluginProxy();
}


void CHostControl::FinalRelease() {
  plugin_proxy_.reset();
}

HRESULT CHostControl::OpenUrlStream(const wchar_t *url, void *notify_data) {
  return StreamOperation::OpenURL(plugin_proxy_.get(), url, notify_data);
}

STDMETHODIMP CHostControl::GetTypeInfoCount(UINT *pctinfo) {
  if (!pctinfo) {
    return E_POINTER;
  } else {
    *pctinfo = 0;
  }
  return S_OK;
}

STDMETHODIMP CHostControl::GetTypeInfo(UINT itinfo,
                                       LCID lcid,
                                       ITypeInfo **pptinfo) {
  return E_NOTIMPL;
}

STDMETHODIMP CHostControl::GetIDsOfNames(REFIID riid,
                                         LPOLESTR *rgszNames,
                                         UINT cNames,
                                         LCID lcid,
                                         DISPID *rgdispid) {
  // Forward all requests through the typelib before defaulting to the
  // NPAPI plugin.
  HRESULT hr = DispatchImpl::GetIDsOfNames(riid, rgszNames, cNames,
                                           lcid, rgdispid);
  if (SUCCEEDED(hr)) {
    return hr;
  }

  if (plugin_proxy_.get()) {
    CComPtr<INPObjectProxy> script_object;
    hr = plugin_proxy_->GetScriptableObject(&script_object);
    if (SUCCEEDED(hr)) {
      return script_object->GetIDsOfNames(riid, rgszNames, cNames, lcid,
                                          rgdispid);
    } else {
      return hr;
    }
  } else {
    return E_FAIL;
  }
}

STDMETHODIMP CHostControl::Invoke(DISPID dispidMember,
                                  REFIID riid,
                                  LCID lcid,
                                  WORD wFlags,
                                  DISPPARAMS *pdispparams,
                                  VARIANT *pvarResult,
                                  EXCEPINFO *pexcepinfo,
                                  UINT *puArgErr) {
  // Forward all Invoke requests through the typelib first.
  HRESULT hr = DispatchImpl::Invoke(dispidMember, riid, lcid, wFlags,
                                    pdispparams, pvarResult, pexcepinfo,
                                    puArgErr);
  if (SUCCEEDED(hr)) {
    return hr;
  }

  // Disregard reserved dispatch-ids corresponding to VB/OLE.
  if (static_cast<int>(dispidMember) < 0)
    return E_FAIL;

  if (plugin_proxy_.get()) {
    CComPtr<INPObjectProxy> script_object;
    hr = plugin_proxy_->GetScriptableObject(&script_object);
    if (SUCCEEDED(hr)) {
      return script_object->Invoke(dispidMember, riid, lcid, wFlags,
                                   pdispparams, pvarResult, pexcepinfo,
                                   puArgErr);
    } else {
      return hr;
    }
  } else {
    return E_FAIL;
  }
}

STDMETHODIMP CHostControl::DeleteMemberByDispID(DISPID id) {
  HRESULT hr;
  if (plugin_proxy_.get()) {
    CComPtr<INPObjectProxy> script_object;
    hr = plugin_proxy_->GetScriptableObject(&script_object);
    if (SUCCEEDED(hr)) {
      return script_object->DeleteMemberByDispID(id);
    } else {
      return hr;
    }
  } else {
    return E_FAIL;
  }
}

STDMETHODIMP CHostControl::DeleteMemberByName(BSTR bstrName, DWORD grfdex) {
  HRESULT hr;
  if (plugin_proxy_.get()) {
    CComPtr<INPObjectProxy> script_object;
    hr = plugin_proxy_->GetScriptableObject(&script_object);
    if (SUCCEEDED(hr)) {
      return script_object->DeleteMemberByName(bstrName, grfdex);
    } else {
      return hr;
    }
  } else {
    return E_FAIL;
  }
}

STDMETHODIMP CHostControl::GetDispID(BSTR bstrName,
                                     DWORD grfdex,
                                     DISPID* pid) {
  // Forward all DISPID requests through the typelib before defaulting to the
  // to the NPAPI plugin.
  HRESULT hr;
  if (SUCCEEDED(hr = DispatchImpl::GetIDsOfNames(IID_NULL,
                                                 &bstrName,
                                                 1,
                                                 LOCALE_SYSTEM_DEFAULT,
                                                 pid))) {
    return hr;
  }

  if (plugin_proxy_.get()) {
    CComPtr<INPObjectProxy> script_object;
    hr = plugin_proxy_->GetScriptableObject(&script_object);
    if (SUCCEEDED(hr)) {
      return script_object->GetDispID(bstrName, grfdex, pid);
    } else {
      return hr;
    }
  } else {
    return E_FAIL;
  }
}

STDMETHODIMP CHostControl::GetMemberName(DISPID id,
                                         BSTR* pbstrName) {
  HRESULT hr;
  if (plugin_proxy_.get()) {
    CComPtr<INPObjectProxy> script_object;
    hr = plugin_proxy_->GetScriptableObject(&script_object);
    if (SUCCEEDED(hr)) {
      return script_object->GetMemberName(id, pbstrName);
    } else {
      return hr;
    }
  } else {
    return E_FAIL;
  }
}

STDMETHODIMP CHostControl::GetMemberProperties(DISPID id,
                                               DWORD grfdexFetch,
                                               DWORD* pgrfdex) {
  HRESULT hr;
  if (plugin_proxy_.get()) {
    CComPtr<INPObjectProxy> script_object;
    hr = plugin_proxy_->GetScriptableObject(&script_object);
    if (SUCCEEDED(hr)) {
      return script_object->GetMemberProperties(id, grfdexFetch, pgrfdex);
    } else {
      return hr;
    }
  } else {
    return E_FAIL;
  }
}

STDMETHODIMP CHostControl::GetNameSpaceParent(IUnknown** punk) {
  HRESULT hr;
  if (plugin_proxy_.get()) {
    CComPtr<INPObjectProxy> script_object;
    hr = plugin_proxy_->GetScriptableObject(&script_object);
    if (SUCCEEDED(hr)) {
      return script_object->GetNameSpaceParent(punk);
    } else {
      return hr;
    }
  } else {
    return E_FAIL;
  }
}

STDMETHODIMP CHostControl::GetNextDispID(DWORD grfdex,
                                         DISPID id,
                                         DISPID* pid) {
  HRESULT hr;
  if (plugin_proxy_.get()) {
    CComPtr<INPObjectProxy> script_object;
    hr = plugin_proxy_->GetScriptableObject(&script_object);
    if (SUCCEEDED(hr)) {
      return script_object->GetNextDispID(grfdex, id, pid);
    } else {
      return hr;
    }
  } else {
    return E_FAIL;
  }
}

STDMETHODIMP CHostControl::InvokeEx(DISPID id,
                                    LCID lcid,
                                    WORD wFlags,
                                    DISPPARAMS* pdb,
                                    VARIANT* pVarRes,
                                    EXCEPINFO* pei,
                                    IServiceProvider* pspCaller) {
  // Forward all InvokeEx requests through the typelib
  HRESULT hr = DispatchImpl::Invoke(id, IID_NULL, lcid, wFlags,
                                    pdb, pVarRes, pei,
                                    NULL);
  if (SUCCEEDED(hr)) {
    return hr;
  }

  if (plugin_proxy_.get()) {
    CComPtr<INPObjectProxy> script_object;
    hr = plugin_proxy_->GetScriptableObject(&script_object);
    if (SUCCEEDED(hr)) {
      return script_object->InvokeEx(id, lcid, wFlags, pdb, pVarRes, pei,
                                     pspCaller);
    } else {
      return hr;
    }
  } else {
    return E_FAIL;
  }
}

// Receive the arguments provided to the plug-in in the param-tag.
STDMETHODIMP CHostControl::Load(IPropertyBag* property_bag,
                                IErrorLog* error_log) {
  if (!property_bag) {
    return E_INVALIDARG;
  }

  // Iterate through all of the properties provided, and register them, in
  // ASCII-string form with the control.
  CComQIPtr<IPropertyBag2> property_bag2 = property_bag;
  if (property_bag2) {
    ULONG property_count;
    if (SUCCEEDED(property_bag2->CountProperties(&property_count))) {
      for (int x = 0; x < property_count; ++x) {
        PROPBAG2 property = {0};
        ULONG properties_read = 0;
        if (SUCCEEDED(property_bag2->GetPropertyInfo(x, 1, &property,
                                                     &properties_read))) {
          CComVariant variant;
          HRESULT prop_hr;
          if (SUCCEEDED(property_bag2->Read(1, &property, NULL,
                                            &variant, &prop_hr))) {
            if (SUCCEEDED(variant.ChangeType(VT_BSTR))) {
              USES_CONVERSION;
              RegisterPluginParameter(OLE2A(property.pstrName),
                                      OLE2A(variant.bstrVal));
            }
          }
          // According to the msdn documentation, the name of the property
          // must be freed through CoTaskMemFree.
          // See:  http://msdn.microsoft.com/en-us/library/aa768191(VS.85).aspx
          CoTaskMemFree(property.pstrName);
        }
      }
    }
  }

  return IPersistPropertyBagImpl<CHostControl>::Load(property_bag, error_log);
}

HRESULT CHostControl::GetStringProperty(NPPVariable np_property_variable,
                                        BSTR* string) {
  HRESULT hr;
  if (FAILED(hr = ConstructPluginProxy())) {
    return hr;
  }

  char* property = NULL;
  if (NPERR_NO_ERROR != plugin_proxy_->GetPluginFunctions()->getvalue(
          NULL,
          np_property_variable,
          &property)) {
    return E_FAIL;
  }

  scoped_array<wchar_t> wide_property;
  if (FAILED(hr = ConvertMultiByteToWideChar(property, &wide_property))) {
    return hr;
  }
  *string = SysAllocString(wide_property.get());
  return S_OK;
}

STDMETHODIMP CHostControl::get_description(BSTR *returned_description) {
  return GetStringProperty(NPPVpluginDescriptionString, returned_description);
}

STDMETHODIMP CHostControl::get_name(BSTR *returned_name) {
  return GetStringProperty(NPPVpluginNameString, returned_name);
}

HRESULT CHostControl::ConstructPluginProxy() {
  // If the plugin has already been constructed, then exit early.
  if (plugin_proxy_.get()) {
    return S_OK;
  }

  HRESULT hr;
  NPPluginProxy* plugin_proxy = NULL;
  if (FAILED(hr = NPPluginProxy::Create(&plugin_proxy))) {
    return hr;
  }

  plugin_proxy_.reset(plugin_proxy);
  return S_OK;
}
