// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_UNPACKER_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_UNPACKER_H_

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "base/tuple.h"

class DictionaryValue;
class SkBitmap;

// This class unpacks an extension.  It is designed to be used in a sandboxed
// child process.  We unpack and parse various bits of the extension, then
// report back to the browser process, who then transcodes the pre-parsed bits
// and writes them back out to disk for later use.
class ExtensionUnpacker {
 public:
  typedef std::vector< Tuple2<SkBitmap, FilePath> > DecodedImages;

  explicit ExtensionUnpacker(const FilePath& extension_path)
      : extension_path_(extension_path) {}

  // Install the extension file at |extension_path|.  Returns true on success.
  // Otherwise, error_message will contain a string explaining what went wrong.
  bool Run();

  // Write the decoded images to kDecodedImagesFilename.  We do this instead
  // of sending them over IPC, since they are so large.  Returns true on
  // success.
  bool DumpImagesToFile();

  // Read the decoded images back from the file we saved them to.
  // |extension_path| is the path to the extension we unpacked that wrote the
  // data. Returns true on success.
  static bool ReadImagesFromFile(const FilePath& extension_path,
                                 DecodedImages* images);

  const std::string& error_message() { return error_message_; }
  DictionaryValue* parsed_manifest() {
    return parsed_manifest_.get();
  }
  const DecodedImages& decoded_images() { return decoded_images_; }

 private:
  // Parse the manifest.json file inside the extension (not in the header).
  // Caller takes ownership of return value.
  DictionaryValue* ReadManifest();

  // Decodes the image at the given path and puts it in our list of decoded
  // images.
  bool AddDecodedImage(const FilePath& path);

  // Set the error message.
  void SetError(const std::string& error);

  // The extension to unpack.
  FilePath extension_path_;

  // The place we unpacked the extension to.
  FilePath temp_install_dir_;

  // The parsed version of the manifest JSON contained in the extension.
  scoped_ptr<DictionaryValue> parsed_manifest_;

  // A list of decoded images and the paths where those images came from.  Paths
  // are relative to the manifest file.
  DecodedImages decoded_images_;

  // The last error message that was set.  Empty if there were no errors.
  std::string error_message_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUnpacker);
};

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_UNPACKER_H_
