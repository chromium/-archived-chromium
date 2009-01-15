// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "webkit/glue/plugins/plugin_stream.h"

#include <string.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "webkit/glue/plugins/plugin_instance.h"

namespace NPAPI {

PluginStream::PluginStream(
    PluginInstance *instance,
    const char *url,
    bool need_notify,
    void *notify_data)
    : instance_(instance),
      notify_needed_(need_notify),
      notify_data_(notify_data),
      close_on_write_data_(false),
      requested_plugin_mode_(NP_NORMAL),
      opened_(false),
      temp_file_(NULL),
      temp_file_path_(),
      data_offset_(0),
      seekable_stream_(false) {
  memset(&stream_, 0, sizeof(stream_));
  stream_.url = strdup(url);
}

void PluginStream::UpdateUrl(const char* url) {
  DCHECK(!opened_);
  free(const_cast<char*>(stream_.url));
  stream_.url = strdup(url);
}

void PluginStream::WriteAsFile() {
  if (requested_plugin_mode_ == NP_ASFILE ||
      requested_plugin_mode_ == NP_ASFILEONLY)
    instance_->NPP_StreamAsFile(&stream_, temp_file_path_.value().c_str());
}

size_t PluginStream::WriteBytes(const char *buf, size_t length) {
  return fwrite(buf, sizeof(char), length, temp_file_);
}

bool PluginStream::OpenTempFile() {
  DCHECK(temp_file_ == NULL);

  if (file_util::CreateTemporaryFileName(&temp_file_path_))
    temp_file_ = file_util::OpenFile(temp_file_path_, "a");

  if (!temp_file_) {
    temp_file_path_ = FilePath("");
    return false;
  }

  return true;
}

void PluginStream::CloseTempFile() {
  file_util::CloseFile(temp_file_);
  temp_file_ = NULL;
}

bool PluginStream::TempFileIsValid() {
  return temp_file_ != NULL;
}

}  // namespace NPAPI

