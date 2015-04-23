// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// MainFrm.h : interface of the CMainFrame class
// This file is in Microsoft coding style

#ifndef MEDIA_PLAYER_MAINFRM_H_
#define MEDIA_PLAYER_MAINFRM_H_

const int POPUP_MENU_POSITION = 0;
const int FILE_MENU_POSITION = 0;
const int RECENT_MENU_POSITION = 6;

const wchar_t g_lpcstrMRURegKey[] = L"Software\\Google\\Video\\MediaPlayer";
const wchar_t g_lpcstrApp[] = L"MediaPlayer";


class CMainFrame : public CFrameWindowImpl<CMainFrame>,
    public CUpdateUI<CMainFrame>,
    public CMessageFilter, public CIdleHandler,
    public CPrintJobInfo {
 public:
  DECLARE_FRAME_WND_CLASS(NULL, IDR_MAINFRAME)

  CCommandBarCtrl m_CmdBar;
  CRecentDocumentList m_mru;
  CMruList m_list;
  WtlVideoWindow m_view;

  wchar_t m_szFilePath[MAX_PATH];
  bool enable_exit;

  // printing support
  CPrinter m_printer;
  CDevMode m_devmode;
  CPrintPreviewWindow m_wndPreview;
  CEnhMetaFile m_enhmetafile;
  RECT m_rcMargin;
  bool m_bPrintPreview;

  CMainFrame()
    : m_bPrintPreview(false) {
    ::SetRect(&m_rcMargin, 1000, 1000, 1000, 1000);
    m_printer.OpenDefaultPrinter();
    m_devmode.CopyFromPrinter(m_printer);
    m_szFilePath[0] = 0;
    enable_exit = false;
  }

  virtual BOOL PreTranslateMessage(MSG* pMsg) {
    if (CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg))
      return TRUE;

    return m_view.PreTranslateMessage(pMsg);
  }

  virtual BOOL OnIdle() {
    BOOL bEnable = !m_view.bmp_.IsNull();
    BOOL bMovieOpen = media::Movie::get()->IsOpen() ? true : false;

    float current_position = media::Movie::get()->GetPosition();
    float duration = media::Movie::get()->GetDuration();
    if (enable_exit && bEnable &&
        duration > 0.0f && current_position >= duration) {
      OnFileExit(0, 0, 0);
    }

    UIEnable(ID_FILE_PRINT, bEnable);
    UIEnable(ID_FILE_PRINT_PREVIEW, bEnable);
    UISetCheck(ID_FILE_PRINT_PREVIEW, m_bPrintPreview);
    UIEnable(ID_EDIT_COPY, bEnable);
    UIEnable(ID_EDIT_PASTE, ::IsClipboardFormatAvailable(CF_BITMAP));
    UIEnable(ID_EDIT_CLEAR, bEnable);
    UIEnable(ID_VIEW_QUARTERSIZE, true);
    UIEnable(ID_VIEW_HALFSIZE, true);
    UIEnable(ID_VIEW_NORMALSIZE, true);
    UIEnable(ID_VIEW_DOUBLESIZE, true);
    UIEnable(ID_VIEW_TRIPLESIZE, true);
    UIEnable(ID_VIEW_QUADRUPLESIZE, true);
    UIEnable(ID_VIEW_FITTOSCREEN, false);  // Not currently implemented.
    UIEnable(ID_VIEW_FULLSCREEN, false);  // Not currently implemented.
    UIEnable(ID_VIEW_PROPERTIES, bEnable);
    UIEnable(ID_VIEW_ROTATE0, true);
    UIEnable(ID_VIEW_ROTATE90, true);
    UIEnable(ID_VIEW_ROTATE180, true);
    UIEnable(ID_VIEW_ROTATE270, true);
    UIEnable(ID_VIEW_MIRROR_HORIZONTAL, true);
    UIEnable(ID_VIEW_MIRROR_VERTICAL, true);
    UIEnable(ID_PLAY_PLAY_PAUSE, bMovieOpen);  // If no movie open.
    UIEnable(ID_PLAY_STEP_FORWARD, bMovieOpen);
    UIEnable(ID_PLAY_STEP_BACKWARD, bMovieOpen);
    UIEnable(ID_PLAY_GOTO_START, bMovieOpen);
    UIEnable(ID_PLAY_GOTO_END, bMovieOpen);
    UIEnable(ID_PLAY_GOTO_FRAME, false);  // Not working yet.
    UIEnable(ID_PLAY_HALFSPEED, true);
    UIEnable(ID_PLAY_NORMALSPEED, true);
    UIEnable(ID_PLAY_DOUBLESPEED, true);
    UIEnable(ID_PLAY_TRIPLESPEED, true);
    UIEnable(ID_PLAY_QUADRUPLESPEED, true);
    UIEnable(ID_PLAY_EIGHTSPEED, true);
    UIEnable(ID_PLAY_SIXTEENSPEED, true);
#ifdef _OPENMP
    UIEnable(ID_OPTIONS_OPENMP, true);
#else
    UIEnable(ID_OPTIONS_OPENMP, false);
#endif
    UIEnable(ID_OPTIONS_EXIT, true);  // Not currently implemented.
    UIEnable(ID_OPTIONS_SWSCALER, true);
    UIEnable(ID_OPTIONS_DRAW, true);
    UIEnable(ID_OPTIONS_AUDIO, !bMovieOpen);  // Disable while playing.
    UIEnable(ID_OPTIONS_DUMPYUVFILE, true);

    UISetCheck(ID_RECENT_BTN, m_list.IsWindowVisible());
    UIUpdateToolBar();

    return FALSE;
  }

  void TogglePrintPreview() {
    if (m_bPrintPreview) {  // Close it.
      ATLASSERT(m_hWndClient == m_wndPreview.m_hWnd);

      m_hWndClient = m_view;
      m_view.ShowWindow(SW_SHOW);
      m_wndPreview.DestroyWindow();
    } else {    // display it
      ATLASSERT(m_hWndClient == m_view.m_hWnd);

      m_wndPreview.SetPrintPreviewInfo(m_printer, m_devmode.m_pDevMode,
                                       this, 0, 0);
      m_wndPreview.SetPage(0);

      m_wndPreview.Create(m_hWnd, rcDefault, NULL, 0, WS_EX_CLIENTEDGE);
      m_view.ShowWindow(SW_HIDE);
      m_hWndClient = m_wndPreview;
    }

    m_bPrintPreview = !m_bPrintPreview;
    UpdateLayout();
  }

  void UpdateTitleBar(wchar_t* lpstrTitle) {
    CString strDefault;
    strDefault.LoadString(IDR_MAINFRAME);
    CString strTitle = strDefault;
    if (lpstrTitle != NULL) {
      strTitle = lpstrTitle;
      strTitle += L" - ";
      strTitle += strDefault;
    }
    SetWindowText(strTitle);
  }

  //  Print job info callbacks.
  virtual bool IsValidPage(UINT nPage) {
    return (nPage == 0);  // We have only one page.
  }

  virtual bool PrintPage(UINT nPage, HDC hDC) {
    if (nPage >= 1)    // We have only one page.
      return false;

    if (m_view.bmp_.IsNull())  // We do not have an image.
      return false;

    RECT rcPage =
      { 0, 0,
      ::GetDeviceCaps(hDC, PHYSICALWIDTH) - 2 *
         ::GetDeviceCaps(hDC, PHYSICALOFFSETX),
      ::GetDeviceCaps(hDC, PHYSICALHEIGHT) - 2 *
         ::GetDeviceCaps(hDC, PHYSICALOFFSETY) };

    CDCHandle dc = hDC;
    CClientDC dcScreen(m_hWnd);
    CDC dcMem;
    dcMem.CreateCompatibleDC(dcScreen);
    HBITMAP hBmpOld = dcMem.SelectBitmap(m_view.bmp_);
    int cx = m_view.size_.cx;
    int cy = m_view.size_.cy;

    // Calc scaling factor, so that bitmap is not too small
    // based on the width only, max 3/4 width.
    int nScale = ::MulDiv(rcPage.right, 3, 4) / cx;
    if (nScale == 0)    // too big already
      nScale = 1;
    // Calc margines to center bitmap.
    int xOff = (rcPage.right - nScale * cx) / 2;
    if (xOff < 0)
      xOff = 0;
    int yOff = (rcPage.bottom - nScale * cy) / 2;
    if (yOff < 0)
      yOff = 0;
    // Ensure that preview doesn't go outside of the page.
    int cxBlt = nScale * cx;
    if (xOff + cxBlt > rcPage.right)
      cxBlt = rcPage.right - xOff;
    int cyBlt = nScale * cy;
    if (yOff + cyBlt > rcPage.bottom)
      cyBlt = rcPage.bottom - yOff;

    // Now paint bitmap.
    dc.StretchBlt(xOff, yOff, cxBlt, cyBlt, dcMem, 0, 0, cx, cy, SRCCOPY);

    dcMem.SelectBitmap(hBmpOld);

    return true;
  }

  BEGIN_MSG_MAP_EX(CMainFrame)
    MSG_WM_CREATE(OnCreate)
    MSG_WM_CONTEXTMENU(OnContextMenu)

    COMMAND_ID_HANDLER_EX(ID_FILE_OPEN, OnFileOpen)
    COMMAND_ID_HANDLER_EX(ID_FILE_LAST, OnFileLast)
    COMMAND_RANGE_HANDLER_EX(ID_FILE_MRU_FIRST, ID_FILE_MRU_LAST, OnFileRecent)
    COMMAND_ID_HANDLER_EX(ID_RECENT_BTN, OnRecentButton)
    COMMAND_ID_HANDLER_EX(ID_FILE_PRINT, OnFilePrint);
    COMMAND_ID_HANDLER_EX(ID_FILE_PAGE_SETUP, OnFilePageSetup)
    COMMAND_ID_HANDLER_EX(ID_FILE_PRINT_PREVIEW, OnFilePrintPreview);
    COMMAND_ID_HANDLER_EX(ID_APP_EXIT, OnFileExit)
    COMMAND_ID_HANDLER_EX(ID_EDIT_COPY, OnEditCopy)
    COMMAND_ID_HANDLER_EX(ID_EDIT_PASTE, OnEditPaste)
    COMMAND_ID_HANDLER_EX(ID_EDIT_CLEAR, OnEditClear)
    COMMAND_RANGE_HANDLER_EX(ID_VIEW_QUARTERSIZE, ID_VIEW_FULLSCREEN,
                             OnViewSize)
    COMMAND_ID_HANDLER_EX(ID_VIEW_TOOLBAR, OnViewToolBar)
    COMMAND_ID_HANDLER_EX(ID_VIEW_STATUS_BAR, OnViewStatusBar)
    COMMAND_RANGE_HANDLER_EX(ID_VIEW_ROTATE0, ID_VIEW_MIRROR_VERTICAL,
                             OnViewRotate)
    COMMAND_ID_HANDLER_EX(ID_VIEW_PROPERTIES, OnViewProperties)
    COMMAND_ID_HANDLER_EX(ID_PLAY_PLAY_PAUSE, OnPlayPlayPause)
    COMMAND_ID_HANDLER_EX(ID_PLAY_STEP_FORWARD, OnPlayStepForward)
    COMMAND_ID_HANDLER_EX(ID_PLAY_STEP_BACKWARD, OnPlayStepBackward)
    COMMAND_ID_HANDLER_EX(ID_PLAY_GOTO_START, OnPlayGotoStart)
    COMMAND_ID_HANDLER_EX(ID_PLAY_GOTO_END, OnPlayGotoEnd)
    COMMAND_ID_HANDLER_EX(ID_PLAY_GOTO_FRAME, OnPlayGotoFrame)
    COMMAND_RANGE_HANDLER_EX(ID_PLAY_HALFSPEED, ID_PLAY_SIXTEENSPEED,
                             OnPlaySpeed)
    COMMAND_ID_HANDLER_EX(ID_APP_ABOUT, OnAppAbout)
    COMMAND_ID_HANDLER_EX(ID_OPTIONS_OPENMP, OnOptionsOpenMP)
    COMMAND_ID_HANDLER_EX(ID_OPTIONS_EXIT, OnOptionsExit)
    COMMAND_ID_HANDLER_EX(ID_OPTIONS_SWSCALER, OnOptionsSWScaler)
    COMMAND_ID_HANDLER_EX(ID_OPTIONS_DRAW, OnOptionsDraw)
    COMMAND_ID_HANDLER_EX(ID_OPTIONS_AUDIO, OnOptionsAudio)
    COMMAND_ID_HANDLER_EX(ID_OPTIONS_DUMPYUVFILE, OnOptionsDumpYUVFile)

    CHAIN_MSG_MAP(CUpdateUI<CMainFrame>)
    CHAIN_MSG_MAP(CFrameWindowImpl<CMainFrame>)
  END_MSG_MAP()

  BEGIN_UPDATE_UI_MAP(CMainFrame)
    UPDATE_ELEMENT(ID_FILE_PRINT, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
    UPDATE_ELEMENT(ID_FILE_PRINT_PREVIEW, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
    UPDATE_ELEMENT(ID_EDIT_COPY, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
    UPDATE_ELEMENT(ID_EDIT_PASTE, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
    UPDATE_ELEMENT(ID_EDIT_CLEAR, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
    UPDATE_ELEMENT(ID_VIEW_QUARTERSIZE, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_VIEW_HALFSIZE, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_VIEW_NORMALSIZE, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_VIEW_DOUBLESIZE, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_VIEW_TRIPLESIZE, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_VIEW_QUADRUPLESIZE, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_VIEW_FITTOSCREEN, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_VIEW_FULLSCREEN, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_VIEW_TOOLBAR, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_VIEW_STATUS_BAR, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_VIEW_ROTATE0, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_VIEW_ROTATE90, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_VIEW_ROTATE180, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_VIEW_ROTATE270, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_VIEW_MIRROR_HORIZONTAL, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_VIEW_MIRROR_VERTICAL, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_VIEW_PROPERTIES, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
    UPDATE_ELEMENT(ID_PLAY_PLAY_PAUSE, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
    UPDATE_ELEMENT(ID_PLAY_STEP_FORWARD, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_PLAY_STEP_BACKWARD, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_PLAY_GOTO_START, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_PLAY_GOTO_END, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_PLAY_GOTO_FRAME, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_PLAY_HALFSPEED, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_PLAY_NORMALSPEED, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_PLAY_DOUBLESPEED, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_PLAY_TRIPLESPEED, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_PLAY_QUADRUPLESPEED, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_PLAY_EIGHTSPEED, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_PLAY_SIXTEENSPEED, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_OPTIONS_OPENMP, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_OPTIONS_EXIT, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_OPTIONS_SWSCALER, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_OPTIONS_DRAW, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_OPTIONS_AUDIO, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_OPTIONS_DUMPYUVFILE, UPDUI_MENUPOPUP)
    UPDATE_ELEMENT(ID_RECENT_BTN, UPDUI_TOOLBAR)
  END_UPDATE_UI_MAP()

  void UpdateSizeUICheck() {
    int view_size = m_view.GetViewSize();
    UISetCheck(ID_VIEW_QUARTERSIZE,   (view_size == 0));
    UISetCheck(ID_VIEW_HALFSIZE,      (view_size == 1));
    UISetCheck(ID_VIEW_NORMALSIZE,    (view_size == 2));
    UISetCheck(ID_VIEW_DOUBLESIZE,    (view_size == 3));
    UISetCheck(ID_VIEW_TRIPLESIZE,    (view_size == 4));
    UISetCheck(ID_VIEW_QUADRUPLESIZE, (view_size == 5));
    UISetCheck(ID_VIEW_FITTOSCREEN,   (view_size == 6));
    UISetCheck(ID_VIEW_FULLSCREEN,    (view_size == 7));
  }

  void UpdateSpeedUICheck() {
    if (media::Movie::get()) {
      float play_rate = media::Movie::get()->GetPlayRate();
      UISetCheck(ID_PLAY_HALFSPEED,      (play_rate == 0.5f));
      UISetCheck(ID_PLAY_NORMALSPEED,    (play_rate == 1.0f));
      UISetCheck(ID_PLAY_DOUBLESPEED,    (play_rate == 2.0f));
      UISetCheck(ID_PLAY_TRIPLESPEED,    (play_rate == 3.0f));
      UISetCheck(ID_PLAY_QUADRUPLESPEED, (play_rate == 4.0f));
      UISetCheck(ID_PLAY_EIGHTSPEED,     (play_rate == 8.0f));
      UISetCheck(ID_PLAY_SIXTEENSPEED,   (play_rate == 16.0f));
    }
  }

  void UpdateRotateUICheck() {
    int view_rotate = m_view.GetViewRotate();
    UISetCheck(ID_VIEW_ROTATE0,           (view_rotate == 0));
    UISetCheck(ID_VIEW_ROTATE90,          (view_rotate == 1));
    UISetCheck(ID_VIEW_ROTATE180,         (view_rotate == 2));
    UISetCheck(ID_VIEW_ROTATE270,         (view_rotate == 3));
    UISetCheck(ID_VIEW_MIRROR_HORIZONTAL, (view_rotate == 4));
    UISetCheck(ID_VIEW_MIRROR_VERTICAL,   (view_rotate == 5));
  }

  int OnCreate(LPCREATESTRUCT /*lpCreateStruct*/) {
    // create command bar window
    HWND hWndCmdBar = m_CmdBar.Create(m_hWnd, rcDefault, NULL,
                                      ATL_SIMPLE_CMDBAR_PANE_STYLE);
    // atach menu
    m_CmdBar.AttachMenu(GetMenu());
    // load command bar images
    m_CmdBar.LoadImages(IDR_MAINFRAME);
    // remove old menu
    SetMenu(NULL);

    // create toolbar and rebar
    HWND hWndToolBar = CreateSimpleToolBarCtrl(m_hWnd, IDR_MAINFRAME,
                                               FALSE,
                                               ATL_SIMPLE_TOOLBAR_PANE_STYLE);

    CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
    AddSimpleReBarBand(hWndCmdBar);
    AddSimpleReBarBand(hWndToolBar, NULL, TRUE);

    // create status bar
    CreateSimpleStatusBar();

    // create view window
    m_hWndClient = m_view.Create(m_hWnd, rcDefault, NULL,
                                 WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
                                 WS_CLIPCHILDREN, WS_EX_CLIENTEDGE);

    // set up MRU stuff
    CMenuHandle menu = m_CmdBar.GetMenu();
    CMenuHandle menuFile = menu.GetSubMenu(FILE_MENU_POSITION);
    CMenuHandle menuMru = menuFile.GetSubMenu(RECENT_MENU_POSITION);
    m_mru.SetMenuHandle(menuMru);
    m_mru.SetMaxEntries(12);

    m_mru.ReadFromRegistry(g_lpcstrMRURegKey);

    // create MRU list
    m_list.Create(m_hWnd);

    // set up update UI
    UIAddToolBar(hWndToolBar);
    UISetCheck(ID_VIEW_NORMALSIZE, 1);
    UISetCheck(ID_PLAY_NORMALSPEED, 1);
    UISetCheck(ID_VIEW_TOOLBAR, 1);
    UISetCheck(ID_VIEW_STATUS_BAR, 1);
    UISetCheck(ID_VIEW_ROTATE0, 1);
    UISetCheck(ID_OPTIONS_OPENMP, 0);
    UISetCheck(ID_OPTIONS_EXIT, 0);
    UISetCheck(ID_OPTIONS_DRAW, 1);
    UISetCheck(ID_OPTIONS_AUDIO, 1);
    UpdateSizeUICheck();
    UpdateSpeedUICheck();

    OnOptionsOpenMP(0, 0, 0);  // Toggle openmp off on init.

    CMessageLoop* pLoop = g_module.GetMessageLoop();
    ATLASSERT(pLoop != NULL);
    pLoop->AddMessageFilter(this);
    pLoop->AddIdleHandler(this);

    return 0;
  }

  void OnContextMenu(CWindow wnd, CPoint point) {
    if (wnd.m_hWnd == m_view.m_hWnd) {
      CMenu menu;
      menu.LoadMenu(IDR_CONTEXTMENU);
      CMenuHandle menuPopup = menu.GetSubMenu(POPUP_MENU_POSITION);
      m_CmdBar.TrackPopupMenu(menuPopup, TPM_RIGHTBUTTON | TPM_VERTICAL,
                              point.x, point.y);
    } else {
      SetMsgHandled(FALSE);
    }
  }

  void OnFileExit(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/) {
    PostMessage(WM_CLOSE);
  }

  bool IsMovie(wchar_t* file_name) {
    if (_tcsstr(file_name, L".bmp"))
      return false;
    return true;
  }

  bool MovieOpenFile(wchar_t* file_name) {
    bool success = false;

    if (m_bPrintPreview)
      TogglePrintPreview();

    // If a movie is open, close it.
    media::Movie::get()->Close();

    if (IsMovie(file_name)) {
      success = media::Movie::get()->Open(file_name, m_view.renderer_);
    } else {
      HBITMAP hbmp = NULL;
      hbmp = (HBITMAP)::LoadImage(NULL, file_name, IMAGE_BITMAP, 0, 0,
                                  LR_CREATEDIBSECTION | LR_DEFAULTCOLOR |
                                  LR_LOADFROMFILE);
      if (hbmp) {
        m_view.SetBitmap(hbmp);
        success = true;
      }
    }

    if (success) {
      m_mru.AddToList(file_name);
      m_mru.WriteToRegistry(g_lpcstrMRURegKey);
      UpdateTitleBar(file_name);
      // TODO(fbarchard): Move name into view class.
      lstrcpy(m_szFilePath, file_name);
    } else {
      CString strMsg = L"Can't open movie from:\n";
      strMsg += file_name;
      ::MessageBeep(MB_ICONERROR);
      MessageBox(strMsg, g_lpcstrApp, MB_OK | MB_ICONERROR);
    }
    return success;
  }

  void OnFileOpen(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/) {
    CFileDialog dlg(TRUE, L"bmp", NULL,
        OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
        L"Movie Files (*.mp4;*.ogg;*.ogv)\0"
        L"*.mp4;*.ogg;*.ogv\0"
        L"Audio Files (*.mp3;*.m4a)\0*.mp3;*.m4a\0All Files (*.*)\0*.*\0",
        m_hWnd);
    if (dlg.DoModal() == IDOK) {
      MovieOpenFile(dlg.m_szFileName);
    }
  }

  void OnFileRecent(UINT /*uNotifyCode*/, int nID, CWindow /*wnd*/) {
    // Get file name from the MRU list
    wchar_t file_name[MAX_PATH];
    if (m_mru.GetFromList(nID, file_name, MAX_PATH)) {
      MovieOpenFile(file_name);
    }
  }

  void OnFileLast(UINT uNotifyCode, int /*nID*/, CWindow wnd) {
    OnFileRecent(uNotifyCode, ID_FILE_MRU_FIRST, wnd);
  }

  void OnRecentButton(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/) {
    UINT uBandID = ATL_IDW_BAND_FIRST + 1;  // toolbar is second added band
    CReBarCtrl rebar = m_hWndToolBar;
    int nBandIndex = rebar.IdToIndex(uBandID);
    REBARBANDINFO rbbi = { 0 };
    rbbi.cbSize = RunTimeHelper::SizeOf_REBARBANDINFO();
    rbbi.fMask = RBBIM_CHILD;
    rebar.GetBandInfo(nBandIndex, &rbbi);
    CToolBarCtrl wndToolBar = rbbi.hwndChild;

    int nIndex = wndToolBar.CommandToIndex(ID_RECENT_BTN);
    CRect rect;
    wndToolBar.GetItemRect(nIndex, rect);
    wndToolBar.ClientToScreen(rect);

    // Build and display MRU list in a popup
    m_list.BuildList(m_mru);
    m_list.ShowList(rect.left, rect.bottom);
  }

  void OnFilePrint(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/) {
    CPrintDialog dlg(FALSE);
    dlg.m_pd.hDevMode = m_devmode.CopyToHDEVMODE();
    dlg.m_pd.hDevNames = m_printer.CopyToHDEVNAMES();
    dlg.m_pd.nMinPage = 1;
    dlg.m_pd.nMaxPage = 1;

    if (dlg.DoModal() == IDOK) {
      m_devmode.CopyFromHDEVMODE(dlg.m_pd.hDevMode);
      m_printer.ClosePrinter();
      m_printer.OpenPrinter(dlg.m_pd.hDevNames, m_devmode.m_pDevMode);

      CPrintJob job;
      job.StartPrintJob(false, m_printer, m_devmode.m_pDevMode, this,
                        L"MediaPlayer Document", 0, 0,
                        (dlg.PrintToFile() != FALSE));
    }

    ::GlobalFree(dlg.m_pd.hDevMode);
    ::GlobalFree(dlg.m_pd.hDevNames);
  }

  void OnFilePageSetup(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/) {
    CPageSetupDialog dlg;
    dlg.m_psd.hDevMode = m_devmode.CopyToHDEVMODE();
    dlg.m_psd.hDevNames = m_printer.CopyToHDEVNAMES();
    dlg.m_psd.rtMargin = m_rcMargin;

    if (dlg.DoModal() == IDOK) {
      if (m_bPrintPreview)
        TogglePrintPreview();

      m_devmode.CopyFromHDEVMODE(dlg.m_psd.hDevMode);
      m_printer.ClosePrinter();
      m_printer.OpenPrinter(dlg.m_psd.hDevNames, m_devmode.m_pDevMode);
      m_rcMargin = dlg.m_psd.rtMargin;
    }

    ::GlobalFree(dlg.m_psd.hDevMode);
    ::GlobalFree(dlg.m_psd.hDevNames);
  }

  void OnFilePrintPreview(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/) {
    TogglePrintPreview();
  }

  void OnEditCopy(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/) {
    if (::OpenClipboard(NULL)) {
      HBITMAP hBitmapCopy = (HBITMAP)::CopyImage(m_view.bmp_.m_hBitmap,
                                                 IMAGE_BITMAP, 0, 0, 0);
      if (hBitmapCopy != NULL)
        ::SetClipboardData(CF_BITMAP, hBitmapCopy);
      else
        MessageBox(L"Can't copy frame", g_lpcstrApp, MB_OK | MB_ICONERROR);

      ::CloseClipboard();
    } else {
      MessageBox(L"Can't open clipboard to copy",
                 g_lpcstrApp, MB_OK | MB_ICONERROR);
    }
  }

  void OnEditPaste(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/) {
    if (m_bPrintPreview)
      TogglePrintPreview();

    if (::OpenClipboard(NULL)) {
      HBITMAP hBitmap = (HBITMAP)::GetClipboardData(CF_BITMAP);
      ::CloseClipboard();
      if (hBitmap != NULL) {
        HBITMAP hBitmapCopy = (HBITMAP)::CopyImage(hBitmap, IMAGE_BITMAP,
                                                   0, 0, 0);
        if (hBitmapCopy != NULL) {
          m_view.SetBitmap(hBitmapCopy);
          UpdateTitleBar(L"(Clipboard)");
          m_szFilePath[0] = 0;
        } else {
          MessageBox(L"Can't paste frame",
                     g_lpcstrApp, MB_OK | MB_ICONERROR);
        }
      } else {
        MessageBox(L"Can't open frame from the clipboard",
                   g_lpcstrApp, MB_OK | MB_ICONERROR);
      }
    } else {
      MessageBox(L"Can't open clipboard to paste",
                 g_lpcstrApp, MB_OK | MB_ICONERROR);
    }
  }

  void OnEditClear(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/) {
    if (m_bPrintPreview)
      TogglePrintPreview();

    media::Movie::get()->Close();
    m_view.Reset();
    UpdateTitleBar(NULL);
    m_szFilePath[0] = 0;
  }

  void OnViewSize(UINT /*uNotifyCode*/, int nID, CWindow /*wnd*/) {
    m_view.SetViewSize(nID - ID_VIEW_QUARTERSIZE);
    UpdateSizeUICheck();
    UpdateLayout();
  }

  void OnViewRotate(UINT /*uNotifyCode*/, int nID, CWindow /*wnd*/) {
    m_view.SetViewRotate(nID - ID_VIEW_ROTATE0);
    UpdateRotateUICheck();
    UpdateLayout();
  }

  void OnViewToolBar(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/) {
    static BOOL bNew = TRUE;  // initially visible
    bNew = !bNew;
    UINT uBandID = ATL_IDW_BAND_FIRST + 1;  // toolbar is second added band
    CReBarCtrl rebar = m_hWndToolBar;
    int nBandIndex = rebar.IdToIndex(uBandID);
    rebar.ShowBand(nBandIndex, bNew);
    UISetCheck(ID_VIEW_TOOLBAR, bNew);
    UpdateLayout();
  }

  void OnViewStatusBar(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/) {
    BOOL bNew = !::IsWindowVisible(m_hWndStatusBar);
    ::ShowWindow(m_hWndStatusBar, bNew ? SW_SHOWNOACTIVATE : SW_HIDE);
    UISetCheck(ID_VIEW_STATUS_BAR, bNew);
    UpdateLayout();
  }

  void OnViewProperties(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/) {
    CBmpProperties prop;
    if (lstrlen(m_szFilePath) > 0)  // we have a file name
      prop.SetFileInfo(m_szFilePath, NULL);
    else        // must be clipboard then
      prop.SetFileInfo(NULL, m_view.bmp_.m_hBitmap);
    prop.DoModal();
  }

  void OnPlayPlayPause(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/) {
    bool paused = !media::Movie::get()->GetPause();
    media::Movie::get()->SetPause(paused);
  }

  void OnPlayStepForward(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/) {
    float current_position = media::Movie::get()->GetPosition();
    media::Movie::get()->SetPosition(current_position + 10.0f);
  }

  void OnPlayStepBackward(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/) {
    float current_position = media::Movie::get()->GetPosition();
    media::Movie::get()->SetPosition(current_position - 10.0f);
  }

  void OnPlayGotoStart(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/) {
    media::Movie::get()->SetPosition(0.0);
  }

  void OnPlayGotoEnd(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/) {
    float current_position = media::Movie::get()->GetDuration();
    media::Movie::get()->SetPosition(current_position - 30.0f);
  }

  void SetPlayRate(int play_speed) {
    if (play_speed == 0) {
      media::Movie::get()->Play(0.5f);
    } else if (play_speed == 2) {
      media::Movie::get()->Play(2.0f);
    } else if (play_speed == 3) {
      media::Movie::get()->Play(3.0f);
    } else if (play_speed == 4) {
      media::Movie::get()->Play(4.0f);
    } else if (play_speed == 5) {
      media::Movie::get()->Play(8.0f);
    } else if (play_speed == 6) {
      media::Movie::get()->Play(16.0f);
    } else {
      media::Movie::get()->Play(1.0f);
    }
  }

  void OnPlaySpeed(UINT /*uNotifyCode*/, int nID, CWindow /*wnd*/) {
    int play_speed = nID - ID_PLAY_HALFSPEED;
    SetPlayRate(play_speed);
    UpdateSpeedUICheck();
    UpdateLayout();
  }

  void OnOptionsOpenMP(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/) {
#ifdef _OPENMP
    bool enable_openmp = !media::Movie::get()->GetOpenMpEnable();
    media::Movie::get()->SetOpenMpEnable(enable_openmp);
    UISetCheck(ID_OPTIONS_OPENMP, enable_openmp);
#endif
    UpdateLayout();
  }

  void OnOptionsExit(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/) {
    // TODO(fbarchard): Implement when pipeline exposes properties.
    enable_exit = !enable_exit;
    UISetCheck(ID_OPTIONS_EXIT, enable_exit);
    UpdateLayout();
  }

  void OnOptionsSWScaler(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/) {
    bool enable_swscaler = !media::Movie::get()->GetSwscalerEnable();
    media::Movie::get()->SetSwscalerEnable(enable_swscaler);
    UISetCheck(ID_OPTIONS_SWSCALER, enable_swscaler);
    UpdateLayout();
  }

  void OnOptionsDraw(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/) {
    bool enable_draw = !media::Movie::get()->GetDrawEnable();
    media::Movie::get()->SetDrawEnable(enable_draw);
    UISetCheck(ID_OPTIONS_DRAW, enable_draw);
    UpdateLayout();
  }

  void OnOptionsAudio(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/) {
    bool enable_audio = !media::Movie::get()->GetAudioEnable();
    media::Movie::get()->SetAudioEnable(enable_audio);
    UISetCheck(ID_OPTIONS_AUDIO, enable_audio);
    UpdateLayout();
  }

  void OnOptionsDumpYUVFile(UINT /*uNotify*/, int /*nID*/, CWindow /*wnd*/) {
    bool enable_dump_yuv_file = !media::Movie::get()->GetDumpYuvFileEnable();
    media::Movie::get()->SetDumpYuvFileEnable(enable_dump_yuv_file);
    UISetCheck(ID_OPTIONS_DUMPYUVFILE, enable_dump_yuv_file);
    UpdateLayout();
  }

  void OnAppAbout(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/) {
    CSimpleDialog<IDD_ABOUTBOX> dlg;
    dlg.DoModal();
  }


  void OnPlayGotoFrame(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/) {
    CSeek seek;
    seek.DoModal();
  }
};

#endif  // MEDIA_PLAYER_MAINFRM_H_

