// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/activex_shim/web_activex_site.h"

#include <exdisp.h>
#include <oaidl.h>
#include <shlguid.h>

#include "base/string_util.h"
#include "webkit/activex_shim/activex_plugin.h"
#include "webkit/activex_shim/npp_impl.h"
#include "webkit/activex_shim/web_activex_container.h"

namespace activex_shim {

// WebActiveXSite
WebActiveXSite::WebActiveXSite()
    : container_(NULL),
      control_(NULL),
      inplace_activated_(false),
      has_capture_(false) {
  rect_.left = 0;
  rect_.top = 0;
  rect_.right = 0;
  rect_.bottom = 0;
}

WebActiveXSite::~WebActiveXSite() {
  // Don't do anything here. Do everything in FinalRelease.
}

void WebActiveXSite::Init(WebActiveXContainer* container, IUnknown* control) {
  container_ = container;
  control_.Attach(control);
  dispatch_ = control;
  ole_object_ = control;
  inplace_object_ = control;
  view_object_ = control;
  inplace_object_windowless_ = control;
}

void WebActiveXSite::FinalRelease() {
  // We must release everything here instead of leaving it to the destructor.
  // Otherwise crash is possible.
  if (control_ != NULL) {
    dispatch_.Release();
    view_object_.Release();
    inplace_object_windowless_.Release();
    if (inplace_object_ != NULL) {
      if (inplace_activated_) {
        // If we just deactivate without checking whether the control has been
        // inplace activated, the control may behave irratically. Flash will
        // decrease its reference count during deactivation. Thus causing
        // crash when we try to release it later.
        inplace_object_->InPlaceDeactivate();
        inplace_activated_ = false;
      }
      inplace_object_.Release();
    }
    if (ole_object_ != NULL) {
      ole_object_->SetClientSite(NULL);
      ole_object_->Close(OLECLOSE_NOSAVE);
      ole_object_.Release();
    }
    long ref = control_.Detach()->Release();
    // It should be 0 otherwise we have incorrect ref counting.
    // Shockwave is known to have ref counting problems. All other controls
    // behave well.
    DCHECK(ref == 0 || container_->plugin()->activex_type()
                       == ACTIVEX_SHOCKWAVE);
  }
}

HRESULT WebActiveXSite::ActivateControl(
    int x, int y, int width, int height,
    const std::vector<ControlParam>& params) {
  // Set the rect size of site first before SetClientSite. Otherwise the
  // control may query the site for such information during SetClientSite.
  rect_.left = x;
  rect_.top = y;
  rect_.right = rect_.left + width;
  rect_.bottom = rect_.top + height;

  HRESULT hr;
  if (ole_object_ != NULL) {
    hr = ole_object_->SetClientSite(static_cast<IOleClientSite*>(this));
    if (FAILED(hr))
      return hr;
  }
  SetExtent(width, height);

  // Set initial properties.
  CComQIPtr<IPersistPropertyBag> persist_property_bag = control_;
  CComQIPtr<IPersistPropertyBag2> persist_property_bag2 = control_;
  if (persist_property_bag2 != NULL || persist_property_bag != NULL) {
    // Use property bag for initialization. This is the preferred way.
    initial_params_ = params;
    // Use bag2 first.
    if (persist_property_bag2 != NULL) {
      persist_property_bag2->InitNew();
      hr = persist_property_bag2->Load(this, NULL);
      DCHECK(SUCCEEDED(hr));
    } else {
      persist_property_bag->InitNew();
      hr = persist_property_bag->Load(this, NULL);
      DCHECK(SUCCEEDED(hr));
    }
    // We don't need this anymore.
    initial_params_.clear();
  } else if (dispatch_ != NULL) {
    // Use the dispatch interface to set the initial properties. This is
    // less efficient for most controls.
    for (unsigned int i = 0; i < params.size(); ++i) {
      const ControlParam& param = params[i];
      VARIANT vtvalue;
      // TODO(ruijiang): Think about type conversion.
      vtvalue.vt = VT_BSTR;
      vtvalue.bstrVal = SysAllocString(param.value.c_str());
      DispSetProperty(dispatch_, param.name.c_str(), vtvalue);
      VariantClear(&vtvalue);
    }
  }

  // In place activate it if it is able to.
  if (inplace_object_ != NULL) {
    hr = DoVerb(OLEIVERB_INPLACEACTIVATE);
    if (FAILED(hr))
      return hr;
  }

  return S_OK;
}

HRESULT WebActiveXSite::DoVerb(long verb) {
  if (ole_object_ != NULL) {
    HRESULT hr = ole_object_->DoVerb(verb, NULL,
                                     static_cast<IOleClientSite*>(this), 0,
                                     container_->container_wnd(), &rect_);
    if (verb == OLEIVERB_INPLACEACTIVATE && SUCCEEDED(hr))
      inplace_activated_ = true;
    return hr;
  } else {
    return E_UNEXPECTED;
  }
}

HRESULT WebActiveXSite::SetExtent(int width, int height) {
  if (ole_object_ != NULL) {
    SIZEL size;
    if (width < 0)
      width = 0;
    if (height < 0)
      height = 0;
    ScreenToHimetric(width, height, &size);
    return ole_object_->SetExtent(DVASPECT_CONTENT, &size);
  } else {
    return E_UNEXPECTED;
  }
}

void WebActiveXSite::SetRect(const RECT* rect) {
  if (EqualRect(&rect_, rect))
    return;
  SetExtent(rect->right - rect->left, rect->bottom - rect->top);
  if (inplace_object_ != NULL) {
    inplace_object_->SetObjectRects(rect, rect);
    rect_ = *rect;
  }
}

// IUnknown
HRESULT STDMETHODCALLTYPE WebActiveXSite::QueryInterface(REFIID iid,
                                                         void** object) {
  *object = NULL;
  if (iid == IID_IUnknown) {
    // Avoid ambiguous resolution of IUnknown.
    *object = static_cast<IUnknown*>(static_cast<MinimumIDispatchImpl*>(this));
  } else if (iid == IID_IDispatch) {
    *object = static_cast<MinimumIDispatchImpl*>(this);
  } else if (iid == IID_IOleClientSite) {
    *object = static_cast<IOleClientSite*>(this);
  } else if (iid == IID_IOleControlSite) {
    *object = static_cast<IOleControlSite*>(this);
  } else if (iid == IID_IOleInPlaceSite) {
    *object = static_cast<IOleInPlaceSite*>(this);
  } else if (iid == IID_IOleInPlaceSiteEx) {
    *object = static_cast<IOleInPlaceSiteEx*>(this);
  } else if (iid == IID_IOleInPlaceSiteWindowless) {
    if (container_->plugin()->windowless())
      *object = static_cast<IOleInPlaceSiteWindowless*>(this);
  } else if (iid == IID_IServiceProvider) {
    *object = static_cast<IServiceProvider*>(this);
  } else if (iid == IID_IPropertyBag) {
    *object = static_cast<IPropertyBag*>(this);
  } else if (iid == IID_IPropertyBag2) {
    *object = static_cast<IPropertyBag2*>(this);
  }
  TRACK_QUERY_INTERFACE(iid, *object != NULL);
  return (*object != NULL) ? S_OK : E_NOINTERFACE;
}

// IOleClientSite
HRESULT STDMETHODCALLTYPE WebActiveXSite::SaveObject() {
  // Do not support saving object to persistant storage.
  return E_NOTIMPL;
}

// Even though Flash will call this method to get the url, it will not use
// the url to resolve its movie path.
// However, according to http://support.microsoft.com/kb/181678, this is
// a valid way of getting url from ActiveX control.
HRESULT STDMETHODCALLTYPE WebActiveXSite::GetMoniker(
    DWORD assign,
    DWORD which_moniker,
    IMoniker** moniker) {
  TRACK_METHOD();
  if (which_moniker == OLEWHICHMK_CONTAINER) {
    std::wstring url = container_->plugin()->GetCurrentURL();
    HRESULT hr = CreateURLMoniker(NULL, url.c_str(), moniker);
    return hr;
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::GetContainer(
    IOleContainer** container) {
  *container = container_;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::ShowObject() {
  TRACK_METHOD();
  // The control asks us to show the object which we already did.
  return S_OK;
};

HRESULT STDMETHODCALLTYPE WebActiveXSite::OnShowWindow(BOOL show) {
  TRACK_METHOD();
  // Doesn't apply to us.
  return S_OK;
};

HRESULT STDMETHODCALLTYPE WebActiveXSite::RequestNewObjectLayout() {
  TRACK_METHOD();
  // As MSDN says: "Currently, there is no standard mechanism by which
  // a container can negotiate how much room an object would like. When
  // such a negotiation is defined, responding to this method will be
  // optional for containers."
  return E_NOTIMPL;
}

// IOleControlSite
HRESULT STDMETHODCALLTYPE WebActiveXSite::OnControlInfoChanged() {
  TRACK_METHOD();
  // As we do not support mnemonics, we do not need to retrieve control info.
  // This may change in the future though.
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::LockInPlaceActive(BOOL lock) {
  TRACK_METHOD();
  // We don't support this.
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::GetExtendedControl(IDispatch** disp) {
  TRACK_METHOD();
  // We do not support extended control.
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::TransformCoords(POINTL* ptl_himetric,
                                                          POINTF* ptf_container,
                                                          DWORD flags) {
  TRACK_METHOD();
  // TODO(ruijiang): so far haven't found anyone use this yet. Be aware and
  // add support if needed.
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::TranslateAccelerator(
    MSG* msg,
    DWORD modifiers) {
  TRACK_METHOD();
  // It would be nice if controls call this and let me process accelerator
  // first. But unfortunately all of them I tested don't. So let's ignore
  // and just keep an eye on it.
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::OnFocus(BOOL got_focus) {
  TRACK_METHOD();
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::ShowPropertyFrame() {
  TRACK_METHOD();
  // No we don't want to show property sheet.
  return E_NOTIMPL;
}

// IOleWindow

HRESULT STDMETHODCALLTYPE WebActiveXSite::GetWindow(HWND* wnd) {
  *wnd = container_->container_wnd();
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::ContextSensitiveHelp(
    BOOL enter_mode) {
  // Do not support this.
  return E_NOTIMPL;
}

// IOleInPlaceSite

HRESULT STDMETHODCALLTYPE WebActiveXSite::CanInPlaceActivate() {
  TRACK_METHOD();
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::OnInPlaceActivate() {
  TRACK_METHOD();
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::OnInPlaceDeactivate() {
  TRACK_METHOD();
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::OnUIActivate() {
  TRACK_METHOD();
  // If we have multiple sites in a container we may deactivate the previous
  // active control. This is not a requirement though.
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::OnUIDeactivate(BOOL undoable) {
  TRACK_METHOD();
  // Some controls will call this when they lose focus. Right now we don't need
  // to do anything about it.
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::GetWindowContext(
    IOleInPlaceFrame** frame,
    IOleInPlaceUIWindow** doc,
    LPRECT pos,
    LPRECT clip,
    LPOLEINPLACEFRAMEINFO frame_info) {
  TRACK_METHOD();
  if (frame) {
    *frame = container_;
  }
  if (doc)
    *doc = NULL;
  if (pos)
    *pos = rect_;
  if (clip)
    *clip = rect_;
  if (frame_info) {
    frame_info->fMDIApp = FALSE;
    frame_info->hwndFrame = container_->container_wnd();
    frame_info->haccel = NULL;
    frame_info->cAccelEntries = 0;
  }
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::Scroll(SIZE scroll_extant) {
  TRACK_METHOD();
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::DiscardUndoState() {
  TRACK_METHOD();
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::DeactivateAndUndo() {
  TRACK_METHOD();
  // Just let the object know that it's deactivated.
  if (inplace_object_ != NULL)
    inplace_object_->UIDeactivate();
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::OnPosRectChange(LPCRECT pos) {
  TRACK_METHOD();
  // We do not let the control move/resize itself. It should be controled
  // by the container/browser.
  return E_UNEXPECTED;
}

// IOleInPlaceSiteEx

HRESULT STDMETHODCALLTYPE WebActiveXSite::OnInPlaceActivateEx(
    BOOL* no_redraw,
    DWORD flags) {
  TRACK_METHOD();
  // Redraw doesn't hurt.
  if (no_redraw)
    *no_redraw = FALSE;
  if (flags & ACTIVATE_WINDOWLESS) {
    // TODO(ruijiang): At this point we know for sure the object is activated
    // as windowless. Revisit when we implement windowless controls.
  }
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::OnInPlaceDeactivateEx(
    BOOL no_redraw) {
  TRACK_METHOD();
  // See also: OnInPlaceDeactivate
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::RequestUIActivate() {
  TRACK_METHOD();
  return S_OK;
}

// IOleInPlaceSiteWindowless

HRESULT STDMETHODCALLTYPE WebActiveXSite::CanWindowlessActivate() {
  TRACK_METHOD();
  // Yes, we prefer windowless activation.
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::GetCapture() {
  TRACK_METHOD();
  return has_capture_ ? S_OK : S_FALSE;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::SetCapture(BOOL capture) {
  TRACK_METHOD();
  // TODO(ruijiang): for now, let's cheat the control that it can always get
  // what it wants (capture).
  if (capture) {
    has_capture_ = true;
    return S_OK;
  } else {
    has_capture_ = false;
    return S_OK;
  }
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::GetFocus() {
  TRACK_METHOD();
  // TODO(ruijiang): handle it.
  return S_FALSE;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::SetFocus(BOOL focus) {
  TRACK_METHOD();
  // TODO(ruijiang): handle it.
  if (focus) {
    return S_FALSE;
  } else {
    return S_OK;
  }
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::GetDC(
    LPCRECT rect,
    DWORD flags,
    HDC* dc) {
  // It's probably not wise to get the dc of Chrome window and return it,
  // because we always draw onto a memory dc. Thus we may have to disappoint
  // the caller.
  // TODO(ruijiang): We may enable this for other browsers like FireFox.
  return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::ReleaseDC(HDC dc) {
  // TODO(ruijiang): We may enable this for other browsers like FireFox.
  return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::InvalidateRect(
    LPCRECT rect,
    BOOL erase) {
  // This would be the rect (in client coord of the control) that we will
  // invalidate.
  RECT rc;
  // Control's client area, start from top-left corner as 0.
  RECT client = rect_;
  OffsetRect(&client, -rect_.left, -rect_.top);
  if (rect) {
    RECT rc_in_client = *rect;
    OffsetRect(&rc_in_client, -rect_.left, -rect_.top);
    if (!IntersectRect(&rc, &rc_in_client, &client))
      return S_OK;
  } else {
    rc = client;
  }
  // Convert it to NPRect. Now rc is relative to the upper-left corner of
  // control. This is the requirement of NPN_InvalidateRect.
  NPRect npr;
  npr.left = static_cast<uint16>(rc.left);
  npr.top = static_cast<uint16>(rc.top);
  npr.right = static_cast<uint16>(rc.right);
  npr.bottom = static_cast<uint16>(rc.bottom);
  g_browser->invalidaterect(container_->plugin()->npp(), &npr);
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::InvalidateRgn(
    HRGN rgn,
    BOOL erase) {
  TRACK_METHOD();
  if (rgn == NULL) {
    return InvalidateRect(NULL, erase);
  } else {
    // TODO(ruijiang): So far no one is using this function yet. So let's just
    // invalidate the whole area. Optimize this when we need to.
    return InvalidateRect(NULL, erase);
  }
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::ScrollRect(
    INT dx,
    INT dy,
    LPCRECT scroll,
    LPCRECT clip) {
  TRACK_METHOD();
  // TODO(ruijiang): revisit.
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::AdjustRect(LPRECT rc) {
  TRACK_METHOD();
  // TODO(ruijiang): revisit.
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::OnDefWindowMessage(
    UINT msg,
    WPARAM wparam,
    LPARAM lparam,
    LRESULT* result) {
  TRACK_METHOD();
  // TODO(ruijiang): handle it later.
  return E_NOTIMPL;
}

// IServiceProvider
HRESULT STDMETHODCALLTYPE WebActiveXSite::QueryService(
    REFGUID guid,
    REFIID riid,
    void** object) {
  HRESULT hr = E_FAIL;
  // TODO(ruijiang): We may need to support SID_SWebBrowserApp and
  // IID_IHTMLWindow2 in the future.
  if (guid == IID_IBindHost || guid == IID_IWebBrowserApp)
    hr = container_->QueryInterface(riid, object);
  TRACK_QUERY_INTERFACE(riid, *object != NULL);
  return hr;
}

// IPropertyBag
HRESULT STDMETHODCALLTYPE WebActiveXSite::Read(LPCOLESTR prop_name,
                                               VARIANT* var,
                                               IErrorLog* err_log) {
  unsigned int i;
  for (i = 0; i < initial_params_.size(); ++i) {
    if (_wcsicmp(prop_name, initial_params_[i].name.c_str()) == 0)
      break;
  }
  if (i >= initial_params_.size())
    return E_INVALIDARG;
  if (var->vt == VT_EMPTY || var->vt == VT_BSTR) {
    // We don't need to do any conversion in this case.
    var->vt = VT_BSTR;
    var->bstrVal = ::SysAllocString(initial_params_[i].value.c_str());
    return S_OK;
  } else {
    // We need to try type conversion.
    ScopedVariant org;
    org.vt = VT_BSTR;
    org.bstrVal = ::SysAllocString(initial_params_[i].value.c_str());
    HRESULT hr = VariantChangeType(var, &org, 0, var->vt);
    return FAILED(hr) ? E_FAIL : S_OK;
  }
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::Write(LPCOLESTR prop_name,
                                                VARIANT* var) {
  TRACK_METHOD();
  return E_NOTIMPL;
}

// IPropertyBag2
HRESULT STDMETHODCALLTYPE WebActiveXSite::Read(ULONG c_properties,
                                               PROPBAG2* prop_bag,
                                               IErrorLog* err_log,
                                               VARIANT* value,
                                               HRESULT* error) {
  if (!prop_bag)
    return E_INVALIDARG;
  for (unsigned int i = 0; i < c_properties; ++i) {
    PROPBAG2* p = prop_bag + i;
    value->vt = p->vt;
    HRESULT hr = Read(p->pstrName, value + i, err_log);
    if (error)
      error[i] = hr;
  }
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::Write(ULONG c_properties,
                                                PROPBAG2* prop_bag,
                                                VARIANT* value) {
  TRACK_METHOD();
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::CountProperties(
    ULONG* pc_properties) {
  if (!pc_properties)
    return E_INVALIDARG;
  *pc_properties = static_cast<ULONG>(initial_params_.size());
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::GetPropertyInfo(
    ULONG iproperty,
    ULONG c_properties,
    PROPBAG2* prop_bag,
    ULONG* properties_returned) {
  if (!prop_bag)
    return E_INVALIDARG;
  if (iproperty >= initial_params_.size())
    return E_INVALIDARG;
  unsigned int i;
  for (i = iproperty;
       i < iproperty + c_properties && i < initial_params_.size();
       ++i) {
    PROPBAG2* p = prop_bag + (i - iproperty);
    memset(p, 0, sizeof(PROPBAG2));
    p->dwType = PROPBAG2_TYPE_DATA;
    p->vt = VT_BSTR;
    p->cfType = CF_TEXT;
    p->dwHint = iproperty;
    // According to the document of IPropertyBag2::GetPropertyInfo, here
    // requires a string allocated by CoTaskMemAlloc.
    p->pstrName = CoTaskMemAllocString(initial_params_[i].name);
  }
  if (properties_returned)
    *properties_returned = i - iproperty;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WebActiveXSite::LoadObject(LPCOLESTR name,
                                                     DWORD hint,
                                                     IUnknown* unk_object,
                                                     IErrorLog* err_log) {
  TRACK_METHOD();
  return E_NOTIMPL;
}

}  // namespace activex_shim
