// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/separator.h"

#if defined(OS_LINUX)
#include "views/controls/native_control_gtk.h"
#elif defined(OS_WIN)
#include "views/controls/native_control_win.h"
#endif
#include "views/widget/widget.h"

namespace views {

#if defined(OS_WIN)
class NativeSeparatorWin : public NativeControlWin {
 public:
  explicit NativeSeparatorWin(Separator* separator) : separator_(separator) {}
  virtual ~NativeSeparatorWin() {}

  // Overridden from NativeControlWin:
  virtual void CreateNativeControl() {
    HWND control_hwnd = CreateWindowEx(GetAdditionalExStyle(), L"STATIC", L"",
                                       WS_CHILD | SS_ETCHEDHORZ | SS_SUNKEN,
                                       0, 0, width(), height(),
                                       separator_->GetWidget()->GetNativeView(),
                                       NULL, NULL, NULL);
    NativeControlCreated(control_hwnd);
  }

 private:
  Separator* separator_;

  DISALLOW_COPY_AND_ASSIGN(NativeSeparatorWin);
};
#elif defined(OS_LINUX)
class NativeSeparatorGtk : public NativeControlGtk {
 public:
  explicit NativeSeparatorGtk(Separator* separator) : separator_(separator) {}
  virtual ~NativeSeparatorGtk() {}

  // Overridden from NativeSeparatorGtk:
  virtual void CreateNativeControl() {
    // TODO(port): create a separator widget and pass to NativeControlCreated.
  }

 private:
  Separator* separator_;

  DISALLOW_COPY_AND_ASSIGN(NativeSeparatorGtk);
};
#endif

// static
const char Separator::kViewClassName[] = "views/Separator";

const int kSeparatorSize = 2;

Separator::Separator() : native_wrapper_(NULL) {
  SetFocusable(false);
}

Separator::~Separator() {
}

////////////////////////////////////////////////////////////////////////////////
// Separator, View overrides:

gfx::Size Separator::GetPreferredSize() {
  return gfx::Size(width(), kSeparatorSize);
}

void Separator::Layout() {
  if (native_wrapper_) {
    int y = static_cast<int>((height() - kSeparatorSize) / 2);
    native_wrapper_->SetBounds(0, y, width(), kSeparatorSize);
    native_wrapper_->Layout();
  }
}

void Separator::ViewHierarchyChanged(bool is_add, View* parent,
                                     View* child) {
  if (is_add && !native_wrapper_ && GetWidget()) {
    CreateNativeWrapper();
    AddChildView(native_wrapper_);
  }
}

std::string Separator::GetClassName() const {
  return kViewClassName;
}

////////////////////////////////////////////////////////////////////////////////
// Separator, private:

void Separator::CreateNativeWrapper() {
#if defined(OS_WIN)
  native_wrapper_ = new NativeSeparatorWin(this);
#elif defined(OS_LINUX)
  native_wrapper_ = new NativeSeparatorGtk(this);
#endif
}

}  // namespace views
