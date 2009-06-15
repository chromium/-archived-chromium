// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Author: yzshen@google.com (Yuzhu Shen)

#ifndef BAR_TOOLBAR_TOOLBAREVENTS_H_
#define BAR_TOOLBAR_TOOLBAREVENTS_H_

#include "cld/bar/toolbar/optionsinterface.h"
#include "common/toolbar_api.h"

class ATL_NO_VTABLE KeyboardEventInfo
    : public CComObjectRootEx<CComMultiThreadModel>,
      public IKeyboardEventInfo {
 public:
  BEGIN_COM_MAP(KeyboardEventInfo)
    COM_INTERFACE_ENTRY(IKeyboardEventInfo)
  END_COM_MAP()

  KeyboardEventInfo() : key_code_(0), flags_(0) {}

  void Set(WPARAM key_code, LPARAM flags);

  STDMETHODIMP get_KeyCode(WPARAM* key_code);
  STDMETHODIMP get_Flags(LPARAM* flags);

 private:
  WPARAM key_code_;
  LPARAM flags_;

  DISALLOW_EVIL_CONSTRUCTORS(KeyboardEventInfo);
};

class ATL_NO_VTABLE MousePointInfo
    : public CComObjectRootEx<CComMultiThreadModel>,
      public IMousePointInfo {
 public:
  BEGIN_COM_MAP(MousePointInfo)
    COM_INTERFACE_ENTRY(IMousePointInfo)
  END_COM_MAP()

  MousePointInfo();

  void set_MousePoint(LONG x_loc, LONG y_loc);
  void set_MousePoint(const POINT& point) { point_ = point; }

  STDMETHODIMP get_MousePoint(POINT* mouse_point);

 private:
  POINT point_;

  DISALLOW_EVIL_CONSTRUCTORS(MousePointInfo);
};

class ATL_NO_VTABLE OptionChangeInfo
    : public CComObjectRootEx<CComMultiThreadModel>,
      public IOptionChangeInfo {
 public:
  BEGIN_COM_MAP(OptionChangeInfo)
    COM_INTERFACE_ENTRY(IOptionChangeInfo)
  END_COM_MAP()

  OptionChangeInfo() : option_id_(0), change_cause_(0) {}

  void Set(IOptions::Option option_id,
           const CString& option_name,
           IOptions::ChangeCause change_cause);

  STDMETHODIMP get_OptionId(LONG *option_id);
  STDMETHODIMP get_OptionName(BSTR* option_name);
  STDMETHODIMP get_ChangeCause(LONG* change_cause);

 private:
  LONG option_id_;
  CString option_name_;
  LONG change_cause_;

  DISALLOW_EVIL_CONSTRUCTORS(OptionChangeInfo);
};

class ATL_NO_VTABLE SidebarWidthChangedInfo
    : public CComObjectRootEx<CComMultiThreadModel>,
      public ISidebarWidthChangedInfo {
 public:
  BEGIN_COM_MAP(SidebarWidthChangedInfo)
    COM_INTERFACE_ENTRY(ISidebarWidthChangedInfo)
    COM_INTERFACE_ENTRY(IToolbarEvent)
  END_COM_MAP()

  SidebarWidthChangedInfo() : width_(0) {}

  void Set(LONG width) { width_ = width; }

  STDMETHODIMP get_Width(LONG* width);

 private:
  LONG width_;

  DISALLOW_EVIL_CONSTRUCTORS(SidebarWidthChangedInfo);
};


class ATL_NO_VTABLE PageSelectionChangedInfo
    : public CComObjectRootEx<CComMultiThreadModel>,
      public IPageSelectionChangedInfo {
 public:
  BEGIN_COM_MAP(PageSelectionChangedInfo)
    COM_INTERFACE_ENTRY(IPageSelectionChangedInfo)
    COM_INTERFACE_ENTRY(IToolbarEvent)
  END_COM_MAP()

  PageSelectionChangedInfo() : is_text_selected_(FALSE) {}

  void SetIsTextSelected(BOOL has_selection) {
      is_text_selected_ = has_selection;
  }

  STDMETHODIMP get_IsTextSelected(BOOL* is_text_selected);

 private:
  BOOL is_text_selected_;

  DISALLOW_EVIL_CONSTRUCTORS(PageSelectionChangedInfo);
};

class ATL_NO_VTABLE ButtonDropdownInfo
    : public CComObjectRootEx<CComMultiThreadModel>,
      public IButtonDropdownInfo {
 public:
  BEGIN_COM_MAP(ButtonDropdownInfo)
    COM_INTERFACE_ENTRY(IButtonDropdownInfo)
    COM_INTERFACE_ENTRY(IToolbarEvent)
  END_COM_MAP()

  ButtonDropdownInfo() : replacement_command_(0) {}

  STDMETHODIMP put_ReplacementCommand(LONG replacement_command) {
    replacement_command_ = replacement_command;
    return S_OK;
  }

  STDMETHODIMP get_ReplacementCommand(LONG *replacement_command) {
    *replacement_command = replacement_command_;
    return S_OK;
  }

 private:
  LONG replacement_command_;

  DISALLOW_EVIL_CONSTRUCTORS(ButtonDropdownInfo);
};
#endif  // BAR_TOOLBAR_TOOLBAREVENTS_H_
