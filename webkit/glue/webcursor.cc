// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webcursor.h"

#include "base/logging.h"
#include "base/pickle.h"
#include "webkit/api/public/WebCursorInfo.h"
#include "webkit/api/public/WebImage.h"

using WebKit::WebCursorInfo;
using WebKit::WebImage;

static const int kMaxCursorDimension = 1024;

WebCursor::WebCursor()
    : type_(WebCursorInfo::TypePointer) {
  InitPlatformData();
}

WebCursor::WebCursor(const WebCursorInfo& cursor_info)
    : type_(WebCursorInfo::TypePointer) {
  InitPlatformData();
  InitFromCursorInfo(cursor_info);
}

WebCursor::~WebCursor() {
  Clear();
}

WebCursor::WebCursor(const WebCursor& other) {
  InitPlatformData();
  Copy(other);
}

const WebCursor& WebCursor::operator=(const WebCursor& other) {
  if (this == &other)
    return *this;

  Clear();
  Copy(other);
  return *this;
}

void WebCursor::InitFromCursorInfo(const WebCursorInfo& cursor_info) {
  Clear();

#if defined(OS_WIN)
  if (cursor_info.externalHandle) {
    InitFromExternalCursor(cursor_info.externalHandle);
    return;
  }
#endif

  type_ = cursor_info.type;
  hotspot_ = cursor_info.hotSpot;
  if (IsCustom())
    SetCustomData(cursor_info.customImage);
}

void WebCursor::GetCursorInfo(WebCursorInfo* cursor_info) const {
  cursor_info->type = static_cast<WebCursorInfo::Type>(type_);
  cursor_info->hotSpot = hotspot_;
  ImageFromCustomData(&cursor_info->customImage);

#if defined(OS_WIN)
  cursor_info->externalHandle = external_cursor_;
#endif
}

bool WebCursor::Deserialize(const Pickle* pickle, void** iter) {
  int type, hotspot_x, hotspot_y, size_x, size_y, data_len;

  const char* data;

  // Leave |this| unmodified unless we are going to return success.
  if (!pickle->ReadInt(iter, &type) ||
      !pickle->ReadInt(iter, &hotspot_x) ||
      !pickle->ReadInt(iter, &hotspot_y) ||
      !pickle->ReadLength(iter, &size_x) ||
      !pickle->ReadLength(iter, &size_y) ||
      !pickle->ReadData(iter, &data, &data_len))
    return false;

  // Ensure the size is sane, and there is enough data.
  if (size_x > kMaxCursorDimension ||
      size_y > kMaxCursorDimension)
    return false;

  // The * 4 is because the expected format is an array of RGBA pixel values.
  if (size_x * size_y * 4 > data_len)
    return false;

  type_ = type;
  hotspot_.set_x(hotspot_x);
  hotspot_.set_y(hotspot_y);
  custom_size_.set_width(size_x);
  custom_size_.set_height(size_y);

  custom_data_.clear();
  if (data_len > 0) {
    custom_data_.resize(data_len);
    memcpy(&custom_data_[0], data, data_len);
  }

  return DeserializePlatformData(pickle, iter);
}

bool WebCursor::Serialize(Pickle* pickle) const {
  if (!pickle->WriteInt(type_) ||
      !pickle->WriteInt(hotspot_.x()) ||
      !pickle->WriteInt(hotspot_.y()) ||
      !pickle->WriteInt(custom_size_.width()) ||
      !pickle->WriteInt(custom_size_.height()))
    return false;

  const char* data = NULL;
  if (!custom_data_.empty())
    data = &custom_data_[0];
  if (!pickle->WriteData(data, custom_data_.size()))
    return false;

  return SerializePlatformData(pickle);
}

bool WebCursor::IsCustom() const {
  return type_ == WebCursorInfo::TypeCustom;
}

bool WebCursor::IsEqual(const WebCursor& other) const {
  if (type_ != other.type_)
    return false;

  if (!IsPlatformDataEqual(other))
    return false;

  return hotspot_ == other.hotspot_ &&
         custom_size_ == other.custom_size_ &&
         custom_data_ == other.custom_data_;
}

void WebCursor::Clear() {
  type_ = WebCursorInfo::TypePointer;
  hotspot_.set_x(0);
  hotspot_.set_y(0);
  custom_size_.set_width(0);
  custom_size_.set_height(0);
  custom_data_.clear();
  CleanupPlatformData();
}

void WebCursor::Copy(const WebCursor& other) {
  type_ = other.type_;
  hotspot_ = other.hotspot_;
  custom_size_ = other.custom_size_;
  custom_data_ = other.custom_data_;
  CopyPlatformData(other);
}

#if WEBKIT_USING_SKIA
// The WEBKIT_USING_CG implementation is in webcursor_mac.mm.
void WebCursor::SetCustomData(const WebImage& image) {
  if (image.isNull())
    return;

  // Fill custom_data_ directly with the NativeImage pixels.
  const SkBitmap& bitmap = image.getSkBitmap();
  SkAutoLockPixels bitmap_lock(bitmap);
  custom_data_.resize(bitmap.getSize());
  memcpy(&custom_data_[0], bitmap.getPixels(), bitmap.getSize());
  custom_size_.set_width(bitmap.width());
  custom_size_.set_height(bitmap.height());
}

void WebCursor::ImageFromCustomData(WebImage* image) const {
  if (custom_data_.empty())
    return;

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                   custom_size_.width(),
                   custom_size_.height());
  if (!bitmap.allocPixels())
    return;
  memcpy(bitmap.getPixels(), &custom_data_[0], custom_data_.size());

  image->assign(bitmap);
}
#endif
