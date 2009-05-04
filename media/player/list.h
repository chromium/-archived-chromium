// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// list.h : class for Recent Files list
// TODO(fbarchard): Remove hungarian notations.

#ifndef MEDIA_PLAYER_LIST_H_
#define MEDIA_PLAYER_LIST_H_

class CMruList : public CWindowImpl<CMruList, CListBox> {
 public:

  CMruList() {
    m_size.cx = 400;
    m_size.cy = 150;
  }

  HWND Create(HWND hWndParent) {
    CWindowImpl<CMruList, CListBox>::Create(hWndParent, rcDefault, NULL,
      WS_POPUP | WS_THICKFRAME | WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
      WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
      WS_EX_CLIENTEDGE);
    if (IsWindow())
      SetFont(AtlGetStockFont(DEFAULT_GUI_FONT));
    return m_hWnd;
  }

  BOOL BuildList(CRecentDocumentList& mru) {
    ATLASSERT(IsWindow());

    ResetContent();

    int nSize = mru.m_arrDocs.GetSize();
    for (int i = 0; i < nSize; i++)
      InsertString(0, mru.m_arrDocs[i].szDocName);  // Reverse order in array.

    if (nSize > 0) {
      SetCurSel(0);
      SetTopIndex(0);
    }

    return TRUE;
  }

  BOOL ShowList(int x, int y) {
    return SetWindowPos(NULL, x, y, m_size.cx, m_size.cy,
                        SWP_NOZORDER | SWP_SHOWWINDOW);
  }

  void HideList() {
    RECT rect;
    GetWindowRect(&rect);
    m_size.cx = rect.right - rect.left;
    m_size.cy = rect.bottom - rect.top;
    ShowWindow(SW_HIDE);
  }

  void FireCommand() {
    int nSel = GetCurSel();
    if (nSel != LB_ERR) {
      ::SetFocus(GetParent());  // Will hide this window.
      ::SendMessage(GetParent(), WM_COMMAND,
                    MAKEWPARAM((WORD)(ID_FILE_MRU_FIRST + nSel),
                               LBN_DBLCLK), (LPARAM)m_hWnd);
    }
  }

  BEGIN_MSG_MAP(CMruList)
    MESSAGE_HANDLER(WM_KEYDOWN, OnKeyDown)
    MESSAGE_HANDLER(WM_LBUTTONDBLCLK, OnLButtonDblClk)
    MESSAGE_HANDLER(WM_KILLFOCUS, OnKillFocus)
    MESSAGE_HANDLER(WM_NCHITTEST, OnNcHitTest)
  END_MSG_MAP()

  LRESULT OnKeyDown(UINT /*uMsg*/,
                    WPARAM wParam,
                    LPARAM /*lParam*/,
                    BOOL& bHandled) {
    if (wParam == VK_RETURN)
      FireCommand();
    else
      bHandled = FALSE;
    return 0;
  }

  LRESULT OnLButtonDblClk(UINT /*uMsg*/,
                          WPARAM /*wParam*/,
                          LPARAM /*lParam*/,
                          BOOL& /*bHandled*/) {
    FireCommand();
    return 0;
  }

  LRESULT OnKillFocus(UINT /*uMsg*/,
                      WPARAM /*wParam*/,
                      LPARAM /*lParam*/,
                      BOOL& /*bHandled*/) {
    HideList();
    return 0;
  }

  LRESULT OnNcHitTest(UINT uMsg,
                      WPARAM wParam,
                      LPARAM lParam,
                      BOOL& /*bHandled*/) {
    LRESULT lRet = DefWindowProc(uMsg, wParam, lParam);
    switch (lRet) {
      case HTLEFT:
      case HTTOP:
      case HTTOPLEFT:
      case HTTOPRIGHT:
      case HTBOTTOMLEFT:
        lRet = HTCLIENT;  // Don't allow resizing here.
        break;
      default:
        break;
    }
    return lRet;
  }

 private:

  SIZE m_size;
};

#endif  // MEDIA_PLAYER_LIST_H_

