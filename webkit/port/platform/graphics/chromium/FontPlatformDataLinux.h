// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FontPlatformDataLinux_h
#define FontPlatformDataLinux_h

#include "config.h"
#include "build/build_config.h"

#include "StringImpl.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class FontDescription;

// TODO(agl): stubs only

class FontPlatformData {
public:
    FontPlatformData();
    FontPlatformData(WTF::HashTableDeletedValueType);
    FontPlatformData(const FontDescription&, const AtomicString& family);
    FontPlatformData(float size, bool bold, bool oblique);
    ~FontPlatformData();

    bool isHashTableDeletedValue() const;
    unsigned hash() const;
    bool operator==(const FontPlatformData& other) const;
    float size() const;
};

}  // namespace WebCore

#endif  // ifdef FontPlatformData_h
