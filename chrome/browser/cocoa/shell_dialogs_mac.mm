// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_dialogs.h"

#include <CoreServices/CoreServices.h>
#import <Cocoa/Cocoa.h>
#include <map>
#include <set>

#include "base/logging.h"
#include "base/scoped_cftyperef.h"
#import "base/scoped_nsobject.h"
#include "base/sys_string_conversions.h"

static const int kFileTypePopupTag = 1234;

class SelectFileDialogImpl;

// A bridge class to act as the modal delegate to the save/open sheet and send
// the results to the C++ class.
@interface SelectFileDialogBridge : NSObject {
 @private
  SelectFileDialogImpl* selectFileDialogImpl_;  // WEAK; owns us
}

- (id)initWithSelectFileDialogImpl:(SelectFileDialogImpl*)s;
- (void)endedPanel:(NSSavePanel *)panel
        withReturn:(int)returnCode
           context:(void *)context;

@end

// Implementation of SelectFileDialog that shows Cocoa dialogs for choosing a
// file or folder.
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

  // Callback from ObjC bridge.
  void FileWasSelected(NSPanel* dialog,
                       NSWindow* parent_window,
                       bool was_cancelled,
                       bool is_multi,
                       const std::vector<FilePath>& files,
                       int index);

  struct SheetContext {
    Type type;
    NSWindow* owning_window;
  };

 private:
  // Gets the accessory view for the save dialog.
  NSView* GetAccessoryView(const FileTypeInfo* file_types,
                           int file_type_index);

  // The listener to be notified of selection completion.
  Listener* listener_;

  // The bridge for results from Cocoa to return to us.
  scoped_nsobject<SelectFileDialogBridge> bridge_;

  // A map from file dialogs to the |params| user data associated with them.
  std::map<NSPanel*, void*> params_map_;

  // The set of all parent windows for which we are currently running dialogs.
  std::set<NSWindow*> parents_;

  DISALLOW_COPY_AND_ASSIGN(SelectFileDialogImpl);
};

// static
SelectFileDialog* SelectFileDialog::Create(Listener* listener) {
  return new SelectFileDialogImpl(listener);
}

SelectFileDialogImpl::SelectFileDialogImpl(Listener* listener)
    : listener_(listener),
      bridge_([[SelectFileDialogBridge alloc]
               initWithSelectFileDialogImpl:this]) {
}

SelectFileDialogImpl::~SelectFileDialogImpl() {
}

bool SelectFileDialogImpl::IsRunning(gfx::NativeWindow parent_window) const {
  return parents_.find(parent_window) != parents_.end();
}

void SelectFileDialogImpl::ListenerDestroyed() {
  listener_ = NULL;
}

void SelectFileDialogImpl::SelectFile(
    Type type,
    const string16& title,
    const FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const FilePath::StringType& default_extension,
    gfx::NativeWindow owning_window,
    void* params) {
  DCHECK(type == SELECT_FOLDER ||
         type == SELECT_OPEN_FILE ||
         type == SELECT_OPEN_MULTI_FILE ||
         type == SELECT_SAVEAS_FILE);
  DCHECK(owning_window);
  parents_.insert(owning_window);

  NSSavePanel* dialog;
  if (type == SELECT_SAVEAS_FILE)
    dialog = [NSSavePanel savePanel];
  else
    dialog = [NSOpenPanel openPanel];

  if (!title.empty())
    [dialog setTitle:base::SysUTF16ToNSString(title)];

  NSString* default_dir = nil;
  NSString* default_filename = nil;
  if (!default_path.empty()) {
    default_dir = base::SysUTF8ToNSString(default_path.DirName().value());
    default_filename = base::SysUTF8ToNSString(default_path.BaseName().value());
  }

  NSMutableArray* allowed_file_types = nil;
  if (file_types) {
    if (!file_types->extensions.empty()) {
      allowed_file_types = [NSMutableArray array];
      for (size_t i=0; i < file_types->extensions.size(); ++i) {
        const std::vector<FilePath::StringType>& ext_list =
            file_types->extensions[i];
        for (size_t j=0; j < ext_list.size(); ++j) {
          [allowed_file_types addObject:base::SysUTF8ToNSString(ext_list[j])];
        }
      }
    }
    if (type == SELECT_SAVEAS_FILE)
      [dialog setAllowedFileTypes:allowed_file_types];
    // else we'll pass it in when we run the open panel

    if (file_types->include_all_files)
      [dialog setAllowsOtherFileTypes:YES];

    if (!file_types->extension_description_overrides.empty()) {
      NSView* accessory_view = GetAccessoryView(file_types, file_type_index);
      [dialog setAccessoryView:accessory_view];
    }
  } else {
    // If no type info is specified, anything goes.
    [dialog setAllowsOtherFileTypes:YES];
  }

  if (!default_extension.empty())
    [dialog setRequiredFileType:base::SysUTF8ToNSString(default_extension)];

  params_map_[dialog] = params;

  SheetContext* context = new SheetContext;
  context->type = type;
  context->owning_window = owning_window;

  if (type == SELECT_SAVEAS_FILE) {
    [dialog beginSheetForDirectory:default_dir
                              file:default_filename
                    modalForWindow:owning_window
                     modalDelegate:bridge_.get()
                    didEndSelector:@selector(endedPanel:withReturn:context:)
                       contextInfo:context];
  } else {
    NSOpenPanel* open_dialog = (NSOpenPanel*)dialog;

    if (type == SELECT_OPEN_MULTI_FILE)
      [open_dialog setAllowsMultipleSelection:YES];
    else
      [open_dialog setAllowsMultipleSelection:NO];

    if (type == SELECT_FOLDER) {
      [open_dialog setCanChooseFiles:NO];
      [open_dialog setCanChooseDirectories:YES];
    } else {
      [open_dialog setCanChooseFiles:YES];
      [open_dialog setCanChooseDirectories:NO];
    }

    [open_dialog beginSheetForDirectory:default_dir
                                   file:default_filename
                                  types:allowed_file_types
                         modalForWindow:owning_window
                          modalDelegate:bridge_.get()
                        didEndSelector:@selector(endedPanel:withReturn:context:)
                            contextInfo:context];
  }
}

void SelectFileDialogImpl::FileWasSelected(NSPanel* dialog,
                                           NSWindow* parent_window,
                                           bool was_cancelled,
                                           bool is_multi,
                                           const std::vector<FilePath>& files,
                                           int index) {
  void* params = params_map_[dialog];
  params_map_.erase(dialog);
  parents_.erase(parent_window);

  if (!listener_)
    return;

  if (was_cancelled) {
    listener_->FileSelectionCanceled(params);
  } else {
    if (is_multi) {
      listener_->MultiFilesSelected(files, params);
    } else {
      listener_->FileSelected(files[0], index, params);
    }
  }
}

NSView* SelectFileDialogImpl::GetAccessoryView(const FileTypeInfo* file_types,
                                               int file_type_index) {
  DCHECK(file_types);
  scoped_nsobject<NSNib> nib (
      [[NSNib alloc] initWithNibNamed:@"SaveAccessoryView" bundle:nil]);
  if (!nib)
    return nil;

  NSArray* objects;
  BOOL success = [nib instantiateNibWithOwner:nil
                              topLevelObjects:&objects];
  if (!success)
    return nil;
  [objects makeObjectsPerformSelector:@selector(release)];

  // This is a one-object nib, but IB insists on creating a second object, the
  // NSApplication. I don't know why.
  size_t view_index = 0;
  while (view_index < [objects count] &&
      ![[objects objectAtIndex:view_index] isKindOfClass:[NSView class]])
    ++view_index;
  DCHECK(view_index < [objects count]);
  NSView* accessory_view = [objects objectAtIndex:view_index];

  NSPopUpButton* popup = [accessory_view viewWithTag:kFileTypePopupTag];
  DCHECK(popup);

  size_t type_count = file_types->extensions.size();
  for (size_t type = 0; type<type_count; ++type) {
    NSString* type_description;
    if (type < file_types->extension_description_overrides.size()) {
      type_description = base::SysUTF16ToNSString(
          file_types->extension_description_overrides[type]);
    } else {
      const std::vector<FilePath::StringType>& ext_list =
          file_types->extensions[type];
      DCHECK(!ext_list.empty());
      NSString* type_extension = base::SysUTF8ToNSString(ext_list[0]);
      scoped_cftyperef<CFStringRef> uti(
          UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension,
                                                (CFStringRef)type_extension,
                                                NULL));
      scoped_cftyperef<CFStringRef> description(
          UTTypeCopyDescription(uti.get()));

      type_description =
          [NSString stringWithString:(NSString*)description.get()];
    }
    [popup addItemWithTitle:type_description];
  }

  [popup selectItemAtIndex:file_type_index-1];  // 1-based
  return accessory_view;
}

@implementation SelectFileDialogBridge

- (id)initWithSelectFileDialogImpl:(SelectFileDialogImpl*)s {
  self = [super init];
  if (self != nil) {
    selectFileDialogImpl_ = s;
  }
  return self;
}

- (void)endedPanel:(id)panel
        withReturn:(int)returnCode
           context:(void *)context {
  int index = 0;
  SelectFileDialogImpl::SheetContext* context_struct =
      (SelectFileDialogImpl::SheetContext*)context;

  SelectFileDialog::Type type = context_struct->type;
  NSWindow* parentWindow = context_struct->owning_window;
  delete context_struct;

  bool isMulti = type == SelectFileDialog::SELECT_OPEN_MULTI_FILE;

  std::vector<FilePath> paths;
  bool did_cancel = returnCode == NSCancelButton;
  if (!did_cancel) {
    if (type == SelectFileDialog::SELECT_SAVEAS_FILE) {
      paths.push_back(FilePath(base::SysNSStringToUTF8([panel filename])));

      NSView* accessoryView = [panel accessoryView];
      if (accessoryView) {
        NSPopUpButton* popup = [accessoryView viewWithTag:kFileTypePopupTag];
        if (popup) {
          // File type indexes are 1-based.
          index = [popup indexOfSelectedItem] + 1;
        }
      }
    } else {
      NSArray* filenames = [panel filenames];
      for (NSString* filename in filenames)
        paths.push_back(FilePath(base::SysNSStringToUTF8(filename)));
    }
  }

  selectFileDialogImpl_->FileWasSelected(panel,
                                         parentWindow,
                                         did_cancel,
                                         isMulti,
                                         paths,
                                         index);
}

@end
