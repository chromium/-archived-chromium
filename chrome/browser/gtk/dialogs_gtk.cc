// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>
#include <map>
#include <set>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/shell_dialogs.h"

// Implementation of SelectFileDialog that shows a Gtk common dialog for
// choosing a file or folder.
// This acts as a modal dialog. Ideally we want to only act modally for the
// parent window and allow other toplevel chrome windows to still function while
// the dialog is showing, but we need the GtkWindowGroup or something similar to
// get that, and that API is only available in more recent versions of GTK.
// TODO(port): fix modality: crbug.com/8727
class SelectFileDialogImpl : public SelectFileDialog {
 public:
  explicit SelectFileDialogImpl(Listener* listener);
  virtual ~SelectFileDialogImpl();

  // BaseShellDialog implementation.
  virtual bool IsRunning(gfx::NativeWindow parent_window) const;
  virtual void ListenerDestroyed();

  // SelectFileDialog implementation.
  // |params| is user data we pass back via the Listener interface.
  virtual void SelectFile(Type type, const std::wstring& title,
                          const std::wstring& default_path,
                          const std::wstring& filter,
                          const std::wstring& default_extension,
                          gfx::NativeWindow parent_window,
                          void* params);

 private:
  // Notifies the listener that a single file was chosen.
  void FileSelected(GtkWidget* dialog, const FilePath& path);

  // Notifies the listener that no file was chosen (the action was canceled).
  // Dialog is passed so we can find that |params| pointer that was passed to
  // us when we were told to show the dialog.
  void FileNotSelected(GtkWidget* dialog);

  // Removes and returns the |params| associated with |dialog| from
  // |params_map_|.
  void* PopParamsForDialog(GtkWidget* dialog);

  // Removes and returns the parent associated with |dialog| from |parents_|.
  void RemoveParentForDialog(GtkWidget* dialog);

  // Callback for when the user cancels or closes the dialog.
  static void OnSelectFileDialogResponse(GtkWidget* dialog, gint response_id,
                                         SelectFileDialogImpl* dialog_impl);

  // The listener to be notified of selection completion.
  Listener* listener_;

  // Our parent window.
  gfx::NativeWindow parent_window_;

  // A map from dialog windows to the |params| user data associated with them.
  std::map<GtkWidget*, void*> params_map_;

  // The set of all parent windows for which we are currently running dialogs.
  std::set<GtkWindow*> parents_;

  DISALLOW_COPY_AND_ASSIGN(SelectFileDialogImpl);
};

// static
SelectFileDialog* SelectFileDialog::Create(Listener* listener) {
  return new SelectFileDialogImpl(listener);
}

SelectFileDialogImpl::SelectFileDialogImpl(Listener* listener)
    : listener_(listener) {
}

SelectFileDialogImpl::~SelectFileDialogImpl() {
}

bool SelectFileDialogImpl::IsRunning(gfx::NativeWindow parent_window) const {
  return parents_.find(parent_window) != parents_.end();
}

void SelectFileDialogImpl::ListenerDestroyed() {
  listener_ = NULL;
}

// We ignore |filter| and |default_extension|.
void SelectFileDialogImpl::SelectFile(
    Type type,
    const std::wstring& title,
    const std::wstring& default_path,
    const std::wstring& filter,
    const std::wstring& default_extension,
    gfx::NativeWindow parent_window,
    void* params) {
  // TODO(estade): on windows, parent_window may be null. But I'm not sure when
  // that's used and how to deal with it here. For now, don't allow it.
  DCHECK(parent_window);
  parents_.insert(parent_window);

  if (type != SELECT_SAVEAS_FILE) {
    NOTIMPLEMENTED();
    return;
  }

  // TODO(port): get rid of these conversions when the parameter types are
  // ported.
  std::string title_string = WideToUTF8(title);
  FilePath default_file_path = FilePath::FromWStringHack(default_path);

  GtkWidget* dialog =
      gtk_file_chooser_dialog_new(title_string.c_str(), parent_window,
                                  GTK_FILE_CHOOSER_ACTION_SAVE,
                                  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                  GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                  NULL);
  params_map_[dialog] = params;
  gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
      default_file_path.DirName().value().c_str());
  gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog),
      default_file_path.BaseName().value().c_str());
  gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), FALSE);
  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  g_signal_connect(G_OBJECT(dialog), "response",
                   G_CALLBACK(OnSelectFileDialogResponse), this);
  gtk_widget_show_all(dialog);
}

void SelectFileDialogImpl::FileSelected(GtkWidget* dialog,
    const FilePath& path) {
  void* params = PopParamsForDialog(dialog);
  if (listener_)
    listener_->FileSelected(path.ToWStringHack(), params);
  RemoveParentForDialog(dialog);
  gtk_widget_destroy(dialog);
}

void SelectFileDialogImpl::FileNotSelected(GtkWidget* dialog) {
  void* params = PopParamsForDialog(dialog);
  if (listener_)
    listener_->FileSelectionCanceled(params);
  RemoveParentForDialog(dialog);
  gtk_widget_destroy(dialog);
}

void* SelectFileDialogImpl::PopParamsForDialog(GtkWidget* dialog) {
  std::map<GtkWidget*, void*>::iterator iter = params_map_.find(dialog);
  DCHECK(iter != params_map_.end());
  void* params = iter->second;
  params_map_.erase(iter);
  return params;
}

void SelectFileDialogImpl::RemoveParentForDialog(GtkWidget* dialog) {
  GtkWindow* parent = gtk_window_get_transient_for(GTK_WINDOW(dialog));
  DCHECK(parent);
  std::set<GtkWindow*>::iterator iter = parents_.find(parent);
  DCHECK(iter != parents_.end());
  parents_.erase(iter);
}

// static
void SelectFileDialogImpl::OnSelectFileDialogResponse(
    GtkWidget* dialog, gint response_id,
    SelectFileDialogImpl* dialog_impl) {
  if (response_id == GTK_RESPONSE_CANCEL ||
      response_id == GTK_RESPONSE_DELETE_EVENT) {
    dialog_impl->FileNotSelected(dialog);
    return;
  }

  DCHECK(response_id == GTK_RESPONSE_ACCEPT);
  gchar* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
  dialog_impl->FileSelected(dialog, FilePath(filename));
}
