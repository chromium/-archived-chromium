// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_unpacker.h"

#include "base/file_util.h"
#include "base/scoped_handle.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/values.h"
#include "net/base/file_stream.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/ipc_message_utils.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/zip.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/glue/image_decoder.h"

namespace {
// The name of a temporary directory to install an extension into for
// validation before finalizing install.
const char kTempExtensionName[] = "TEMP_INSTALL";

// The file to write our decoded images to, relative to the extension_path.
const char kDecodedImagesFilename[] = "DECODED_IMAGES";

// Errors
const char* kCouldNotCreateDirectoryError =
    "Could not create directory for unzipping.";
const char* kCouldNotDecodeImageError = "Could not decode theme image.";
const char* kCouldNotUnzipExtension = "Could not unzip extension.";
const char* kPathNamesMustBeAbsoluteOrLocalError =
    "Path names must not be absolute or contain '..'.";

}  // namespace

static SkBitmap DecodeImage(const FilePath& path) {
  // Read the file from disk.
  std::string file_contents;
  if (!file_util::PathExists(path) ||
      !file_util::ReadFileToString(path, &file_contents)) {
    return SkBitmap();
  }

  // Decode the image using WebKit's image decoder.
  const unsigned char* data =
      reinterpret_cast<const unsigned char*>(file_contents.data());
  webkit_glue::ImageDecoder decoder;
  return decoder.Decode(data, file_contents.length());
}

static bool PathContainsParentDirectory(const FilePath& path) {
  const FilePath::StringType kSeparators(FilePath::kSeparators);
  const FilePath::StringType kParentDirectory(FilePath::kParentDirectory);
  const size_t npos = FilePath::StringType::npos;
  const FilePath::StringType& value = path.value();

  for (size_t i = 0; i < value.length(); ) {
    i = value.find(kParentDirectory, i);
    if (i != npos) {
      if ((i == 0 || kSeparators.find(value[i-1]) == npos) &&
          (i+1 < value.length() || kSeparators.find(value[i+1]) == npos)) {
        return true;
      }
      ++i;
    }
  }

  return false;
}

DictionaryValue* ExtensionUnpacker::ReadManifest() {
  FilePath manifest_path =
      temp_install_dir_.AppendASCII(Extension::kManifestFilename);
  if (!file_util::PathExists(manifest_path)) {
    SetError(Extension::kInvalidManifestError);
    return NULL;
  }

  JSONFileValueSerializer serializer(manifest_path);
  std::string error;
  scoped_ptr<Value> root(serializer.Deserialize(&error));
  if (!root.get()) {
    SetError(error);
    return NULL;
  }

  if (!root->IsType(Value::TYPE_DICTIONARY)) {
    SetError(Extension::kInvalidManifestError);
    return NULL;
  }

  return static_cast<DictionaryValue*>(root.release());
}

bool ExtensionUnpacker::Run() {
  LOG(INFO) << "Installing extension " << extension_path_.value();

  // <profile>/Extensions/INSTALL_TEMP/<version>
  temp_install_dir_ =
      extension_path_.DirName().AppendASCII(kTempExtensionName);
  if (!file_util::CreateDirectory(temp_install_dir_)) {
    SetError(kCouldNotCreateDirectoryError);
    return false;
  }

  if (!Unzip(extension_path_, temp_install_dir_)) {
    SetError(kCouldNotUnzipExtension);
    return false;
  }

  // Parse the manifest.
  parsed_manifest_.reset(ReadManifest());
  if (!parsed_manifest_.get())
    return false;  // Error was already reported.
 
  // NOTE: Since the Unpacker doesn't have the extension's public_id, the
  // InitFromValue is allowed to generate a temporary id for the extension.
  // ANY CODE THAT FOLLOWS SHOULD NOT DEPEND ON THE CORRECT ID OF THIS
  // EXTENSION.
  Extension extension;
  std::string error;
  if (!extension.InitFromValue(*parsed_manifest_,
                               false,
                               &error)) {
    SetError(error);
    return false;
  }
  // Decode any images that the browser needs to display.
  std::set<FilePath> image_paths = extension.GetBrowserImages();
  for (std::set<FilePath>::iterator it = image_paths.begin();
       it != image_paths.end(); ++it) {
    if (!AddDecodedImage(*it))
      return false;  // Error was already reported.
  }

  return true;
}

bool ExtensionUnpacker::DumpImagesToFile() {
  IPC::Message pickle;  // We use a Message so we can use WriteParam.
  IPC::WriteParam(&pickle, decoded_images_);

  FilePath path = extension_path_.DirName().AppendASCII(kDecodedImagesFilename);
  if (!file_util::WriteFile(path, static_cast<const char*>(pickle.data()),
                            pickle.size())) {
    SetError("Could not write image data to disk.");
    return false;
  }

  return true;
}

// static
bool ExtensionUnpacker::ReadImagesFromFile(const FilePath& extension_path,
                                           DecodedImages* images) {
  FilePath path = extension_path.DirName().AppendASCII(kDecodedImagesFilename);
  std::string file_str;
  if (!file_util::ReadFileToString(path, &file_str))
    return false;

  IPC::Message pickle(file_str.data(), file_str.size());
  void* iter = NULL;
  return IPC::ReadParam(&pickle, &iter, images);
}

bool ExtensionUnpacker::AddDecodedImage(const FilePath& path) {
  // Make sure it's not referencing a file outside the extension's subdir.
  if (path.IsAbsolute() || PathContainsParentDirectory(path)) {
    SetError(kPathNamesMustBeAbsoluteOrLocalError);
    return false;
  }

  SkBitmap image_bitmap = DecodeImage(temp_install_dir_.Append(path));
  if (image_bitmap.isNull()) {
    SetError(kCouldNotDecodeImageError);
    return false;
  }

  decoded_images_.push_back(MakeTuple(image_bitmap, path));
  return true;
}

void ExtensionUnpacker::SetError(const std::string &error) {
  error_message_ = error;
}
