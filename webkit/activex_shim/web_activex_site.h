// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_ACTIVEX_SHIM_WEB_ACTIVEX_SITE_H__
#define WEBKIT_ACTIVEX_SHIM_WEB_ACTIVEX_SITE_H__

#include <atlbase.h>
#include <atlcom.h>
#include <objsafe.h>
#include <map>
#include <vector>
#include "webkit/activex_shim/activex_util.h"

namespace activex_shim {

class WebActiveXContainer;
class ActiveXPlugin;

// Control creation parameters
struct ControlParam {
  ControlParam(const std::wstring& new_name, const std::wstring& new_value) {
    name = new_name;
    value = new_value;
  }
  ControlParam() { }
  std::wstring name;
  std::wstring value;
};

// ActiveX control site to receive requests etc from the ActiveX control,
// and interact the container to host a control.
// Implemented interfaces:
//   IDispatch:
//     Required for ambient properties.
//   IOleClientSite:
//     Required interface.
//   IOleControlSite:
//     Required interface.
//   IOleInPlaceSiteWindowless:
//     Required for windowless activation.
//   IServiceProvider:
//     Some controls use this interface to get interface to the IBindHost,
//     IWebBrowserApp interfaces. e.g., Flash needs the IBindHost to create
//     a moniker to the movie. Thus this is a required interface.
//   IPropertyBag:
//     If the control supports IPersistPropertyBag, we could use this interface
//     to initialize the control with param values.
class WebActiveXSite : public MinimumIDispatchImpl,
                       public IOleClientSite,
                       public IOleControlSite,
                       public IOleInPlaceSiteWindowless,
                       public IServiceProvider,
                       public IPropertyBag,
                       public IPropertyBag2 {
 public:
  WebActiveXSite();
  // It's necessary to make it virtual because we do not directly create
  // this object. Instead we usually create NoRefIUnknownImpl<WebActiveXSite>
  virtual ~WebActiveXSite();

  // Container calls this to init a site. The container should assume passing
  // the ownership of IUnknown to site, and not try to release control there
  // after. Site will release control in FinalRelease.
  void Init(WebActiveXContainer* container, IUnknown* control);
  // Deactive and release ActiveX control. Cleanup everything.
  void FinalRelease();
  // Sets the extent of the control, params and inplace activates it.
  HRESULT ActivateControl(int x, int y, int width, int height,
                          const std::vector<ControlParam>& params);
  // A simplified version of calling control's DoVerb.
  HRESULT DoVerb(long verb);
  // Changes the position/size of the control. The container/plugin is
  // responsible to call this everytime the control's position/size changed.
  void SetRect(const RECT* rect);

  // IUnknown
  virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,
                                                   void** object);

  // IOleClientSite
  virtual HRESULT STDMETHODCALLTYPE SaveObject();
  virtual HRESULT STDMETHODCALLTYPE GetMoniker(
      DWORD assign,
      DWORD which_moniker,
      IMoniker** moniker);
  virtual HRESULT STDMETHODCALLTYPE GetContainer(IOleContainer** container);
  virtual HRESULT STDMETHODCALLTYPE ShowObject();
  virtual HRESULT STDMETHODCALLTYPE OnShowWindow(BOOL show);
  virtual HRESULT STDMETHODCALLTYPE RequestNewObjectLayout();

  // IOleControlSite
  virtual HRESULT STDMETHODCALLTYPE OnControlInfoChanged();
  virtual HRESULT STDMETHODCALLTYPE LockInPlaceActive(BOOL lock);
  virtual HRESULT STDMETHODCALLTYPE GetExtendedControl(IDispatch** disp);
  virtual HRESULT STDMETHODCALLTYPE TransformCoords(POINTL* ptl_himetric,
                                                    POINTF* ptf_container,
                                                    DWORD flags);
  virtual HRESULT STDMETHODCALLTYPE TranslateAccelerator(MSG* msg,
                                                         DWORD modifiers);
  virtual HRESULT STDMETHODCALLTYPE OnFocus(BOOL got_focus);
  virtual HRESULT STDMETHODCALLTYPE ShowPropertyFrame();

  // IOleWindow
  virtual HRESULT STDMETHODCALLTYPE GetWindow(HWND* wnd);
  virtual HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(BOOL enter_mode);

  // IOleInPlaceSite
  virtual HRESULT STDMETHODCALLTYPE CanInPlaceActivate();
  virtual HRESULT STDMETHODCALLTYPE OnInPlaceActivate();
  virtual HRESULT STDMETHODCALLTYPE OnUIActivate();
  virtual HRESULT STDMETHODCALLTYPE GetWindowContext(
      IOleInPlaceFrame** frame,
      IOleInPlaceUIWindow** doc,
      LPRECT pos,
      LPRECT clip,
      LPOLEINPLACEFRAMEINFO frame_info);
  virtual HRESULT STDMETHODCALLTYPE Scroll(SIZE scroll_extant);
  virtual HRESULT STDMETHODCALLTYPE OnUIDeactivate(BOOL undoable);
  virtual HRESULT STDMETHODCALLTYPE OnInPlaceDeactivate();
  virtual HRESULT STDMETHODCALLTYPE DiscardUndoState();
  virtual HRESULT STDMETHODCALLTYPE DeactivateAndUndo();
  virtual HRESULT STDMETHODCALLTYPE OnPosRectChange(LPCRECT pos);

  // IOleInPlaceSiteEx
  virtual HRESULT STDMETHODCALLTYPE OnInPlaceActivateEx(
      BOOL* no_redraw,
      DWORD flags);
  virtual HRESULT STDMETHODCALLTYPE OnInPlaceDeactivateEx(BOOL no_redraw);
  virtual HRESULT STDMETHODCALLTYPE RequestUIActivate();

  // IOleInPlaceSiteWindowless
  virtual HRESULT STDMETHODCALLTYPE CanWindowlessActivate();
  virtual HRESULT STDMETHODCALLTYPE GetCapture();
  virtual HRESULT STDMETHODCALLTYPE SetCapture(BOOL capture);
  virtual HRESULT STDMETHODCALLTYPE GetFocus();
  virtual HRESULT STDMETHODCALLTYPE SetFocus(BOOL focus);
  virtual HRESULT STDMETHODCALLTYPE GetDC(
      LPCRECT rect,
      DWORD flags,
      HDC* dc);
  virtual HRESULT STDMETHODCALLTYPE ReleaseDC(HDC dc);
  virtual HRESULT STDMETHODCALLTYPE InvalidateRect(
      LPCRECT rect,
      BOOL erase);
  virtual HRESULT STDMETHODCALLTYPE InvalidateRgn(
      HRGN rgn,
      BOOL erase);
  virtual HRESULT STDMETHODCALLTYPE ScrollRect(
      INT dx,
      INT dy,
      LPCRECT scroll,
      LPCRECT clip);
  virtual HRESULT STDMETHODCALLTYPE AdjustRect(LPRECT rc);
  virtual HRESULT STDMETHODCALLTYPE OnDefWindowMessage(
      UINT msg,
      WPARAM wparam,
      LPARAM lparam,
      LRESULT* result);

  // IServiceProvider
  virtual HRESULT STDMETHODCALLTYPE QueryService(
      REFGUID guid,
      REFIID riid,
      void** object);

  // IPropertyBag
  virtual HRESULT STDMETHODCALLTYPE Read(LPCOLESTR prop_name, VARIANT* var,
                                         IErrorLog* err_log);
  virtual HRESULT STDMETHODCALLTYPE Write(LPCOLESTR prop_name, VARIANT* var);

  // IPropertyBag2
  virtual HRESULT STDMETHODCALLTYPE Read(ULONG c_properties,
                                         PROPBAG2* prop_bag,
                                         IErrorLog* err_log,
                                         VARIANT* value,
                                         HRESULT* error);
  virtual HRESULT STDMETHODCALLTYPE Write(ULONG c_properties,
                                          PROPBAG2* prop_bag,
                                          VARIANT* value);
  virtual HRESULT STDMETHODCALLTYPE CountProperties(ULONG* c_properties);
  virtual HRESULT STDMETHODCALLTYPE GetPropertyInfo(ULONG iproperty,
                                                    ULONG c_properties,
                                                    PROPBAG2* prop_bag,
                                                    ULONG* properties_returned);
  virtual HRESULT STDMETHODCALLTYPE LoadObject(LPCOLESTR pstr_name,
                                               DWORD hint,
                                               IUnknown* unk_object,
                                               IErrorLog* err_log);

  friend WebActiveXContainer;
  friend ActiveXPlugin;

 private:
  // Call IOleObject::SetExtent to change the size of the control. width and
  // height should be in pixels.
  HRESULT SetExtent(int width, int height);

  WebActiveXContainer* container_;
  // Theorectically the control could support only IUnknown interface. This is
  // is the minimum requirement.
  CComPtr<IUnknown> control_;
  // These are all optional interfaces and they could be NULL even if we have
  // created the control successfully.
  CComQIPtr<IDispatch> dispatch_;
  CComQIPtr<IOleObject> ole_object_;
  CComQIPtr<IOleInPlaceObject> inplace_object_;
  CComQIPtr<IViewObject> view_object_;
  CComQIPtr<IOleInPlaceObjectWindowless> inplace_object_windowless_;
  RECT rect_;
  // We need to remember whether we are activated so we can decide whether to
  // deactivate during destruction.
  bool inplace_activated_;
  bool has_capture_;
  // We need to save the initial properties so that during control
  // initialization, the control can query us (IPropertyBag) for those
  // properties.
  std::vector<ControlParam> initial_params_;

  DISALLOW_EVIL_CONSTRUCTORS(WebActiveXSite);
};

}  // namespace activex_shim

#endif // #ifndef WEBKIT_ACTIVEX_SHIM_WEB_ACTIVEX_SITE_H__

