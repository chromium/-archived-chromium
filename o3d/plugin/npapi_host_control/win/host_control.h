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


// Declares a COM class implementing an ActiveX control capable of hosting
// an NPAPI plugin on an OLE site.

#ifndef O3D_PLUGIN_NPAPI_HOST_CONTROL_WIN_HOST_CONTROL_H_
#define O3D_PLUGIN_NPAPI_HOST_CONTROL_WIN_HOST_CONTROL_H_

#include <atlctl.h>
#include <dispex.h>
#include <vector>

#include "base/scoped_ptr.h"

// Directory not included, as this file is auto-generated from the
// type library.
#include "npapi_host_control.h"
#include "plugin/npapi_host_control/win/np_browser_proxy.h"

class NPPluginProxy;

// Class implementing an ActiveX control for containing NPAPI plugin-objects.
// This needs to be CComMultiThreadModel because these objects are concurrently
// AddRefed and Released from StreamOperation threads.
class ATL_NO_VTABLE CHostControl
    : public CComObjectRootEx<CComMultiThreadModel>,
      public CComCoClass<CHostControl, &CLSID_HostControl>,
      public CComControl<CHostControl>,
      // IMPORTANT IMPLEMENTATION NOTE:
      // Pass 0xFFFF to the major and minor versions of the IDispatchImpl
      // for trigger the behaviour in CComTypeInfoHolder::GetTI that forces
      // the type-library to be loaded from the module, not through the
      // registry.
      // Without this behaviour, the plug-in fails to load on Vista with UAC
      // disabled.  This is because all processes run at elevated integrity
      // with UAC disabled.  Because the plug-in is registered as a per-user
      // control (under HKCU), it will fail to load the type-library through
      // the registry:  Elevated processes do not view the contents of the HKCU
      // hive, so it will appear as if the control was not installed properly.
      public IDispatchImpl<IHostControl, &IID_IHostControl,
                           &LIBID_npapi_host_controlLib,
                           0xFFFF,
                           0xFFFF>,
      public IOleControlImpl<CHostControl>,
      public IOleObjectImpl<CHostControl>,
      public IOleInPlaceActiveObjectImpl<CHostControl>,
      public IOleInPlaceObjectWindowlessImpl<CHostControl>,
      public ISupportErrorInfo,
      public IProvideClassInfo2Impl<&CLSID_HostControl, NULL,
                                    &LIBID_npapi_host_controlLib>,
      public IObjectSafetyImpl<CHostControl,
                               INTERFACESAFE_FOR_UNTRUSTED_CALLER |
                               INTERFACESAFE_FOR_UNTRUSTED_DATA>,
      public IPersistStreamInitImpl<CHostControl>,
      public IPersistPropertyBagImpl<CHostControl>,
      public IPersistStorageImpl<CHostControl>,
      public IConnectionPointContainerImpl<CHostControl>,
      public IPropertyNotifySinkCP<CHostControl> {
 public:
  CHostControl();
  virtual ~CHostControl();

DECLARE_OLEMISC_STATUS(OLEMISC_RECOMPOSEONRESIZE |
  OLEMISC_CANTLINKINSIDE |
  OLEMISC_INSIDEOUT |
  OLEMISC_ACTIVATEWHENVISIBLE |
  OLEMISC_SETCLIENTSITEFIRST
)

DECLARE_REGISTRY_RESOURCEID(IDR_HOSTCONTROL)

BEGIN_MSG_MAP(CHostControl)
  MESSAGE_HANDLER(WM_CREATE, OnCreate)
  MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
END_MSG_MAP()

BEGIN_CONNECTION_POINT_MAP(CHostControl)
  CONNECTION_POINT_ENTRY(IID_IPropertyNotifySink)
END_CONNECTION_POINT_MAP()

// Register this control as safe for initialization and scripting.  If these
// categories are skipped, IE will force the user to permit the control to
// allow scripting at every page view.
BEGIN_CATEGORY_MAP(CHostControl)
  IMPLEMENTED_CATEGORY(CATID_SafeForScripting)
  IMPLEMENTED_CATEGORY(CATID_SafeForInitializing)
END_CATEGORY_MAP()

BEGIN_COM_MAP(CHostControl)
  COM_INTERFACE_ENTRY(IHostControl)
  COM_INTERFACE_ENTRY(IDispatch)
  COM_INTERFACE_ENTRY(IDispatchEx)
  COM_INTERFACE_ENTRY(IOleInPlaceObject)
  COM_INTERFACE_ENTRY2(IOleWindow, IOleInPlaceObjectWindowless)
  COM_INTERFACE_ENTRY(IOleInPlaceActiveObject)
  COM_INTERFACE_ENTRY(IOleControl)
  COM_INTERFACE_ENTRY(IOleObject)
  COM_INTERFACE_ENTRY(ISupportErrorInfo)
  COM_INTERFACE_ENTRY(IProvideClassInfo)
  COM_INTERFACE_ENTRY(IProvideClassInfo2)
  COM_INTERFACE_ENTRY(IPersistStreamInit)
  COM_INTERFACE_ENTRY2(IPersist, IPersistStreamInit)
  COM_INTERFACE_ENTRY(IPersistPropertyBag)
  COM_INTERFACE_ENTRY(IPersistStorage)
  COM_INTERFACE_ENTRY(IConnectionPointContainer)
END_COM_MAP()

BEGIN_PROP_MAP(CHostControl)
END_PROP_MAP()

  STDMETHOD(GetTypeInfoCount)(UINT* pctinfo);
  STDMETHOD(GetTypeInfo)(UINT itinfo, LCID lcid, ITypeInfo** pptinfo);
  STDMETHOD(GetIDsOfNames)(REFIID riid, LPOLESTR* rgszNames, UINT cNames,
                           LCID lcid, DISPID* rgdispid);
  STDMETHOD(Invoke)(DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags,
                    DISPPARAMS* pdispparams, VARIANT* pvarResult,
                    EXCEPINFO* pexcepinfo, UINT* puArgErr);
  STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid);

  STDMETHOD(DeleteMemberByDispID)(DISPID id);
  STDMETHOD(DeleteMemberByName)(BSTR bstrName, DWORD grfdex);
  STDMETHOD(GetDispID)(BSTR bstrName, DWORD grfdex, DISPID* pid);
  STDMETHOD(GetMemberName)(DISPID id, BSTR* pbstrName);
  STDMETHOD(GetMemberProperties)(DISPID id, DWORD grfdexFertch, DWORD* pgrfdex);
  STDMETHOD(GetNameSpaceParent)(IUnknown** ppunk);
  STDMETHOD(GetNextDispID)(DWORD grfdex, DISPID id, DISPID* pid);
  STDMETHOD(InvokeEx)(DISPID id, LCID lcid, WORD wFlags, DISPPARAMS* pdp,
                      VARIANT* pVarRes, EXCEPINFO* pei,
                      IServiceProvider* pspCaller);

  // Method overridden from IPersistPropertyBagImpl.
  STDMETHOD(Load(IPropertyBag *pPropBag, IErrorLog *pErrorLog));

  // Returns the properties associated with the NPPVpluginNameString, and
  // NPPVpluginDescriptionString identifiers of the loaded plug-in.  These
  // properties can be used for plug-in introspection and version-dependent
  // behaviour.
  STDMETHOD(get_description)(BSTR *returned_description);
  STDMETHOD(get_name)(BSTR *returned_name);

  LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

  // Initiates a data transfer, calling back into the hosted plug-in instance
  // on status updates.  Does not block on the transfer.
  // Parameters:
  //    url:  The url from which to receive data.
  //    notify_data:  Opaque handle to data provided by the plug-in instance.
  //                  This data will be passed back to the plug-in during
  //                  all call-backs.
  // Returns:
  //    S_OK on success.
  HRESULT OpenUrlStream(const wchar_t *url, void *notify_data);

  // Returns the user-agent string of the browser hosting the control.
  // Returns NULL on failure.
  const char* GetUserAgent() const;

  // Return a moniker representing the url of the page in which the plugin is
  // contained.
  IMoniker* GetURLMoniker() const {
    return url_moniker_;
  }

  DECLARE_PROTECT_FINAL_CONSTRUCT()
  HRESULT FinalConstruct();
  void FinalRelease();

 private:
  // Performs all of the basic construction of the hosted NPPluginProxy object,
  // but does not initialize an active instance of the plug-in.
  HRESULT ConstructPluginProxy();

  // Returns an NPAPI property from the hosted plug-in.
  // Parameters:
  //    np_property_variable:  NPPVariable value corresponding to the property
  //                           to return.
  //    string:  Pointer to BString that receives the property.
  // Returns:
  //    S_OK on success.
  HRESULT GetStringProperty(NPPVariable np_property_variable, BSTR* string);

  // Free all resources allocated when constructing the windowed instance
  // of the hosted plug-in in OnCreate(...).
  void TearDown();

  void RegisterPluginParameter(const char *name, const char *value) {
    ATLASSERT(name && value);
    plugin_argument_names_.push_back(CStringA(name));
    plugin_argument_values_.push_back(CStringA(value));
  }

  // Browser proxy instance used to communicate with the hosted NPAPI plugin.
  scoped_ptr<NPBrowserProxy> browser_proxy_;

  // Pointer to the plugin being hosted by the control.
  scoped_ptr<NPPluginProxy> plugin_proxy_;

  // Cached value of the name of the control as it exists in the HTML DOM.
  BSTR embedded_name_;

  // Cached string representation of the user-agent, initialized by first call
  // to GetUserAgent.
  mutable scoped_array<char> user_agent_;

  CComPtr<IWebBrowserApp> web_browser_app_;
  CComQIPtr<IServiceProvider, &IID_IServiceProvider> service_provider_;
  CComPtr<IDispatchEx> document_dispatch_;
  CComPtr<IHTMLDocument2> html_document2_;
  CComPtr<IHTMLDocument3> html_document3_;
  CComPtr<IDispatchEx> window_dispatch_;
  CComPtr<IHTMLWindow2> html_window_;
  CComPtr<IOmNavigator> navigator_;
  CComPtr<IMoniker> url_moniker_;

  // Array of strings to be passed as name/value arguments to the NPAPI
  // plug-in instance during construction in NPP_New.
  std::vector<CStringA> plugin_argument_names_;
  std::vector<CStringA> plugin_argument_values_;

  typedef IDispatchImpl<IHostControl, &IID_IHostControl,
                        &LIBID_npapi_host_controlLib,
                        0xFFFF, 0xFFFF> DispatchImpl;

  DISALLOW_COPY_AND_ASSIGN(CHostControl);
};

// Register this COM class with the COM module.
OBJECT_ENTRY_AUTO(__uuidof(HostControl), CHostControl);

#endif  // O3D_PLUGIN_NPAPI_HOST_CONTROL_WIN_HOST_CONTROL_H_
