// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RGBColor_h
#define RGBColor_h

#include "config.h"
#include "CSSPrimitiveValue.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class RGBColor : public RefCounted<RGBColor> {
public:
    // TODO(ager): Make constructor private once codegenerator changes
    // have landed upstream.
    RGBColor(unsigned rgbcolor) : m_rgbcolor(rgbcolor) { }

    static PassRefPtr<RGBColor> create(unsigned rgbcolor);

    PassRefPtr<CSSPrimitiveValue> red();
    PassRefPtr<CSSPrimitiveValue> green();
    PassRefPtr<CSSPrimitiveValue> blue();

private:
    unsigned m_rgbcolor;
};

}  // namespace WebCore

#endif  // RGBColor_h
