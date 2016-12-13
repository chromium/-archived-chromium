/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the declarations for the functions in the
// O3D converter namespace.  These functions do most of the actual
// work of conditioning and packagaging a JSON file for use in
// O3D.

#ifndef O3D_CONVERTER_CROSS_CONVERTER_H_
#define O3D_CONVERTER_CROSS_CONVERTER_H_

#include "base/file_path.h"
#include "core/cross/types.h"

namespace o3d {
namespace converter {

// Options struct for passing to the converter.
struct Options {
  Options()
      : base_path(FilePath::kCurrentDirectory),
        condition(true),
        up_axis(0, 0, 0),
        pretty_print(false),
        keep_filters(false),
        keep_materials(false) {
  }

  // The path to the "base" of the model path, from which all paths
  // are made relative.  Defaults to the current directory.
  FilePath base_path;

  // Indicates if the converter should condition the input shaders or
  // not.
  bool condition;

  // The up axis of the output. The input will be converted to a new coordinate
  // system if it indicates a different up axis. Zero means no conversion.
  Vector3 up_axis;

  // This indicates whether or not the serialized JSON code should be
  // pretty-printed (formatted with spaces and newlines) or just
  // emitted as one huge one-line string.  Defaults to false.
  bool pretty_print;

  // Tells the converter not to set all filters to tri-linear.
  bool keep_filters;

  // Tells the converter not to change materials to constant if they are used by
  // a mesh that has no normals.
  bool keep_materials;
};

// Converts the given file for use in O3D.  This is done by
// loading the input file, traversing the scene graph and serializing
// what is found there.
bool Convert(const FilePath& in_filename,
             const FilePath& out_filename,
             const Options& options,
             String* error_messages);

// Verifies the given shader file as "valid".  Returns true if input
// shader file contains valid code (code that can be compiled
// successfully).
bool Verify(const FilePath& in_filename,
            const FilePath& out_filename,
            const Options& options,
            String* error_messages);

}  // namespace converter
}  // namespace o3d

#endif  // O3D_CONVERTER_CROSS_CONVERTER_H_
