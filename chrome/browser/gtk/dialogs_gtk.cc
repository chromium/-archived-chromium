// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>
#include <map>
#include <set>

#include "app/l10n_util.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/thread.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/shell_dialogs.h"
#include "grit/generated_resources.h"

// The size of the preview we display for selected image files. We set height
// larger than width because generally there is more free space vertically
// than horiztonally (setting the preview image will alway expand the width of
// the dialog, but usually not the height). The image's aspect ratio will always
// be preserved.
static const int kPreviewWidth = 256;
static const int kPreviewHeight = 512;

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
  virtual void SelectFile(Type type,
                          const string16& title,
                          const FilePath& default_path,
                          const FileTypeInfo* file_types,
                          int file_type_index,
                          const FilePath::StringType& default_extension,
                          gfx::NativeWindow owning_window,
                          void* params);

 private:
  // Add the filters from |file_types_| to |chooser|.
  void AddFilters(GtkFileChooser* chooser);

  // Notifies the listener that a single file was chosen.
  void FileSelected(GtkWidget* dialog, const FilePath& path);

  // Notifies the listener that multiple files were chosen.
  void MultiFilesSelected(GtkWidget* dialog,
                          const std::vector<FilePath>& files);

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

  // Callback for when we update the preview for the selection.
  static void OnUpdatePreview(GtkFileChooser* chooser,
                              SelectFileDialogImpl* dialog);

  // The listener to be notified of selection completion.
  Listener* listener_;

  // A map from dialog windows to the |params| user data associated with them.
  std::map<GtkWidget*, void*> params_map_;

  // The file filters.
  FileTypeInfo file_types_;

  // The index of the default selected file filter.
  // Note: This starts from 1, not 0.
  size_t file_type_index_;

  // The set of all parent windows for which we are currently running dialogs.
  std::set<GtkWindow*> parents_;

  // The type of dialog we are showing the user.
  Type type_;

  // These two variables track where the user last saved a file or opened a
  // file so that we can display future dialogs with the same starting path.
  static FilePath* last_saved_path_;
  static FilePath* last_opened_path_;

  // The GtkImage widget for showing previews of selected images.
  GtkWidget* preview_;

  DISALLOW_COPY_AND_ASSIGN(SelectFileDialogImpl);
};

FilePath* SelectFileDialogImpl::last_saved_path_ = NULL;
FilePath* SelectFileDialogImpl::last_opened_path_ = NULL;

// static
SelectFileDialog* SelectFileDialog::Create(Listener* listener) {
  DCHECK(MessageLoop::current() !=
         g_browser_process->io_thread()->message_loop());
  DCHECK(MessageLoop::current() !=
         g_browser_process->file_thread()->message_loop());
  return new SelectFileDialogImpl(listener);
}

SelectFileDialogImpl::SelectFileDialogImpl(Listener* listener)
    : listener_(listener) {
  if (!last_saved_path_) {
    last_saved_path_ = new FilePath();
    last_opened_path_ = new FilePath();
  }
}

SelectFileDialogImpl::~SelectFileDialogImpl() {
}

bool SelectFileDialogImpl::IsRunning(gfx::NativeWindow parent_window) const {
  return parents_.find(parent_window) != parents_.end();
}

void SelectFileDialogImpl::ListenerDestroyed() {
  listener_ = NULL;
}

// We ignore |default_extension|.
void SelectFileDialogImpl::SelectFile(
    Type type,
    const string16& title,
    const FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const FilePath::StringType& default_extension,
    gfx::NativeWindow owning_window,
    void* params) {
  type_ = type;
  // TODO(estade): on windows, owning_window may be null. But I'm not sure when
  // that's used and how to deal with it here. For now, don't allow it.
  DCHECK(owning_window);
  parents_.insert(owning_window);

  std::string title_string = UTF16ToUTF8(title);

  file_type_index_ = file_type_index;
  if (file_types)
    file_types_ = *file_types;
  else
    file_types_.include_all_files = true;

  GtkWidget* dialog = NULL;
  switch (type) {
    case SELECT_OPEN_FILE:
      DCHECK(default_path.empty());
      dialog = CreateFileOpenDialog(title_string, owning_window);
      break;
    case SELECT_OPEN_MULTI_FILE:
      DCHECK(default_path.empty());
      dialog = CreateMultiFileOpenDialog(title_string, owning_window);
      break;
    case SELECT_SAVEAS_FILE:
      dialog = CreateSaveAsDialog(title_string, default_path, owning_window);
      break;
    default:
      NOTIMPLEMENTED() << "Dialog type " << type << " not implemented.";
      return;
  }

  preview_ = gtk_image_new();
  g_signal_connect(dialog, "update-preview", G_CALLBACK(OnUpdatePreview), this);
  gtk_file_chooser_set_preview_widget(GTK_FILE_CHOOSER(dialog), preview_);

  params_map_[dialog] = params;
  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  gtk_widget_show_all(dialog);
}

void SelectFileDialogImpl::AddFilters(GtkFileChooser* chooser) {
  for (size_t i = 0; i < file_types_.extensions.size(); ++i) {
    GtkFileFilter* filter = NULL;
    for (size_t j = 0; j < file_types_.extensions[i].size(); ++j) {
      // TODO(estade): it's probably preferable to use mime types, but we are
      // passed extensions, so it's much easier to use globs.
      if (!file_types_.extensions[i][j].empty()) {
        if (!filter)
          filter = gtk_file_filter_new();
        gtk_file_filter_add_pattern(filter,
            ("*." + file_types_.extensions[i][j]).c_str());
      }
    }
    // We didn't find any non-empty extensions to filter on.
    if (!filter)
      continue;

    // The description vector may be blank, in which case we are supposed to
    // use some sort of default description based on the filter.
    if (i < file_types_.extension_description_overrides.size()) {
      gtk_file_filter_set_name(filter, UTF16ToUTF8(
          file_types_.extension_description_overrides[i]).c_str());
    } else {
      // TODO(estade): There is no system default filter description so we use
      // the filter itself if the description is blank. This is far from
      // perfect. If we have multiple patterns (such as *.png, *.bmp, etc.),
      // this will only show the first pattern. Also, it would be better to have
      // human readable names like "PNG image" rather than "*.png", particularly
      // since extensions aren't a requirement on linux.
      gtk_file_filter_set_name(filter,
          ("*." + file_types_.extensions[i][0]).c_str());
    }

    gtk_file_chooser_add_filter(chooser, filter);
    if (i == file_type_index_ - 1)
      gtk_file_chooser_set_filter(chooser, filter);
  }

  // Add the *.* filter, but only if we have added other filters (otherwise it
  // is implied).
  if (file_types_.include_all_files && file_types_.extensions.size() > 0) {
    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter, "*");
    gtk_file_filter_set_name(filter,
        WideToUTF8(l10n_util::GetString(IDS_SAVEAS_ALL_FILES)).c_str());
    gtk_file_chooser_add_filter(chooser, filter);
  }
}

void SelectFileDialogImpl::FileSelected(GtkWidget* dialog,
                                        const FilePath& path) {
  if (type_ == SELECT_SAVEAS_FILE) {
    *last_saved_path_ = path.DirName();
  } else {
    DCHECK_EQ(type_, SELECT_OPEN_FILE);
    *last_opened_path_ = path.DirName();
  }

  if (listener_) {
    GtkFileFilter* selected_filter =
        gtk_file_chooser_get_filter(GTK_FILE_CHOOSER(dialog));
    GSList* filters = gtk_file_chooser_list_filters(GTK_FILE_CHOOSER(dialog));
    int idx = g_slist_index(filters, selected_filter);
    g_slist_free(filters);
    listener_->FileSelected(path, idx + 1, PopParamsForDialog(dialog));
  }
  RemoveParentForDialog(dialog);
  gtk_widget_destroy(dialog);
}

void SelectFileDialogImpl::MultiFilesSelected(GtkWidget* dialog,
    const std::vector<FilePath>& files) {
  *last_opened_path_ = files[0].DirName();

  if (listener_)
    listener_->MultiFilesSelected(files, PopParamsForDialog(dialog));
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

  AddFilters(GTK_FILE_CHOOSER(dialog));
  if (!last_opened_path_->empty()) {
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
                                        last_opened_path_->value().c_str());
  }
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

  AddFilters(GTK_FILE_CHOOSER(dialog));
  if (!last_opened_path_->empty()) {
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
                                        last_opened_path_->value().c_str());
  }
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

  AddFilters(GTK_FILE_CHOOSER(dialog));
  // Since we expect that the file will not already exist, we use
  // set_current_folder() followed by set_current_name().
  if (last_saved_path_->empty()) {
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
        default_path.DirName().value().c_str());
  } else {
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
                                        last_saved_path_->value().c_str());
  }
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

// static
void SelectFileDialogImpl::OnSelectMultiFileDialogResponse(
    GtkWidget* dialog, gint response_id,
    SelectFileDialogImpl* dialog_impl) {
  if (IsCancelResponse(response_id)) {
    dialog_impl->FileNotSelected(dialog);
    return;
  }

  GSList* filenames = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
  std::vector<FilePath> filenames_fp;
  for (GSList* iter = filenames; iter != NULL; iter = g_slist_next(iter)) {
    filenames_fp.push_back(FilePath(static_cast<char*>(iter->data)));
    g_free(iter->data);
  }

  g_slist_free(filenames);
  dialog_impl->MultiFilesSelected(dialog, filenames_fp);
}

// static
void SelectFileDialogImpl::OnUpdatePreview(GtkFileChooser* chooser,
                                           SelectFileDialogImpl* dialog) {
  gchar* filename = gtk_file_chooser_get_preview_filename(chooser);
  if (!filename)
    return;
  // This will preserve the image's aspect ratio.
  GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file_at_size(filename, kPreviewWidth,
                                                       kPreviewHeight, NULL);
  g_free(filename);
  if (pixbuf) {
    gtk_image_set_from_pixbuf(GTK_IMAGE(dialog->preview_), pixbuf);
    g_object_unref(pixbuf);
  }
  gtk_file_chooser_set_preview_widget_active(chooser, pixbuf ? TRUE : FALSE);
}
