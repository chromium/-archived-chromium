// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "FontPlatformData.h"

#include "NotImplemented.h"

namespace WebCore {

// TODO(agl): stubs only

FontPlatformData::FontPlatformData() {
    notImplemented();
}

FontPlatformData::FontPlatformData(WTF::HashTableDeletedValueType) {
    notImplemented();
}

FontPlatformData::FontPlatformData(const FontDescription& fontDescription,
                                   const AtomicString& familyName) {
    notImplemented();
}

FontPlatformData::FontPlatformData(float size, bool bold, bool oblique) {
    notImplemented();
}

FontPlatformData::~FontPlatformData() {
    notImplemented();
}

bool FontPlatformData::isHashTableDeletedValue() const {
    notImplemented();
    return false;
}

unsigned FontPlatformData::hash() const {
    notImplemented();
    return 0;
}

bool FontPlatformData::operator==(const FontPlatformData &other) const {
    notImplemented();
    return false;
}

float FontPlatformData::size() const {
    notImplemented();
    return 0;
}

}  // namespace WebCore
