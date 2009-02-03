// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/renderer/media/data_source_impl.h"

DataSourceImpl::DataSourceImpl(WebMediaPlayerDelegateImpl* delegate)
    : delegate_(delegate) {
}

DataSourceImpl::~DataSourceImpl() {
}

void DataSourceImpl::Stop() {
  // TODO(scherkus): implement Stop.
  NOTIMPLEMENTED();
}

bool DataSourceImpl::Initialize(const std::string& uri) {
  // TODO(scherkus): implement Initialize.
  NOTIMPLEMENTED();
  return false;
}

const media::MediaFormat* DataSourceImpl::GetMediaFormat() {
  // TODO(scherkus): implement GetMediaFormat.
  NOTIMPLEMENTED();
  return NULL;
}

size_t DataSourceImpl::Read(char* data, size_t size) {
  // TODO(scherkus): implement Read.
  NOTIMPLEMENTED();
  return media::DataSource::kReadError;
}

bool DataSourceImpl::GetPosition(int64* position_out) {
  // TODO(scherkus): implement GetPosition.
  NOTIMPLEMENTED();
  return false;
}

bool DataSourceImpl::SetPosition(int64 position) {
  // TODO(scherkus): implement SetPosition.
  NOTIMPLEMENTED();
  return false;
}

bool DataSourceImpl::GetSize(int64* size_out) {
  // TODO(scherkus): implement GetSize.
  NOTIMPLEMENTED();
  return false;
}

bool DataSourceImpl::IsMediaFormatSupported(const media::MediaFormat* format) {
  // TODO(hclam: implement this.
  NOTIMPLEMENTED();
  return true;
}
