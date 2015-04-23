// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// props.h : movie properties dialog
// TODO(fbarchard): Remove hungarian notation.

#ifndef MEDIA_PLAYER_PROPS_H_
#define MEDIA_PLAYER_PROPS_H_

class CFileName : public CWindowImpl<CFileName> {
 public:

  CFileName() : file_name_(NULL) {
  }

  void Init(HWND hwnd, wchar_t* lpstrName) {
    ATLASSERT(::IsWindow(hwnd));
    SubclassWindow(hwnd);

    // Set tooltip.
    tooltip_.Create(m_hWnd);
    ATLASSERT(tooltip_.IsWindow());
    RECT rect;
    GetClientRect(&rect);
    CToolInfo ti(0, m_hWnd, tooltip_id_, &rect, NULL);
    tooltip_.AddTool(&ti);

    // Set text.
    file_name_ = lpstrName;
    if (file_name_ == NULL)
      return;

    CClientDC dc(m_hWnd);  // will not really paint
    HFONT hFontOld = dc.SelectFont(AtlGetDefaultGuiFont());

    RECT rcText = rect;
    dc.DrawText(file_name_, -1, &rcText,
                DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX |
                DT_CALCRECT);
    BOOL bTooLong = (rcText.right > rect.right);
    if (bTooLong)
      tooltip_.UpdateTipText(file_name_, m_hWnd, tooltip_id_);
    tooltip_.Activate(bTooLong);

    dc.SelectFont(hFontOld);

    Invalidate();
    UpdateWindow();
  }

  BEGIN_MSG_MAP(CFileName)
    MESSAGE_RANGE_HANDLER(WM_MOUSEFIRST, WM_MOUSELAST, OnMouseMessage)
    MESSAGE_HANDLER(WM_PAINT, OnPaint)
  END_MSG_MAP()

  LRESULT OnMouseMessage(UINT uMsg, WPARAM wParam, LPARAM lParam,
                         BOOL& bHandled) {
    if (tooltip_.IsWindow()) {
      MSG msg = { m_hWnd, uMsg, wParam, lParam };
      tooltip_.RelayEvent(&msg);
    }
    bHandled = FALSE;
    return 1;
  }

  LRESULT OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/,
                  BOOL& /*bHandled*/) {
    CPaintDC dc(m_hWnd);
    if (file_name_ != NULL) {
      RECT rect;
      GetClientRect(&rect);

      dc.SetTextColor(::GetSysColor(COLOR_WINDOWTEXT));
      dc.SetBkMode(TRANSPARENT);
      HFONT hFontOld = dc.SelectFont(AtlGetDefaultGuiFont());

      dc.DrawText(file_name_, -1, &rect, DT_SINGLELINE | DT_LEFT |
                  DT_VCENTER | DT_NOPREFIX | DT_PATH_ELLIPSIS);

      dc.SelectFont(hFontOld);
    }
    return 0;
  }

  DECLARE_WND_CLASS_EX(NULL, 0, COLOR_3DFACE)

  wchar_t* file_name_;

  enum { tooltip_id_ = 1313 };
  CToolTipCtrl tooltip_;
};

// Movie properties.
// TODO(fbarchard): Add movie duration in seconds.
class CPageOne : public CPropertyPageImpl<CPageOne> {
 public:
  enum { IDD = IDD_PROP_PAGE1 };

  CPageOne() : file_name_(NULL) {
  }

  BEGIN_MSG_MAP(CPageOne)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    CHAIN_MSG_MAP(CPropertyPageImpl<CPageOne>)
  END_MSG_MAP()

  LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/,
                       BOOL& /*bHandled*/) {
    if (file_name_ != NULL) {
      file_location_.Init(GetDlgItem(IDC_FILELOCATION), file_name_);

      WIN32_FIND_DATA find_data;
      HANDLE hFind = ::FindFirstFile(file_name_, &find_data);
      if (hFind != INVALID_HANDLE_VALUE) {
        // TODO(fbarchard): Support files larger than 2 GB
        int nSizeK = (find_data.nFileSizeLow / 1024);
        if (nSizeK == 0 && find_data.nFileSizeLow != 0)
          nSizeK = 1;
        wchar_t szBuff[100];
        wsprintf(szBuff, L"%i KB", nSizeK);
        SetDlgItemText(IDC_FILESIZE, szBuff);

        // TODO(fbarchard): We need a pipeline property for frame rate.
        float duration = media::Movie::get()->GetDuration();
        float fps = 29.97f;
        wsprintf(szBuff, L"%i.%2i Seconds, %i Frames",
                 static_cast<int>(duration),
                 static_cast<int>(duration * 100) % 100,
                 static_cast<int>(duration * fps));
        SetDlgItemText(IDC_FILEDURATION, szBuff);

        SYSTEMTIME st;
        ::FileTimeToSystemTime(&find_data.ftCreationTime, &st);
        ::GetDateFormat(LOCALE_USER_DEFAULT, 0, &st,
                        L"dddd, MMMM dd',' yyyy',' ",
                        szBuff, sizeof(szBuff) / sizeof(szBuff[0]));
        wchar_t szBuff1[50];
        ::GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st,
                        L"hh':'mm':'ss tt", szBuff1,
                        sizeof(szBuff1) / sizeof(szBuff1[0]));
        lstrcat(szBuff, szBuff1);
        SetDlgItemText(IDC_FILEDATE, szBuff);

        szBuff[0] = 0;
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE) != 0)
          lstrcat(szBuff, L"Archive, ");
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0)
          lstrcat(szBuff, L"Read-only, ");
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0)
          lstrcat(szBuff, L"Hidden, ");
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) != 0)
          lstrcat(szBuff, L"System");
        int nLen = lstrlen(szBuff);
        if (nLen >= 2 && szBuff[nLen - 2] == L',')
          szBuff[nLen - 2] = 0;
        SetDlgItemText(IDC_FILEATTRIB, szBuff);

        ::FindClose(hFind);
      }
    } else {
      SetDlgItemText(IDC_FILELOCATION, L"(Clipboard)");
      SetDlgItemText(IDC_FILESIZE, L"N/A");
      SetDlgItemText(IDC_FILEDATE, L"N/A");
      SetDlgItemText(IDC_FILEATTRIB, L"N/A");
    }
    return TRUE;
  }

  CFileName file_location_;
  wchar_t* file_name_;
};

// Frame properties.
// TODO(fbarchard): This is not implemented for movies yet.
class CPageTwo : public CPropertyPageImpl<CPageTwo> {
 public:
  enum { IDD = IDD_PROP_PAGE2 };

  CPageTwo() : file_name_(NULL) {
  }

  BEGIN_MSG_MAP(CPageTwo)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    CHAIN_MSG_MAP(CPropertyPageImpl<CPageTwo>)
  END_MSG_MAP()

  LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/,
                       BOOL& /*bHandled*/) {
    // Special - remove unused buttons, move Close button, center wizard
    CPropertySheetWindow sheet = GetPropertySheet();
#if !defined(_AYGSHELL_H_) && !defined(__AYGSHELL_H__)  // PPC specific.
    sheet.CancelToClose();
    RECT rect;
    CButton btnCancel = sheet.GetDlgItem(IDCANCEL);
    btnCancel.GetWindowRect(&rect);
    sheet.ScreenToClient(&rect);
    btnCancel.ShowWindow(SW_HIDE);
    CButton btnClose = sheet.GetDlgItem(IDOK);
    btnClose.SetWindowPos(NULL, &rect, SWP_NOZORDER | SWP_NOSIZE);
    sheet.CenterWindow(GetPropertySheet().GetParent());

    sheet.ModifyStyleEx(WS_EX_CONTEXTHELP, 0);
#endif  // (_AYGSHELL_H_) || defined(__AYGSHELL_H__)  // PPC specific.

    // Get and display movie prperties.
    SetDlgItemText(IDC_TYPE, L"MP4 Movie");
    wchar_t* lpstrCompression = L"H.264";

    if (file_name_ != NULL) {
      HANDLE hFile = ::CreateFile(file_name_, GENERIC_READ,
                                  FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL |
                                  FILE_FLAG_SEQUENTIAL_SCAN, NULL);
      ATLASSERT(hFile != INVALID_HANDLE_VALUE);
      if (hFile != INVALID_HANDLE_VALUE) {
        DWORD dwRead = 0;
        BITMAPFILEHEADER bfh;
        ::ReadFile(hFile, &bfh, sizeof(bfh), &dwRead, NULL);
        BITMAPINFOHEADER bih;
        ::ReadFile(hFile, &bih, sizeof(bih), &dwRead, NULL);
        ::CloseHandle(hFile);

        SetDlgItemInt(IDC_WIDTH, bih.biWidth);
        SetDlgItemInt(IDC_HEIGHT, bih.biHeight);
        SetDlgItemInt(IDC_HORRES, ::MulDiv(bih.biXPelsPerMeter, 254, 10000));
        SetDlgItemInt(IDC_VERTRES, ::MulDiv(bih.biYPelsPerMeter, 254, 10000));
        SetDlgItemInt(IDC_BITDEPTH, bih.biBitCount);

        switch (bih.biCompression) {
        case BI_RLE4:
        case BI_RLE8:
          lpstrCompression = L"RLE";
          break;
        case BI_BITFIELDS:
          lpstrCompression = L"Uncompressed with bitfields";
          break;
        case BI_JPEG:
        case BI_PNG:
          lpstrCompression = L"Unknown";
          break;
        }
        SetDlgItemText(IDC_COMPRESSION, lpstrCompression);
      }
    } else {   // Must be pasted from the clipboard.
      ATLASSERT(!bmp_.IsNull());
      BITMAP bitmap = { 0 };
      bool result = bmp_.GetBitmap(bitmap);
      ATLASSERT(result);
      if (result) {
        CClientDC dc(NULL);
        SetDlgItemInt(IDC_WIDTH, bitmap.bmWidth);
        SetDlgItemInt(IDC_HEIGHT, bitmap.bmHeight);
        // Should we use screen resolution here???
        SetDlgItemInt(IDC_HORRES, dc.GetDeviceCaps(LOGPIXELSX));
        SetDlgItemInt(IDC_VERTRES, dc.GetDeviceCaps(LOGPIXELSX));
        SetDlgItemInt(IDC_BITDEPTH, bitmap.bmBitsPixel);
        SetDlgItemText(IDC_COMPRESSION, lpstrCompression);
      }
    }
    return TRUE;
  }

  CBitmapHandle bmp_;
  wchar_t* file_name_;
};

// Screen properties.
class CPageThree : public CPropertyPageImpl<CPageThree> {
 public:
  enum { IDD = IDD_PROP_PAGE3 };

  BEGIN_MSG_MAP(CPageThree)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    CHAIN_MSG_MAP(CPropertyPageImpl<CPageThree>)
  END_MSG_MAP()

  LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/,
                       BOOL& /*bHandled*/) {
    // Get and set screen properties.
    CClientDC dc(NULL);
    SetDlgItemInt(IDC_WIDTH, dc.GetDeviceCaps(HORZRES));
    SetDlgItemInt(IDC_HEIGHT, dc.GetDeviceCaps(VERTRES));
    SetDlgItemInt(IDC_HORRES, dc.GetDeviceCaps(LOGPIXELSX));
    SetDlgItemInt(IDC_VERTRES, dc.GetDeviceCaps(LOGPIXELSY));
    SetDlgItemInt(IDC_BITDEPTH, dc.GetDeviceCaps(BITSPIXEL));

    return TRUE;
  }
};

// TODO(fbachard): Frame properties only work for images, so
// this tab is removed until movie frame properties can be
// added.
class CBmpProperties : public CPropertySheetImpl<CBmpProperties> {
 public:

  CBmpProperties() {
    m_psh.dwFlags |= PSH_NOAPPLYNOW;

    AddPage(page1_);
    // AddPage(page2_);  // Not implemented for movies yet.
    AddPage(page3_);
    SetActivePage(0);
    SetTitle(L"Movie Properties");
  }

  void SetFileInfo(wchar_t* file_path, HBITMAP hBitmap) {
    page1_.file_name_ = file_path;
    page2_.file_name_ = file_path;
    page2_.bmp_ = hBitmap;
  }

  BEGIN_MSG_MAP(CBmpProperties)
    CHAIN_MSG_MAP(CPropertySheetImpl<CBmpProperties>)
  END_MSG_MAP()

  CPageOne page1_;
  CPageTwo page2_;
  CPageThree page3_;
};

#endif  // MEDIA_PLAYER_PROPS_H_

