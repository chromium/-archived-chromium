// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_STATUS_VIEW_H_
#define CHROME_BROWSER_TAB_CONTENTS_STATUS_VIEW_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atlctrls.h>
#include <atlmisc.h>
#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/tab_contents/tab_contents.h"

typedef CWinTraits<WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS>
    StatusViewTraits;

// A base class for about:network, about:ipc etc.  It handles creating a row of
// buttons at the top of the page.  Derived classes get a rect of the remaining
// area and can create their own controls there.
class StatusView : public TabContents,
                   public CWindowImpl<StatusView, CWindow, StatusViewTraits> {
 public:
  StatusView(TabContentsType type);

  BEGIN_MSG_MAP(StatusView)
    MSG_WM_CREATE(OnCreate)
    MSG_WM_ERASEBKGND(OnEraseBkgnd)
    MSG_WM_SIZE(OnSize)
  END_MSG_MAP()

  virtual void CreateView();
  virtual HWND GetContainerHWND() const { return m_hWnd; }

  // Derived classes should implement the following functions
  // TabContents override, to set the page title.
  // virtual const std::wstring GetDefaultTitle() = 0;
  // Gives a rect whose top left corner is after the buttons.  The size of the
  // controls that are added by derived classes will be set in the next OnSize,
  // for now can use any height/width.
  virtual void OnCreate(const CRect& rect) = 0;
  virtual void OnSize(const CRect& rect) = 0;

 protected:
  // Should be deleted via CloseContents.
  virtual ~StatusView();

  // Creates and adds a button to the top row of the page.  Button ids should
  // be unique and start at 101.
  void CreateButton(int id, const wchar_t* title);
  void SetButtonText(int id, const wchar_t* title);

  static const int kLayoutPadding;
  static const int kButtonWidth;
  static const int kButtonHeight;

 private:
  // FocusTraversal Implementation
  // TODO (jcampan): make focus traversal work
  views::View* FindNextFocusableView(views::View* starting_view, bool reverse,
                                     bool dont_loop) {
    return NULL;
  }

  // Event handlers
  LRESULT OnCreate(LPCREATESTRUCT create_struct);
  void OnSize(UINT size_command, const CSize& new_size);
  LRESULT OnEraseBkgnd(HDC hdc);

  struct ButtonInfo {
    CButton* button;
    int id;
  };

  std::vector<ButtonInfo> buttons_;

  DISALLOW_EVIL_CONSTRUCTORS(StatusView);
};

#endif  // #ifndef CHROME_BROWSER_TAB_CONTENTS_STATUS_VIEW_H_

