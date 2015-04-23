// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHELL_DIALOGS_H_
#define CHROME_BROWSER_SHELL_DIALOGS_H_

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/gfx/native_widget_types.h"
#include "base/ref_counted.h"
#include "base/string16.h"

namespace gfx {
class Font;
}

// A base class for shell dialogs.
class BaseShellDialog {
 public:
  // Returns true if a shell dialog box is currently being shown modally
  // to the specified owner.
  virtual bool IsRunning(gfx::NativeWindow owning_window) const = 0;

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
    // file/folder path is in |path|. |params| is contextual passed to
    // SelectFile. |index| specifies the index of the filter passed to the
    // the initial call to SelectFile.
    virtual void FileSelected(const FilePath& path,
                              int index, void* params) = 0;

    // Notifies the Listener that many files have been selected. The
    // files are in |files|. |params| is contextual passed to SelectFile.
    virtual void MultiFilesSelected(
      const std::vector<FilePath>& files, void* params) {};

    // Notifies the Listener that the file/folder selection was aborted (via
    // the  user canceling or closing the selection dialog box, for example).
    // |params| is contextual passed to SelectFile.
    virtual void FileSelectionCanceled(void* params) {};
  };

  // Creates a dialog box helper. This object is ref-counted, but the returned
  // object will have no reference (refcount is 0).
  static SelectFileDialog* Create(Listener* listener);

  // Holds information about allowed extensions on a file save dialog.
  // |extensions| is a list of allowed extensions. For example, it might be
  //   { { "htm", "html" }, { "txt" } }. Only pass more than one extension
  //   in the inner vector if the extensions are equivalent. Do NOT include
  //   leading periods.
  // |extension_description_overrides| overrides the system descriptions of the
  //   specified extensions. Entries correspond to |extensions|; if left blank
  //   the system descriptions will be used.
  // |include_all_files| specifies whether there will be a filter added for all
  //   files (i.e. *.*).
  struct FileTypeInfo {
    std::vector<std::vector<FilePath::StringType> > extensions;
    std::vector<string16> extension_description_overrides;
    bool include_all_files;
  };

  // Selects a file. This will start displaying the dialog box. This will also
  // block the calling window until the dialog box is complete. The listener
  // associated with this object will be notified when the selection is
  // complete.
  // |type| is the type of file dialog to be shown, see Type enumeration above.
  // |title| is the title to be displayed in the dialog. If this string is
  //   empty, the default title is used.
  // |default_path| is the default path and suggested file name to be shown in
  //   the dialog. This only works for SELECT_SAVEAS_FILE and SELECT_OPEN_FILE.
  //   Can be an empty string to indicate the platform default.
  // |file_types| holds the infomation about the file types allowed. Pass NULL
  //   to get no special behavior
  // |file_type_index| is the 1-based index into the file type list in
  //   |file_types|. Specify 0 if you don't need to specify extension behavior.
  // |default_extension| is the default extension to add to the file if the
  //   user doesn't type one. This should NOT include the '.'. On Windows, if
  //   you specify this you must also specify |file_types|.
  // |owning_window| is the window the dialog is modal to, or NULL for a
  //   modeless dialog.
  // |params| is data from the calling context which will be passed through to
  //   the listener. Can be NULL.
  // NOTE: only one instance of any shell dialog can be shown per owning_window
  //       at a time (for obvious reasons).
  virtual void SelectFile(Type type,
                          const string16& title,
                          const FilePath& default_path,
                          const FileTypeInfo* file_types,
                          int file_type_index,
                          const FilePath::StringType& default_extension,
                          gfx::NativeWindow owning_window,
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
    virtual void FontSelected(const gfx::Font& font, void* params) = 0;

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
  virtual void SelectFont(gfx::NativeWindow owning_window,
                          void* params) = 0;

  // Same as above - also support specifying the default font selected in the
  // list when the dialog appears.
  virtual void SelectFont(gfx::NativeWindow owning_window,
                          void* params,
                          const std::wstring& font_name,
                          int font_size) = 0;
};

#endif  // CHROME_BROWSER_SHELL_DIALOGS_H_
