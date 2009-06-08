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


// This file contains implementation for raw-data which may be used
// by the progressive streaming archive system

#include "import/cross/raw_data.h"
#include "base/file_util.h"

#ifdef OS_MACOSX
#include <CoreFoundation/CoreFoundation.h>
#endif

#ifdef OS_WIN
#include <rpc.h>
#endif

namespace o3d {

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// RawData class
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

O3D_DEFN_CLASS(RawData, ParamObject);

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
RawData::RawData(ServiceLocator* service_locator,
                 const String &uri,
                 const void *data,
                 size_t length)
    : ParamObject(service_locator), uri_(uri) {
  // make private copy of data
  data_.reset(new uint8[length]);
  length_ = length;
  memcpy(data_.get(), data, length);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
RawData::Ref RawData::Create(ServiceLocator* service_locator,
                             const String &uri,
                             const void *data,
                             size_t length) {
  return RawData::Ref(new RawData(service_locator, uri, data, length));
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
RawData::~RawData() {
  Discard();
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
const uint8 *RawData::GetData() const {
  // Return data immediately if we have it
  if (data_.get()) {
    return data_.get();
  }

  // We need to load the data from the cache file
  if (temp_filepath_.empty()) {
    DLOG(ERROR) << "cannot retrieve data object - it has been released";
    return NULL;
  }

  FILE *tempfile = file_util::OpenFile(temp_filepath_, "rb");
  if (!tempfile) {
    DLOG(ERROR) << "cached data file cannot be opened";
    return NULL;
  }

  data_.reset(new uint8[length_]);
  size_t bytes_read = fread(data_.get(), 1, length_, tempfile);

  if (bytes_read != length_) {
    DLOG(ERROR) << "error reading cached data file";
    data_.reset();
  }

  file_util::CloseFile(tempfile);

  return data_.get();
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

namespace {

// Simple UTF8 validation.
// Params:
//   data: RawData to validate.
//   utf8_length: pointer to size_t to receieve length of UTF8 data.
// Returns:
//   the start of the UTF-8 string or NULL if not valid UTF-8.
const char* GetValidUTF8(const RawData& data, size_t* utf8_length) {
  DCHECK(utf8_length);
  const uint8* s = data.GetDataAs<const uint8>(0);
  if (!s) {
    return NULL;
  }
  size_t length = data.GetLength();

  // Check for BOM and skip it.
  if (length >= 3 && s[0] == 0xEF && s[1] == 0xBB && s[2] == 0xBF) {
    length -= 3;
    s += 3;
  }

  const uint8* start = s;
  *utf8_length = length;

  while (length) {
    uint8 c = *s++;
    if (c >= 0x80) {
      // It's a multi-byte character
      if (c >= 0xC2 && c <= 0xF4) {
        uint32 codepoint;
        size_t remaining_code_length = 0;
        if ((c & 0xE0) == 0xC0) {
          codepoint = c & 0x1F;
          remaining_code_length = 1;
        } else if ((c & 0xF0) == 0xE0) {
          codepoint = c & 0x0F;
          remaining_code_length = 2;
        } else if ((c & 0xF8) == 0xF0) {
          codepoint = c & 0x07;
          remaining_code_length = 3;
        }
        if (remaining_code_length == 0 || remaining_code_length > length) {
          // Not valid UTF-8
          return NULL;
        }
        length -= remaining_code_length;
        for (size_t cc = 0; cc < remaining_code_length; ++cc) {
          c = *s++;
          if ((c & 0xC0) != 0x80) {
            // Not valid UTF-8
            return NULL;
          }
          codepoint = (codepoint << 6) | (c & 0x3F);
        }
        if (codepoint >= 0xD800 && codepoint < 0xDFFF) {
          // Not valid UTF-8
          return NULL;
        }
      } else {
        // Not valid UTF.
        return NULL;
      }
    } else if (c == 0x00) {
      // It's NULL, not UTF-8
      return NULL;
    }
    --length;
  }
  return reinterpret_cast<const char*>(start);
};

}  // anonymous namespace

String RawData::StringValue() const {
  // NOTE: Originally it was thought to only allow certain extensions.
  // Unfortunately it's not clear what list of extensions are valid. The list of
  // extensions that might be useful to an application is nearly infinite (.txt,
  // .json, .xml, .ini, .csv, .php, .js, .html, .css .xsl, .dae, etc.) So,
  // instead we validate the string is valid UTF-8 AND that there are no NULLs
  // in the string.
  size_t length;
  const char* utf8 = GetValidUTF8(*this, &length);
  if (!utf8) {
    O3D_ERROR(service_locator()) << "RawData is not valid UTF-8 string";
  } else {
    return String (utf8, length);
  }
  return String();
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void RawData::Flush() {
  // Only create the temp file if it doesn't already exist
  if (data_.get() && temp_filepath_.empty()) {
    if (GetTempFilePathFromURI(uri_, &temp_filepath_)) {
      FILE *tempfile = file_util::OpenFile(temp_filepath_, "wb");

      if (tempfile) {
        fwrite(data_.get(), 1, GetLength(), tempfile);
        file_util::CloseFile(tempfile);

        // Now that the data is cached, free it
        data_.reset();
      } else {
        DLOG(ERROR) << "error creating cached data file";
        temp_filepath_ = FilePath();
      }
    }
  }
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
const FilePath& RawData::GetTempFilePath() {
  Flush();  // writes temp file if it's not already written
  return temp_filepath_;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void RawData::Discard() {
  data_.reset();
  DeleteTempFile();
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool RawData::IsOffsetLengthValid(size_t offset, size_t length) const {
  if (offset + length < offset) {
    O3D_ERROR(service_locator()) << "overflow";
    return false;
  }
  if (offset + length > length_) {
    O3D_ERROR(service_locator()) << "illegal data offset or size";
    return false;
  }
  return true;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void RawData::DeleteTempFile() {
  if (!temp_filepath_.empty()) {
    file_util::Delete(temp_filepath_, false);
    temp_filepath_ = FilePath();
  }
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static String GetUUIDString() {
#ifdef OS_WIN
  // now generate a GUID
  UUID guid = {0};
  UuidCreate(&guid);

  // and format into a wide-string
  char guid_string[37];
#if defined(OS_WIN)
#define snprintf _snprintf
#endif
  snprintf(
      guid_string, sizeof(guid_string) / sizeof(guid_string[0]),
      "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      guid.Data1, guid.Data2, guid.Data3,
      guid.Data4[0], guid.Data4[1], guid.Data4[2],
      guid.Data4[3], guid.Data4[4], guid.Data4[5],
      guid.Data4[6], guid.Data4[7]);

  return guid_string;
#endif

#ifdef OS_MACOSX
  CFUUIDRef uuid = CFUUIDCreate(NULL);
  CFStringRef uuid_string_ref = CFUUIDCreateString(NULL, uuid);
  CFRelease(uuid);

  char uuid_string[64];
  uuid_string[0] = 0;  // null-terminate, in case CFStringGetCString() fails
  CFStringGetCString(uuid_string_ref,
                     uuid_string,
                     sizeof(uuid_string),
                     kCFStringEncodingUTF8);
  CFRelease(uuid_string_ref);


  return uuid_string;
#endif

#ifdef OS_LINUX
  static unsigned int index = 0;
  char uuid[18] = {0};
  unsigned int pid = getpid();
  snprintf(uuid, 18, "%08x-%08x", pid, index++);
  return String(uuid);
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool RawData::GetTempFilePathFromURI(const String &uri,
                                     FilePath *temp_fullpath) {
  if (!temp_fullpath) return false;

  // We use a UUID here to avoid any possible collisions with other tempfiles
  // which have been or will be written sharing the same basic name

  FilePath temp_path;
  if (!file_util::GetTempDir(&temp_path)) {
    return false;
  }

  String uuid_string = GetUUIDString();

  // format the temp file basename
  String filename;

  // try to retain the original file suffix (.jpg, etc.)
  int dot_position = uri.rfind('.');
  if (dot_position != std::string::npos) {
    filename = uuid_string + uri.substr(dot_position);
  } else {
    filename = uuid_string;
  }

  // Construct the full pathname
  FilePath fullpath = temp_path;
  fullpath = fullpath.AppendASCII(filename);

  if (temp_fullpath) *temp_fullpath = fullpath;

  return true;
}

}  // namespace o3d
