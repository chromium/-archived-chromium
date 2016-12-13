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


// useful utility functions for unit/functional testing

#include "tests/common/cross/test_utils.h"
#include <sys/stat.h>
#include "core/cross/client.h"
#include "import/cross/targz_processor.h"
#include "tests/common/win/testing_common.h"

namespace test_utils {

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
uint8 *ReadFile(o3d::String file, size_t *data_size) {
  FILE *fp = fopen(file.c_str(), "rb");

  if (fp == NULL) return NULL;

  // Figure out the file length
  struct stat file_info;
  stat(file.c_str(), &file_info);
  size_t file_size = file_info.st_size;

  // Read it all into memory
  // TODO: this should be new[] instead of malloc.
  uint8 *data = static_cast<uint8*>(malloc(file_size));
  DCHECK(data);

  int bytes_read = fread(data, 1, file_size, fp);
  fclose(fp);
  DCHECK_EQ(bytes_read, file_size);

  // Return file size and data
  if (data_size) *data_size = file_size;
  return data;
}

}  // namespace test_utils
