// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/file_descriptor_posix.h"

#include "base/logging.h"

DescriptorSet::DescriptorSet()
    : next_descriptor_(0) {
}

DescriptorSet::~DescriptorSet() {
  if (next_descriptor_ == descriptors_.size())
    return;

  LOG(WARNING) << "DescriptorSet destroyed with unconsumed descriptors";
  // We close all the descriptors where the close flag is set. If this
  // message should have been transmitted, then closing those with close
  // flags set mirrors the expected behaviour.
  //
  // If this message was received with more descriptors than expected
  // (which could a DOS against the browser by a rouge renderer) then all
  // the descriptors have their close flag set and we free all the extra
  // kernel resources.
  for (unsigned i = next_descriptor_; i < descriptors_.size(); ++i) {
    if (descriptors_[i].auto_close)
      close(descriptors_[i].fd);
  }
}

void DescriptorSet::Add(int fd) {
  struct FileDescriptor sd;
  sd.fd = fd;
  sd.auto_close = false;
  descriptors_.push_back(sd);
  DCHECK(descriptors_.size() <= MAX_DESCRIPTORS_PER_MESSAGE);
}

void DescriptorSet::AddAndAutoClose(int fd) {
  struct FileDescriptor sd;
  sd.fd = fd;
  sd.auto_close = true;
  descriptors_.push_back(sd);
  DCHECK(descriptors_.size() <= MAX_DESCRIPTORS_PER_MESSAGE);
}

int DescriptorSet::NextDescriptor() {
  if (next_descriptor_ == descriptors_.size())
    return -1;

  return descriptors_[next_descriptor_++].fd;
}

void DescriptorSet::GetDescriptors(int* buffer) const {
  for (std::vector<FileDescriptor>::const_iterator
       i = descriptors_.begin(); i != descriptors_.end(); ++i) {
    *(buffer++) = i->fd;
  }
}

void DescriptorSet::CommitAll() {
  for (std::vector<FileDescriptor>::iterator
       i = descriptors_.begin(); i != descriptors_.end(); ++i) {
    if (i->auto_close)
      close(i->fd);
  }
  descriptors_.clear();
}

void DescriptorSet::SetDescriptors(const int* buffer, unsigned count) {
  DCHECK(count <= MAX_DESCRIPTORS_PER_MESSAGE);
  DCHECK(descriptors_.size() == 0);

  descriptors_.reserve(count);
  for (unsigned i = 0; i < count; ++i) {
    struct FileDescriptor sd;
    sd.fd = buffer[i];
    sd.auto_close = true;
    descriptors_.push_back(sd);
  }
}
