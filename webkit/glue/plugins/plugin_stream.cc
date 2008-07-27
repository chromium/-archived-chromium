// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// TODO : Support NP_ASFILEONLY mode
// TODO : Support NP_SEEK mode
// TODO : Support SEEKABLE=true in NewStream

#include "webkit/glue/plugins/plugin_stream.h"

#include "base/string_util.h"
#include "base/message_loop.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/webkit_glue.h"
#include "googleurl/src/gurl.h"

namespace NPAPI {

PluginStream::PluginStream(
    PluginInstance *instance,
    const char *url,
    bool need_notify,
    void *notify_data)
    : instance_(instance),
      bytes_sent_(0),
      notify_needed_(need_notify),
      notify_data_(notify_data),
      close_on_write_data_(false),
      opened_(false),
      requested_plugin_mode_(NP_NORMAL),
      temp_file_handle_(INVALID_HANDLE_VALUE) {
  memset(&stream_, 0, sizeof(stream_));
  stream_.url = _strdup(url);
  temp_file_name_[0] = '\0';
}

PluginStream::~PluginStream() {
  // always cleanup our temporary files.
  CleanupTempFile();

  free(const_cast<char*>(stream_.url));
}

void PluginStream::UpdateUrl(const char* url) {
  DCHECK(!opened_);
  free(const_cast<char*>(stream_.url));
  stream_.url = _strdup(url);
}

bool PluginStream::Open(const std::string &mime_type,
                        const std::string &headers,
                        uint32 length,
                        uint32 last_modified) {
  headers_ = headers;
  NPP id = instance_->npp();
  stream_.end = length;
  stream_.lastmodified = last_modified;
  stream_.pdata = 0;
  stream_.ndata = id->ndata;
  stream_.notifyData = notify_data_;
  if (!headers_.empty())
    stream_.headers = headers_.c_str();

  const char *char_mime_type = "application/x-unknown-content-type";
  std::string temp_mime_type;
  if (!mime_type.empty()) {
      char_mime_type = mime_type.c_str();
  } else {
    GURL gurl(stream_.url);
    std::wstring path(UTF8ToWide(gurl.path()));
    if (webkit_glue::GetMimeTypeFromFile(path, &temp_mime_type))
      char_mime_type = temp_mime_type.c_str();
  }

  // Silverlight expects a valid mime type
  DCHECK(strlen(char_mime_type) != 0);
  NPError err = instance_->NPP_NewStream((NPMIMEType)char_mime_type,
                                         &stream_, false,
                                         &requested_plugin_mode_);
  if (err != NPERR_NO_ERROR)
    return false;

  opened_ = true;

  // If the plugin has requested certain modes, then we need a copy
  // of this file on disk.  Open it and save it as we go.
  if (requested_plugin_mode_ == NP_ASFILEONLY ||
    requested_plugin_mode_ == NP_ASFILE ||
    requested_plugin_mode_ == NP_SEEK) {
    if (OpenTempFile() == false)
      return false;
  }

  return true;
}

int PluginStream::Write(const char *buffer, const int length) {
  // There may be two streams to write to - the plugin and the file.
  // It is unclear what to do if we cannot write to both.  The rules of
  // this function are that the plugin must consume at least as many
  // bytes as returned by the WriteReady call.  So, we will attempt to
  // write that many to both streams.  If we can't write that many bytes
  // to each stream, we'll return failure.

  DCHECK(opened_);
  if (WriteToFile(buffer, length) && WriteToPlugin(buffer, length))
    return length;

  return -1;
}

bool PluginStream::WriteToFile(const char *buf, const int length) {
  // For ASFILEONLY, ASFILE, and SEEK modes, we need to write
  // to the disk
  if (temp_file_handle_ != INVALID_HANDLE_VALUE &&
      (requested_plugin_mode_ == NP_SEEK ||
       requested_plugin_mode_ == NP_ASFILE ||
       requested_plugin_mode_ == NP_ASFILEONLY) ) {
    int totalBytesWritten = 0;
    DWORD bytes;
    do {
      if (WriteFile(temp_file_handle_, buf, length, &bytes, 0) == FALSE)
        break;
      totalBytesWritten += bytes;
    } while (bytes > 0 && totalBytesWritten < length);

    if (totalBytesWritten != length)
      return false;
  }

  return true;
}

bool PluginStream::WriteToPlugin(const char *buf, const int length) {
  // For NORMAL and ASFILE modes, we send the data to the plugin now
  if (requested_plugin_mode_ != NP_NORMAL &&
      requested_plugin_mode_ != NP_ASFILE)
    return true;

  int written = TryWriteToPlugin(buf, length);
  if (written == -1)
      return false;

  if (written < length) {
    // Buffer the remaining data.
    size_t remaining = length - written;
    size_t previous_size = delivery_data_.size();
    delivery_data_.resize(previous_size + remaining);
    memcpy(&delivery_data_[previous_size], buf + written, remaining);
    MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &PluginStream::OnDelayDelivery));
  }

  return true;
}

void PluginStream::OnDelayDelivery() {
  // It is possible that the plugin stream may have closed before the task
  // was hit.
  if (!opened_) {
    return;
  }

  int size = static_cast<int>(delivery_data_.size());
  int written = TryWriteToPlugin(&delivery_data_.front(), size);
  if (written > 0) {
    // Remove the data that we already wrote.
    delivery_data_.erase(delivery_data_.begin(),
                         delivery_data_.begin() + written);
  }
}

int PluginStream::TryWriteToPlugin(const char *buf, const int length) {
  bool result = true;
  int byte_offset = 0;

  while (byte_offset < length) {
    int bytes_remaining = length - byte_offset;
    int bytes_to_write = instance_->NPP_WriteReady(&stream_);
    if (bytes_to_write > bytes_remaining)
      bytes_to_write = bytes_remaining;

    if (bytes_to_write == 0)
      return byte_offset;

    int bytesSent = instance_->NPP_Write(&stream_,
                                         bytes_sent_,
                                         bytes_to_write,
                                         const_cast<char*>(buf + byte_offset));
    if (bytesSent < bytes_to_write) {
      // We couldn't write all the bytes. This is an error.
      return -1;
    }
    bytes_sent_ += bytesSent;
    byte_offset += bytesSent;
  }

  if (close_on_write_data_)
    Close(NPRES_DONE);

  return length;
}

void PluginStream::WriteAsFile() {
  if (requested_plugin_mode_ == NP_ASFILE ||
      requested_plugin_mode_ == NP_ASFILEONLY)
    instance_->NPP_StreamAsFile(&stream_, temp_file_name_);
}

bool PluginStream::Close(NPReason reason) {
  if (opened_ == true) {
    opened_ = false;

    if (delivery_data_.size()) {
      if (reason == NPRES_DONE) {
        // There is more data to be streamed, don't destroy the stream now.
        close_on_write_data_ = true;
        return true;
      } else {
        // Stop any pending data from being streamed
        delivery_data_.resize(0);
      }
    }

    // If we have a temp file, be sure to close it.
    // Also, allow the plugin to access it now.
    if (temp_file_handle_ != INVALID_HANDLE_VALUE) {
      CloseTempFile();
      WriteAsFile();
    }

    if (stream_.ndata != NULL) {
      // Stream hasn't been closed yet.
      NPError err = instance_->NPP_DestroyStream(&stream_, reason);
      DCHECK(err == NPERR_NO_ERROR);
    }
  }

  Notify(reason);
  return true;
}

bool PluginStream::OpenTempFile() {
  DCHECK(temp_file_handle_ == INVALID_HANDLE_VALUE);

  // The reason for using all the Ascii versions of these filesystem
  // calls is that the filename which we pass back to the plugin
  // via NPAPI is an ascii filename.  Otherwise, we'd use wide-chars.
  //
  // TODO:
  // This is a bug in NPAPI itself, and it needs to be fixed.
  // The case which will fail is if a user has a multibyte name,
  // but has the system locale set to english.  GetTempPathA will
  // return junk in this case, causing us to be unable to open the
  // file.

  char temp_directory[MAX_PATH];
  if (GetTempPathA(MAX_PATH, temp_directory) == 0)
    return false;
  if (GetTempFileNameA(temp_directory, "npstream", 0, temp_file_name_) == 0)
    return false;
  temp_file_handle_ = CreateFileA(temp_file_name_,
                                  FILE_ALL_ACCESS,
                                  FILE_SHARE_READ,
                                  0,
                                  CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL,
                                  0);
  if (temp_file_handle_ == INVALID_HANDLE_VALUE) {
    temp_file_name_[0] = '\0';
    return false;
  }
  return true;
}

void PluginStream::CloseTempFile() {
  if (temp_file_handle_ != INVALID_HANDLE_VALUE) {
    CloseHandle(temp_file_handle_);
    temp_file_handle_ = INVALID_HANDLE_VALUE;
  }
}

void PluginStream::CleanupTempFile() {
  CloseTempFile();
  if (temp_file_name_[0] != '\0') {
    DeleteFileA(temp_file_name_);
    temp_file_name_[0] = '\0';
  }
}

void PluginStream::Notify(NPReason reason) {
  if (notify_needed_) {
    instance_->NPP_URLNotify(stream_.url, reason, notify_data_);
    notify_needed_ = false;
  }
}

}  // namespace NPAPI
