// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_dialogs.h"

#include <windows.h>
#include <Commdlg.h>
#include <shlobj.h>
#include <atlbase.h>

#include <algorithm>
#include <map>

#include "base/file_util.h"
#include "base/registry.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/win_util.h"
#include "generated_resources.h"

class ShellDialogThread : public base::Thread {
 public:
  ShellDialogThread() : base::Thread("Chrome_ShellDialogThread") { }

 protected:
  void Init() {
    // Initializes the COM library on the current thread.
    CoInitialize(NULL);
  }

  void CleanUp() {
    // Closes the COM library on the current thread. CoInitialize must
    // be balanced by a corresponding call to CoUninitialize.
    CoUninitialize();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellDialogThread);
};

///////////////////////////////////////////////////////////////////////////////
// A base class for all shell dialog implementations that handles showing a
// shell dialog modally on its own thread.
class BaseShellDialogImpl {
 public:
  BaseShellDialogImpl();
  virtual ~BaseShellDialogImpl();

 protected:
  // Represents a run of a dialog.
  struct RunState {
    // Owning HWND, may be null.
    HWND owner;

    // Thread dialog is run on.
    base::Thread* dialog_thread;
  };

  // Called at the beginning of a modal dialog run. Disables the owner window
  // and tracks it. Returns the message loop of the thread that the dialog will
  // be run on.
  RunState BeginRun(HWND owner);

  // Cleans up after a dialog run. If the run_state has a valid HWND this makes
  // sure that the window is enabled. This is essential because BeginRun
  // aggressively guards against multiple modal dialogs per HWND. Must be called
  // on the UI thread after the result of the dialog has been determined.
  //
  // In addition this deletes the Thread in RunState.
  void EndRun(RunState run_state);

  // Returns true if a modal shell dialog is currently active for the specified
  // owner. Must be called on the UI thread.
  bool IsRunningDialogForOwner(HWND owner) const;

  // Disables the window |owner|. Can be run from either the ui or the dialog
  // thread. Can be called on either the UI or the dialog thread. This function
  // is called on the dialog thread after the modal Windows Common dialog
  // functions return because Windows automatically re-enables the owning
  // window when those functions return, but we don't actually want them to be
  // re-enabled until the response of the dialog propagates back to the UI
  // thread, so we disable the owner manually after the Common dialog function
  // returns.
  void DisableOwner(HWND owner);

  // The UI thread's message loop.
  MessageLoop* ui_loop_;

 private:
  // Creates a thread to run a shell dialog on. Each dialog requires its own
  // thread otherwise in some situations where a singleton owns a single
  // instance of this object we can have a situation where a modal dialog in
  // one window blocks the appearance of a modal dialog in another.
  static base::Thread* CreateDialogThread();

  // Enables the window |owner_|. Can only be run from the ui thread.
  void EnableOwner(HWND owner);

  // A list of windows that currently own active shell dialogs for this
  // instance. For example, if the DownloadManager owns an instance of this
  // object and there are two browser windows open both with Save As dialog
  // boxes active, this list will consist of the two browser windows' HWNDs.
  // The derived class must call EndRun once the dialog is done showing to
  // remove the owning HWND from this list.
  // This object is static since it is maintained for all instances of this
  // object - i.e. you can't have a font picker and a file picker open for the
  // same owner, even though they might be represented by different instances
  // of this object.
  // This set only contains non-null HWNDs. NULL hwnds are not added to this
  // list.
  typedef std::set<HWND> Owners;
  static Owners owners_;
  static int instance_count_;

  DISALLOW_COPY_AND_ASSIGN(BaseShellDialogImpl);
};

// static
BaseShellDialogImpl::Owners BaseShellDialogImpl::owners_;
int BaseShellDialogImpl::instance_count_ = 0;

BaseShellDialogImpl::BaseShellDialogImpl()
    : ui_loop_(MessageLoop::current()) {
  ++instance_count_;
}

BaseShellDialogImpl::~BaseShellDialogImpl() {
  // All runs should be complete by the time this is called!
  if (--instance_count_ == 0)
    DCHECK(owners_.empty());
}

BaseShellDialogImpl::RunState BaseShellDialogImpl::BeginRun(HWND owner) {
  // Cannot run a modal shell dialog if one is already running for this owner.
  DCHECK(!IsRunningDialogForOwner(owner));
  // The owner must be a top level window, otherwise we could end up with two
  // entries in our map for the same top level window.
  DCHECK(!owner || owner == GetAncestor(owner, GA_ROOT));
  RunState run_state;
  run_state.dialog_thread = CreateDialogThread();
  run_state.owner = owner;
  if (owner) {
    owners_.insert(owner);
    DisableOwner(owner);
  }
  return run_state;
}

void BaseShellDialogImpl::EndRun(RunState run_state) {
  if (run_state.owner) {
    DCHECK(IsRunningDialogForOwner(run_state.owner));
    EnableOwner(run_state.owner);
    DCHECK(owners_.find(run_state.owner) != owners_.end());
    owners_.erase(run_state.owner);
  }
  DCHECK(run_state.dialog_thread);
  delete run_state.dialog_thread;
}

bool BaseShellDialogImpl::IsRunningDialogForOwner(HWND owner) const {
  return (owner && owners_.find(owner) != owners_.end());
}

void BaseShellDialogImpl::DisableOwner(HWND owner) {
  if (IsWindow(owner))
    EnableWindow(owner, FALSE);
}

// static
base::Thread* BaseShellDialogImpl::CreateDialogThread() {
  base::Thread* thread = new ShellDialogThread;
  bool started = thread->Start();
  DCHECK(started);
  return thread;
}

void BaseShellDialogImpl::EnableOwner(HWND owner) {
  if (IsWindow(owner))
    EnableWindow(owner, TRUE);
}

// Implementation of SelectFileDialog that shows a Windows common dialog for
// choosing a file or folder.
class SelectFileDialogImpl : public SelectFileDialog,
                             public BaseShellDialogImpl {
 public:
  explicit SelectFileDialogImpl(Listener* listener);
  virtual ~SelectFileDialogImpl();

  // SelectFileDialog implementation:
  virtual void SelectFile(Type type, const std::wstring& title,
                          const std::wstring& default_path,
                          const std::wstring& filter,
                          const std::wstring& default_extension,
                          HWND owning_hwnd,
                          void* params);
  virtual bool IsRunning(HWND owning_hwnd) const;
  virtual void ListenerDestroyed();

 private:
  // Shows the file selection dialog modal to |owner| and calls the result
  // back on the ui thread. Run on the dialog thread.
  void ExecuteSelectFile(Type type,
                         const std::wstring& title,
                         const std::wstring& default_path,
                         const std::wstring& filter,
                         const std::wstring& default_extension,
                         RunState run_state,
                         void* params);

  // Notifies the listener that a folder was chosen. Run on the ui thread.
  void FileSelected(const std::wstring& path, void* params, RunState run_state);

  // Notifies listener that multiple files were chosen. Run on the ui thread.
  void MultiFilesSelected(const std::vector<std::wstring>& paths, void* params,
                         RunState run_state);

  // Notifies the listener that no file was chosen (the action was canceled).
  // Run on the ui thread.
  void FileNotSelected(void* params, RunState run_state);

  // Runs a Folder selection dialog box, passes back the selected folder in
  // |path| and returns true if the user clicks OK. If the user cancels the
  // dialog box the value in |path| is not modified and returns false. |title|
  // is the user-supplied title text to show for the dialog box. Run on the
  // dialog thread.
  bool RunSelectFolderDialog(const std::wstring& title,
                             HWND owner,
                             std::wstring* path);

  // Runs an Open file dialog box, with similar semantics for input paramaters
  // as RunSelectFolderDialog.
  bool RunOpenFileDialog(const std::wstring& title,
                         const std::wstring& filters,
                         HWND owner,
                         std::wstring* path);

  // Runs an Open file dialog box that supports multi-select, with similar
  // semantics for input paramaters as RunOpenFileDialog.
  bool RunOpenMultiFileDialog(const std::wstring& title,
                              const std::wstring& filter,
                              HWND owner,
                              std::vector<std::wstring>* paths);

  // The callback function for when the select folder dialog is opened.
  static int CALLBACK BrowseCallbackProc(HWND window, UINT message,
                                         LPARAM parameter,
                                         LPARAM data);

  // The listener to be notified of selection completion.
  Listener* listener_;

  DISALLOW_COPY_AND_ASSIGN(SelectFileDialogImpl);
};

SelectFileDialogImpl::SelectFileDialogImpl(Listener* listener)
    : listener_(listener),
      BaseShellDialogImpl() {
}

SelectFileDialogImpl::~SelectFileDialogImpl() {
}

void SelectFileDialogImpl::SelectFile(Type type,
                                      const std::wstring& title,
                                      const std::wstring& default_path,
                                      const std::wstring& filter,
                                      const std::wstring& default_extension,
                                      HWND owner,
                                      void* params) {
  RunState run_state = BeginRun(owner);
  run_state.dialog_thread->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &SelectFileDialogImpl::ExecuteSelectFile, type,
                        title, default_path, filter, default_extension,
                        run_state, params));
}

bool SelectFileDialogImpl::IsRunning(HWND owning_hwnd) const {
  return listener_ && IsRunningDialogForOwner(owning_hwnd);
}

void SelectFileDialogImpl::ListenerDestroyed() {
  // Our associated listener has gone away, so we shouldn't call back to it if
  // our worker thread returns after the listener is dead.
  listener_ = NULL;
}

void SelectFileDialogImpl::ExecuteSelectFile(
    Type type,
    const std::wstring& title,
    const std::wstring& default_path,
    const std::wstring& filter,
    const std::wstring& default_extension,
    RunState run_state,
    void* params) {
  std::wstring path = default_path;
  bool success = false;
  if (type == SELECT_FOLDER) {
    success = RunSelectFolderDialog(title, run_state.owner, &path);
  } else if (type == SELECT_SAVEAS_FILE) {
    unsigned index = 0;
    success = win_util::SaveFileAsWithFilter(run_state.owner, default_path,
        filter, default_extension, &index, &path);
    DisableOwner(run_state.owner);
  } else if (type == SELECT_OPEN_FILE) {
    success = RunOpenFileDialog(title, filter, run_state.owner, &path);
  } else if (type == SELECT_OPEN_MULTI_FILE) {
    std::vector<std::wstring> paths;
    if (RunOpenMultiFileDialog(title, filter, run_state.owner, &paths)) {
      ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
          &SelectFileDialogImpl::MultiFilesSelected, paths, params, run_state));
      return;
    }
  }

  if (success) {
    ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
        &SelectFileDialogImpl::FileSelected, path, params, run_state));
  } else {
    ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
        &SelectFileDialogImpl::FileNotSelected, params, run_state));
  }
}

void SelectFileDialogImpl::FileSelected(const std::wstring& selected_folder,
                                        void* params,
                                        RunState run_state) {
  if (listener_)
    listener_->FileSelected(selected_folder, params);
  EndRun(run_state);
}

void SelectFileDialogImpl::MultiFilesSelected(
  const std::vector<std::wstring>& selected_files,
  void* params,
  RunState run_state) {
  if (listener_)
    listener_->MultiFilesSelected(selected_files, params);
  EndRun(run_state);
}

void SelectFileDialogImpl::FileNotSelected(void* params, RunState run_state) {
  if (listener_)
    listener_->FileSelectionCanceled(params);
  EndRun(run_state);
}

int CALLBACK SelectFileDialogImpl::BrowseCallbackProc(HWND window,
                                                      UINT message,
                                                      LPARAM parameter,
                                                      LPARAM data)
{
  if (message == BFFM_INITIALIZED) {
    // WParam is TRUE since passing a path.
    // data lParam member of the BROWSEINFO structure.
    SendMessage(window, BFFM_SETSELECTION, TRUE, (LPARAM)data);
  }
  return 0;
}

bool SelectFileDialogImpl::RunSelectFolderDialog(const std::wstring& title,
                                                 HWND owner,
                                                 std::wstring* path) {
  DCHECK(path);

  wchar_t dir_buffer[MAX_PATH + 1];

  bool result = false;
  BROWSEINFO browse_info = {0};
  browse_info.hwndOwner = owner;
  browse_info.lpszTitle = title.c_str();
  browse_info.pszDisplayName = dir_buffer;
  browse_info.ulFlags = BIF_USENEWUI | BIF_RETURNONLYFSDIRS;
  
  if (path->length()) {
    // Highlight the current value.
    browse_info.lParam = (LPARAM)path->c_str();
    browse_info.lpfn = &BrowseCallbackProc;
  }
  
  LPITEMIDLIST list = SHBrowseForFolder(&browse_info);
  DisableOwner(owner);
  if (list) {
    STRRET out_dir_buffer;
    ZeroMemory(&out_dir_buffer, sizeof(out_dir_buffer));
    out_dir_buffer.uType = STRRET_WSTR;
    CComPtr<IShellFolder> shell_folder = NULL;
    if (SHGetDesktopFolder (&shell_folder) == NOERROR) {
      HRESULT hr = shell_folder->GetDisplayNameOf(list, SHGDN_FORPARSING,
                                                 &out_dir_buffer);
      if (SUCCEEDED(hr) && out_dir_buffer.uType == STRRET_WSTR) {
        *path = out_dir_buffer.pOleStr;
        CoTaskMemFree(out_dir_buffer.pOleStr);
        result = true;
      }
      else {
        // Use old way if we don't get what we want.
        wchar_t old_out_dir_buffer[MAX_PATH + 1];
        if (SHGetPathFromIDList(list, old_out_dir_buffer)) {
          *path = old_out_dir_buffer;
          result = true;
        }
      }

      // According to MSDN, win2000 will not resolve shortcuts, so we do it
      // ourself.
      file_util::ResolveShortcut(path);
    }
    CoTaskMemFree(list);
  }
  return result;
}

bool SelectFileDialogImpl::RunOpenFileDialog(
    const std::wstring& title,
    const std::wstring& filter,
    HWND owner,
    std::wstring* path) {
  OPENFILENAME ofn;
  // We must do this otherwise the ofn's FlagsEx may be initialized to random
  // junk in release builds which can cause the Places Bar not to show up!
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = owner;

  wchar_t filename[MAX_PATH];
  base::wcslcpy(filename, path->c_str(), arraysize(filename));

  ofn.lpstrFile = filename;
  ofn.nMaxFile = MAX_PATH;
  // We use OFN_NOCHANGEDIR so that the user can rename or delete the directory
  // without having to close Chrome first.
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

  if (!filter.empty()) {
    ofn.lpstrFilter = filter.c_str();
  }
  bool success = !!GetOpenFileName(&ofn);
  DisableOwner(owner);
  if (success)
    *path = filename;
  return success;
}

bool SelectFileDialogImpl::RunOpenMultiFileDialog(
    const std::wstring& title,
    const std::wstring& filter,
    HWND owner,
    std::vector<std::wstring>* paths) {
  OPENFILENAME ofn;
  // We must do this otherwise the ofn's FlagsEx may be initialized to random
  // junk in release builds which can cause the Places Bar not to show up!
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = owner;

  wchar_t filename[MAX_PATH] = L"";

  ofn.lpstrFile = filename;
  ofn.nMaxFile = MAX_PATH;
  // We use OFN_NOCHANGEDIR so that the user can rename or delete the directory
  // without having to close Chrome first.
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER
               | OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT;

  if (!filter.empty()) {
    ofn.lpstrFilter = filter.c_str();
  }
  bool success = !!GetOpenFileName(&ofn);
  DisableOwner(owner);
  if (success) {
    std::vector<std::wstring> files;
    const wchar_t* selection = ofn.lpstrFile;
    while (*selection) {  // Empty string indicates end of list.
      files.push_back(selection);
      // Skip over filename and null-terminator.
      selection += files.back().length() + 1;
    }
    if (files.empty()) {
      success = false;
    } else if (files.size() == 1) {
      // When there is one file, it contains the path and filename.
      paths->swap(files);
    } else {
      // Otherwise, the first string is the path, and the remainder are
      // filenames.
      std::vector<std::wstring>::iterator path = files.begin();
      for (std::vector<std::wstring>::iterator file = path + 1;
           file != files.end(); ++file) {
        paths->push_back(*path + L'\\' + *file);
      }
    }
  }
  return success;
}

// static
SelectFileDialog* SelectFileDialog::Create(Listener* listener) {
  return new SelectFileDialogImpl(listener);
}

///////////////////////////////////////////////////////////////////////////////
// SelectFontDialogImpl
//  Implementation of SelectFontDialog that shows a Windows common dialog for
//  choosing a font.
class SelectFontDialogImpl : public SelectFontDialog,
                             public BaseShellDialogImpl {
 public:
  explicit SelectFontDialogImpl(Listener* listener);
  virtual ~SelectFontDialogImpl();

  // SelectFontDialog implementation:
  virtual void SelectFont(HWND owning_hwnd, void* params);
  virtual void SelectFont(HWND owning_hwnd,
                          void* params,
                          const std::wstring& font_name,
                          int font_size);
  virtual bool IsRunning(HWND owning_hwnd) const;
  virtual void ListenerDestroyed();

 private:
  // Shows the font selection dialog modal to |owner| and calls the result
  // back on the ui thread. Run on the dialog thread.
  void ExecuteSelectFont(RunState run_state, void* params);

  // Shows the font selection dialog modal to |owner| and calls the result
  // back on the ui thread. Run on the dialog thread.
  void ExecuteSelectFontWithNameSize(RunState run_state,
                                     void* params,
                                     const std::wstring& font_name,
                                     int font_size);

  // Notifies the listener that a font was chosen. Run on the ui thread.
  void FontSelected(LOGFONT logfont, void* params, RunState run_state);

  // Notifies the listener that no font was chosen (the action was canceled).
  // Run on the ui thread.
  void FontNotSelected(void* params, RunState run_state);

  // The listener to be notified of selection completion.
  Listener* listener_;

  DISALLOW_COPY_AND_ASSIGN(SelectFontDialogImpl);
};

SelectFontDialogImpl::SelectFontDialogImpl(Listener* listener)
    : listener_(listener),
      BaseShellDialogImpl() {
}

SelectFontDialogImpl::~SelectFontDialogImpl() {
}

void SelectFontDialogImpl::SelectFont(HWND owner, void* params) {
  RunState run_state = BeginRun(owner);
  run_state.dialog_thread->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &SelectFontDialogImpl::ExecuteSelectFont,
          run_state, params));
}

void SelectFontDialogImpl::SelectFont(HWND owner, void* params,
                                      const std::wstring& font_name,
                                      int font_size) {
  RunState run_state = BeginRun(owner);
  run_state.dialog_thread->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this,
          &SelectFontDialogImpl::ExecuteSelectFontWithNameSize, run_state,
          params, font_name, font_size));
}

bool SelectFontDialogImpl::IsRunning(HWND owning_hwnd) const {
  return listener_ && IsRunningDialogForOwner(owning_hwnd);
}

void SelectFontDialogImpl::ListenerDestroyed() {
  // Our associated listener has gone away, so we shouldn't call back to it if
  // our worker thread returns after the listener is dead.
  listener_ = NULL;
}

void SelectFontDialogImpl::ExecuteSelectFont(RunState run_state, void* params) {
  LOGFONT logfont;
  CHOOSEFONT cf;
  cf.lStructSize = sizeof(cf);
  cf.hwndOwner = run_state.owner;
  cf.lpLogFont = &logfont;
  cf.Flags = CF_SCREENFONTS;
  bool success = !!ChooseFont(&cf);
  DisableOwner(run_state.owner);
  if (success) {
    ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
        &SelectFontDialogImpl::FontSelected, logfont, params, run_state));
  } else {
    ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
        &SelectFontDialogImpl::FontNotSelected, params, run_state));
  }
}

void SelectFontDialogImpl::ExecuteSelectFontWithNameSize(
    RunState run_state, void* params, const std::wstring& font_name,
    int font_size) {
  // Create the HFONT from font name and size.
  HDC hdc = GetDC(NULL);
  long lf_height = -MulDiv(font_size, GetDeviceCaps(hdc, LOGPIXELSY), 72);
  ReleaseDC(NULL, hdc);
  HFONT hf = ::CreateFont(lf_height, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                          font_name.c_str());
  LOGFONT logfont;
  GetObject(hf, sizeof(LOGFONT), &logfont);
  CHOOSEFONT cf;
  cf.lStructSize = sizeof(cf);
  cf.hwndOwner = run_state.owner;
  cf.lpLogFont = &logfont;
  // Limit the list to a reasonable subset of fonts.
  // TODO : get rid of style selector and script selector
  // 1. List only truetype font
  // 2. Exclude vertical fonts (whose names begin with '@')
  // 3. Exclude symbol and OEM fonts
  // 4. Limit the size to [8, 40].
  // See http://msdn.microsoft.com/en-us/library/ms646832(VS.85).aspx
  cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_TTONLY |
      CF_NOVERTFONTS | CF_SCRIPTSONLY | CF_LIMITSIZE;

  // These limits are arbitrary and needs to be revisited. Is it bad
  // to clamp the size at 40 from A11Y point of view?
  cf.nSizeMin = 8;
  cf.nSizeMax = 40;

  bool success = !!ChooseFont(&cf);
  DisableOwner(run_state.owner);
  if (success) {
    ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
        &SelectFontDialogImpl::FontSelected, logfont, params, run_state));
  } else {
    ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
        &SelectFontDialogImpl::FontNotSelected, params, run_state));
  }
}

void SelectFontDialogImpl::FontSelected(LOGFONT logfont,
                                        void* params,
                                        RunState run_state) {
  if (listener_) {
    HFONT font = CreateFontIndirect(&logfont);
    if (font) {
      listener_->FontSelected(ChromeFont::CreateFont(font), params);
      DeleteObject(font);
    } else {
      listener_->FontSelectionCanceled(params);
    }
  }
  EndRun(run_state);
}

void SelectFontDialogImpl::FontNotSelected(void* params, RunState run_state) {
  if (listener_)
    listener_->FontSelectionCanceled(params);
  EndRun(run_state);
}

// static
SelectFontDialog* SelectFontDialog::Create(Listener* listener) {
  return new SelectFontDialogImpl(listener);
}
