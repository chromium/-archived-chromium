// Copyright 2008 Google Inc. All Rights Reserved.

#include "base/gfx/size.h"

#import <CoreGraphics/CoreGraphics.h>

#include <ostream>

namespace gfx {

CGSize Size::ToCGSize() const {
  CGSize s;
  s.width = width_;
  s.height = height_;
  return s;
}

std::ostream& operator<<(std::ostream& out, const gfx::Size& s) {
  return out << s.width() << "x" << s.height();
}

}  // namespace gfx
