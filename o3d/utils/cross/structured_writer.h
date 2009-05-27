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


// This file contains the declaration of class StructuredWriter.

#ifndef O3D_UTILS_CROSS_STRUCTURED_WRITER_H_
#define O3D_UTILS_CROSS_STRUCTURED_WRITER_H_

#include <string>

#include "base/basictypes.h"

namespace o3d {

// Interface of classes that can write structured data consisting of
// a hierarchy of objects (with named properties) and arrays of typed
// values.
class StructuredWriter {
 public:
  StructuredWriter() {}
  virtual ~StructuredWriter() {}

  // Opens an object block. An object block contains property / value
  // pairs.
  virtual void OpenObject() = 0;

  // Closes an object block.
  virtual void CloseObject() = 0;

  // Opens an array block. An array block contains zero or more values.
  virtual void OpenArray() = 0;

  // Closes an array block.
  virtual void CloseArray() = 0;

  // Hints to the writer that it should not add additional formatting
  // to make the output more readable. These can be nested.
  virtual void BeginCompacting() = 0;

  // Ends the hint that additional formatting is not necessary.
  virtual void EndCompacting() = 0;

  // Writes the name of a property. This should be immediately by a
  // value.
  virtual void WritePropertyName(const std::string& name) = 0;

  // Writes a boolean value.
  virtual void WriteBool(bool value) = 0;

  // Writes an integer value.
  virtual void WriteInt(int value) = 0;

  // Writes an unsigned integer value.
  virtual void WriteUnsignedInt(unsigned int value) = 0;

  // Writes a float value.
  virtual void WriteFloat(float value) = 0;

  // Writes a string value.
  virtual void WriteString(const std::string& value) = 0;

  // Writes a null value.
  virtual void WriteNull() = 0;

  // Flushes any unwritten content and closes the writer. No further
  // writes are allowed.
  virtual void Close() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(StructuredWriter);
};
}  // namespace o3d

#endif  // O3D_UTILS_CROSS_STRUCTURED_WRITER_H_
