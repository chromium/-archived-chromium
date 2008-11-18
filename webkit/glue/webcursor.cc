// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webcursor.h"

#include "config.h"
#include "NativeImageSkia.h"
#include "PlatformCursor.h"

#undef LOG
#include "base/logging.h"
#include "base/pickle.h"

WebCursor::WebCursor()
    : type_(WebCore::PlatformCursor::typePointer) {
}

WebCursor::WebCursor(const WebCore::PlatformCursor& platform_cursor)
    : type_(platform_cursor.type()),
      hotspot_(platform_cursor.hotSpot().x(), platform_cursor.hotSpot().y()) {
  if (IsCustom())
    SetCustomData(platform_cursor.customImage().get());
}

bool WebCursor::Deserialize(const Pickle* pickle, void** iter) {
  int type, hotspot_x, hotspot_y, size_x, size_y, data_len;
  const char* data;

  // Leave |this| unmodified unless we are going to return success.
  if (!pickle->ReadInt(iter, &type) ||
      !pickle->ReadInt(iter, &hotspot_x) ||
      !pickle->ReadInt(iter, &hotspot_y) ||
      !pickle->ReadInt(iter, &size_x) ||
      !pickle->ReadInt(iter, &size_y) ||
      !pickle->ReadData(iter, &data, &data_len))
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

  return true;
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
  return pickle->WriteData(data, custom_data_.size());
}

bool WebCursor::IsCustom() const {
  return type_ == WebCore::PlatformCursor::typeCustom;
}

bool WebCursor::IsEqual(const WebCursor& other) const {
  if (!IsCustom())
    return type_ == other.type_;

  return hotspot_ == other.hotspot_ &&
         custom_size_ == other.custom_size_ &&
         custom_data_ == other.custom_data_;
}

#if !defined(OS_MACOSX)
// The Mac version of Chromium is built with PLATFORM(CG) while all other
// versions are PLATFORM(SKIA). We'll keep this Skia implementation here for
// common use and put the Mac implementation in webcursor_mac.mm.
void WebCursor::SetCustomData(WebCore::Image* image) {
  WebCore::NativeImagePtr image_ptr = image->nativeImageForCurrentFrame();
  if (!image_ptr)
    return;

  // Fill custom_data_ directly with the NativeImage pixels.
  SkAutoLockPixels bitmap_lock(*image_ptr);
  custom_data_.resize(image_ptr->getSize());
  memcpy(&custom_data_[0], image_ptr->getPixels(), image_ptr->getSize());
  custom_size_.set_width(image_ptr->width());
  custom_size_.set_height(image_ptr->height());
}
#endif
