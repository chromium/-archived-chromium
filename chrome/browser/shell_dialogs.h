// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHELL_DIALOGS_H_
#define CHROME_BROWSER_SHELL_DIALOGS_H_

#include <string>
#include <vector>

#include "base/gfx/native_widget_types.h"
#include "base/ref_counted.h"

class ChromeFont;

// Helpers to show certain types of Windows shell dialogs in a way that doesn't
// block the UI of the entire app.

// A base class for shell dialogs.
class BaseShellDialog {
 public:
  // Returns true if the a shell dialog box is currently being shown modally
  // to the specified owner.
  virtual bool IsRunning(gfx::NativeView owning_window) const = 0;

  // Notifies the dialog box that the listener has been destroyed and it should
  // no longer be sent notifications.
  virtual void ListenerDestroyed() = 0;
};

// Shows a dialog box for selecting a file or a folder.
class SelectFileDialog
    : public base::RefCountedThreadSafe<SelectFileDialog>,
      public BaseShellDialog {
 public:
  enum Type {
    SELECT_FOLDER,
    SELECT_SAVEAS_FILE,
    SELECT_OPEN_FILE,
    SELECT_OPEN_MULTI_FILE
  };

  virtual ~SelectFileDialog() {}

  // An interface implemented by a Listener object wishing to know about the
  // the result of the Select File/Folder action. These callbacks must be
  // re-entrant.
  class Listener {
   public:
    // Notifies the Listener that a file/folder selection has been made. The
    // file/folder path is in |selected_path|. |params| is contextual passed to
    // SelectFile.
    virtual void FileSelected(const std::wstring& path, void* params) = 0;

    // Notifies the Listener that many files have been selected. The
    // files are in |files|. |params| is contextual passed to SelectFile.
    virtual void MultiFilesSelected(
      const std::vector<std::wstring>& files, void* params) {};

    // Notifies the Listener that the file/folder selection was aborted (via
    // the  user canceling or closing the selection dialog box, for example).
    // |params| is contextual passed to SelectFile.
    virtual void FileSelectionCanceled(void* params) {};
  };

  // Creates a dialog box helper. This object is ref-counted, but the returned
  // object will have no reference (refcount is 0).
  static SelectFileDialog* Create(Listener* listener);

  // Selects a file. This will start displaying the dialog box. This will also
  // block the calling window until the dialog box is complete. The listener
  // associated with this object will be notified when the selection is
  // complete.
  // |type| is the type of file dialog to be shown, see Type enumeration above.
  // |title| is the title to be displayed in the dialog. If this string is
  // empty, the default title is used.
  // |default_path| is the default path and suggested file name to be shown in
  // the dialog. This only works for SELECT_SAVEAS_FILE and SELECT_OPEN_FILE.
  // Can be an empty string to indicate Windows should choose the default to
  // show.
  // |filter| is a null (\0) separated list of alternating filter description
  // and filters and terminated with two nulls.
  // |owning_window| is the window the dialog is modal to, or NULL for a
  // modeless dialog.
  // |default_extension| is the default extension to add to the file if the
  // user doesn't type one. This should NOT include the '.'. If you specify
  // this you must also specify a filter.
  // |params| is data from the calling context which will be passed through to
  // the listener. Can be NULL.
  // NOTE: only one instance of any shell dialog can be shown per owning_window
  //       at a time (for obvious reasons).
  virtual void SelectFile(Type type,
                          const std::wstring& title,
                          const std::wstring& default_path,
                          const std::wstring& filter,
                          const std::wstring& default_extension,
                          gfx::NativeView owning_window,
                          void* params) = 0;
};

// Shows a dialog box for selecting a font.
class SelectFontDialog
    : public base::RefCountedThreadSafe<SelectFileDialog>,
      public BaseShellDialog {
 public:
  virtual ~SelectFontDialog() {}

  // An interface implemented by a Listener object wishing to know about the
  // the result of the Select Font action. These callbacks must be
  // re-entrant.
  class Listener {
   public:
    // Notifies the Listener that a font selection has been made. The font
    // details are supplied in |font|. |params| is contextual passed to
    // SelectFile.
    virtual void FontSelected(const ChromeFont& font, void* params) = 0;

    // Notifies the Listener that the font selection was aborted (via the user
    // canceling or closing the selection dialog box, for example). |params| is
    // contextual passed to SelectFile.
    virtual void FontSelectionCanceled(void* params) {};
  };

  // Creates a dialog box helper. This object is ref-counted, but the returned
  // object will have no reference (refcount is 0).
  static SelectFontDialog* Create(Listener* listener);

  // Selects a font. This will start displaying the dialog box. This will also
  // block the calling window until the dialog box is complete. The listener
  // associated with this object will be notified when the selection is
  // complete.
  // |owning_window| is the window the dialog is modal to, or NULL for a
  // modeless dialog.
  // |params| is data from the calling context which will be passed through to
  // the listener. Can be NULL.
  // NOTE: only one instance of any shell dialog can be shown per owning_window
  //       at a time (for obvious reasons).
  // TODO(beng): support specifying the default font selected in the list when
  //             the dialog appears.
  virtual void SelectFont(gfx::NativeView owning_window,
                          void* params) = 0;

  // Same as above - also support specifying the default font selected in the
  // list when the dialog appears.
  virtual void SelectFont(gfx::NativeView owning_window,
                          void* params,
                          const std::wstring& font_name,
                          int font_size) = 0;
};

#endif  // CHROME_BROWSER_SHELL_DIALOGS_H_
