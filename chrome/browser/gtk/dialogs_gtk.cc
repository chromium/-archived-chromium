// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>
#include <map>
#include <set>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
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
                          int filter_index,
                          const std::wstring& default_extension,
                          gfx::NativeWindow parent_window,
                          void* params);

 private:
  // Notifies the listener that a single file was chosen.
  void FileSelected(GtkWidget* dialog, const FilePath& path);

  // Notifies the listener that multiple files were chosen.
  // TODO(estade): this should deal in FilePaths.
  void MultiFilesSelected(GtkWidget* dialog,
                          const std::vector<std::wstring>& files);

  // Notifies the listener that no file was chosen (the action was canceled).
  // Dialog is passed so we can find that |params| pointer that was passed to
  // us when we were told to show the dialog.
  void FileNotSelected(GtkWidget* dialog);

  GtkWidget* CreateFileOpenDialog(const std::string& title,
      gfx::NativeWindow parent);

  GtkWidget* CreateMultiFileOpenDialog(const std::string& title,
      gfx::NativeWindow parent);

  GtkWidget* CreateSaveAsDialog(const std::string& title,
      const FilePath& default_path, gfx::NativeWindow parent);

  // Removes and returns the |params| associated with |dialog| from
  // |params_map_|.
  void* PopParamsForDialog(GtkWidget* dialog);

  // Removes and returns the parent associated with |dialog| from |parents_|.
  void RemoveParentForDialog(GtkWidget* dialog);

  // Check whether response_id corresponds to the user cancelling/closing the
  // dialog. Used as a helper for the below callbacks.
  static bool IsCancelResponse(gint response_id);

  // Callback for when the user responds to a Save As or Open File dialog.
  static void OnSelectSingleFileDialogResponse(
      GtkWidget* dialog, gint response_id, SelectFileDialogImpl* dialog_impl);

  // Callback for when the user responds to a Open Multiple Files dialog.
  static void OnSelectMultiFileDialogResponse(
      GtkWidget* dialog, gint response_id, SelectFileDialogImpl* dialog_impl);

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
// TODO(estade): use |filter|.
void SelectFileDialogImpl::SelectFile(
    Type type,
    const std::wstring& title,
    const std::wstring& default_path,
    const std::wstring& filter,
    int filter_index,
    const std::wstring& default_extension,
    gfx::NativeWindow parent_window,
    void* params) {
  // TODO(estade): on windows, parent_window may be null. But I'm not sure when
  // that's used and how to deal with it here. For now, don't allow it.
  DCHECK(parent_window);
  parents_.insert(parent_window);

  // TODO(port): get rid of these conversions when the parameter types are
  // ported.
  std::string title_string = WideToUTF8(title);
  FilePath default_file_path = FilePath::FromWStringHack(default_path);

  GtkWidget* dialog = NULL;
  switch (type) {
    case SELECT_OPEN_FILE:
      DCHECK(default_path.empty());
      dialog = CreateFileOpenDialog(title_string, parent_window);
      break;
    case SELECT_OPEN_MULTI_FILE:
      DCHECK(default_path.empty());
      dialog = CreateMultiFileOpenDialog(title_string, parent_window);
      break;
    case SELECT_SAVEAS_FILE:
      dialog = CreateSaveAsDialog(title_string, default_file_path,
                                  parent_window);
      break;
    default:
      NOTIMPLEMENTED() << "Dialog type " << type << " not implemented.";
      return;
  }

  params_map_[dialog] = params;
  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  gtk_widget_show_all(dialog);
}

void SelectFileDialogImpl::FileSelected(GtkWidget* dialog,
    const FilePath& path) {
  void* params = PopParamsForDialog(dialog);
  if (listener_)
    listener_->FileSelected(path.ToWStringHack(), 1, params);
  RemoveParentForDialog(dialog);
  gtk_widget_destroy(dialog);
}

void SelectFileDialogImpl::MultiFilesSelected(GtkWidget* dialog,
    const std::vector<std::wstring>& files) {
  void* params = PopParamsForDialog(dialog);
  if (listener_)
    listener_->MultiFilesSelected(files, params);
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

GtkWidget* SelectFileDialogImpl::CreateFileOpenDialog(const std::string& title,
    gfx::NativeWindow parent) {
  // TODO(estade): do we want to set the open directory to some sort of default?
  GtkWidget* dialog =
      gtk_file_chooser_dialog_new(title.c_str(), parent,
                                  GTK_FILE_CHOOSER_ACTION_OPEN,
                                  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                  GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                  NULL);
  gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), FALSE);
  g_signal_connect(G_OBJECT(dialog), "response",
                   G_CALLBACK(OnSelectSingleFileDialogResponse), this);
  return dialog;
}

GtkWidget* SelectFileDialogImpl::CreateMultiFileOpenDialog(
    const std::string& title, gfx::NativeWindow parent) {
  // TODO(estade): do we want to set the open directory to some sort of default?
  GtkWidget* dialog =
      gtk_file_chooser_dialog_new(title.c_str(), parent,
                                  GTK_FILE_CHOOSER_ACTION_OPEN,
                                  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                  GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                  NULL);
  gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);
  g_signal_connect(G_OBJECT(dialog), "response",
                   G_CALLBACK(OnSelectMultiFileDialogResponse), this);
  return dialog;
}

GtkWidget* SelectFileDialogImpl::CreateSaveAsDialog(const std::string& title,
    const FilePath& default_path, gfx::NativeWindow parent) {
  GtkWidget* dialog =
      gtk_file_chooser_dialog_new(title.c_str(), parent,
                                  GTK_FILE_CHOOSER_ACTION_SAVE,
                                  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                  GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                  NULL);
  // Since we expect that the file will not already exist, we use
  // set_current_folder() followed by set_current_name().
  gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
      default_path.DirName().value().c_str());
  gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog),
      default_path.BaseName().value().c_str());
  gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), FALSE);
  g_signal_connect(G_OBJECT(dialog), "response",
                   G_CALLBACK(OnSelectSingleFileDialogResponse), this);
  return dialog;
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
bool SelectFileDialogImpl::IsCancelResponse(gint response_id) {
  bool is_cancel = response_id == GTK_RESPONSE_CANCEL ||
                   response_id == GTK_RESPONSE_DELETE_EVENT;
  if (is_cancel)
    return true;

  DCHECK(response_id == GTK_RESPONSE_ACCEPT);
  return false;
}

// static
void SelectFileDialogImpl::OnSelectSingleFileDialogResponse(
    GtkWidget* dialog, gint response_id,
    SelectFileDialogImpl* dialog_impl) {
  if (IsCancelResponse(response_id)) {
    dialog_impl->FileNotSelected(dialog);
    return;
  }

  gchar* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
  dialog_impl->FileSelected(dialog, FilePath(filename));
}

void SelectFileDialogImpl::OnSelectMultiFileDialogResponse(
    GtkWidget* dialog, gint response_id,
    SelectFileDialogImpl* dialog_impl) {
  if (IsCancelResponse(response_id)) {
    dialog_impl->FileNotSelected(dialog);
    return;
  }

  GSList* filenames = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
  std::vector<std::wstring> filenames_w;
  for (GSList* iter = filenames; iter != NULL; iter = g_slist_next(iter)) {
    filenames_w.push_back(base::SysNativeMBToWide(
        static_cast<char*>(iter->data)));
    g_free(iter->data);
  }

  g_slist_free(filenames);
  dialog_impl->MultiFilesSelected(dialog, filenames_w);
}
