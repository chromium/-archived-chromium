// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "webkit/glue/webcursor.h"

#import <AppKit/AppKit.h>

#include "base/logging.h"
#include "base/scoped_cftyperef.h"
#include "webkit/api/public/WebCursorInfo.h"
#include "webkit/api/public/WebImage.h"
#include "webkit/api/public/WebSize.h"

using WebKit::WebCursorInfo;
using WebKit::WebImage;
using WebKit::WebSize;

namespace {

// TODO(avi): Is loading resources what we want to do here?
NSCursor* LoadCursor(const char* name, int x, int y) {
  NSString* file_name = [NSString stringWithUTF8String:name];
  DCHECK(file_name);
  NSImage* cursor_image = [NSImage imageNamed:file_name];
  DCHECK(cursor_image);
  return [[[NSCursor alloc] initWithImage:cursor_image
                                  hotSpot:NSMakePoint(x, y)] autorelease];
}

CGImageRef CreateCGImageFromCustomData(const std::vector<char>& custom_data,
                                       const gfx::Size& custom_size) {
  scoped_cftyperef<CGColorSpaceRef> cg_color(CGColorSpaceCreateDeviceRGB());
  // this is safe since we're not going to draw into the context we're creating
  void* data = const_cast<char*>(&custom_data[0]);
  // settings here match SetCustomData() below; keep in sync
  scoped_cftyperef<CGContextRef> context(
      CGBitmapContextCreate(data,
                            custom_size.width(),
                            custom_size.height(),
                            8,
                            custom_size.width()*4,
                            cg_color.get(),
                            kCGImageAlphaPremultipliedLast |
                            kCGBitmapByteOrder32Big));
  return CGBitmapContextCreateImage(context.get());
}

NSCursor* CreateCustomCursor(const std::vector<char>& custom_data,
                             const gfx::Size& custom_size,
                             const gfx::Point& hotspot) {
  scoped_cftyperef<CGImageRef> cg_image(
      CreateCGImageFromCustomData(custom_data, custom_size));

  NSBitmapImageRep* ns_bitmap =
      [[NSBitmapImageRep alloc] initWithCGImage:cg_image.get()];
  NSImage* cursor_image = [[NSImage alloc] init];
  DCHECK(cursor_image);
  [cursor_image addRepresentation:ns_bitmap];
  [ns_bitmap release];

  NSCursor* cursor = [[NSCursor alloc] initWithImage:cursor_image
                                             hotSpot:NSMakePoint(hotspot.x(),
                                                                 hotspot.y())];
  [cursor_image release];

  return [cursor autorelease];
}

}  // namespace

// We're matching Safari's cursor choices; see platform/mac/CursorMac.mm
NSCursor* WebCursor::GetCursor() const {
  switch (type_) {
    case WebCursorInfo::TypePointer:
      return [NSCursor arrowCursor];
    case WebCursorInfo::TypeCross:
      return LoadCursor("crossHairCursor", 11, 11);
    case WebCursorInfo::TypeHand:
      return LoadCursor("linkCursor", 6, 1);
    case WebCursorInfo::TypeIBeam:
      return [NSCursor IBeamCursor];
    case WebCursorInfo::TypeWait:
      return LoadCursor("waitCursor", 7, 7);
    case WebCursorInfo::TypeHelp:
      return LoadCursor("helpCursor", 8, 8);
    case WebCursorInfo::TypeEastResize:
    case WebCursorInfo::TypeEastPanning:
      return LoadCursor("eastResizeCursor", 14, 7);
    case WebCursorInfo::TypeNorthResize:
    case WebCursorInfo::TypeNorthPanning:
      return LoadCursor("northResizeCursor", 7, 1);
    case WebCursorInfo::TypeNorthEastResize:
    case WebCursorInfo::TypeNorthEastPanning:
      return LoadCursor("northEastResizeCursor", 14, 1);
    case WebCursorInfo::TypeNorthWestResize:
    case WebCursorInfo::TypeNorthWestPanning:
      return LoadCursor("northWestResizeCursor", 0, 0);
    case WebCursorInfo::TypeSouthResize:
    case WebCursorInfo::TypeSouthPanning:
      return LoadCursor("southResizeCursor", 7, 14);
    case WebCursorInfo::TypeSouthEastResize:
    case WebCursorInfo::TypeSouthEastPanning:
      return LoadCursor("southEastResizeCursor", 14, 14);
    case WebCursorInfo::TypeSouthWestResize:
    case WebCursorInfo::TypeSouthWestPanning:
      return LoadCursor("southWestResizeCursor", 1, 14);
    case WebCursorInfo::TypeWestResize:
    case WebCursorInfo::TypeWestPanning:
      return LoadCursor("westResizeCursor", 1, 7);
    case WebCursorInfo::TypeNorthSouthResize:
      return LoadCursor("northSouthResizeCursor", 7, 7);
    case WebCursorInfo::TypeEastWestResize:
      return LoadCursor("eastWestResizeCursor", 7, 7);
    case WebCursorInfo::TypeNorthEastSouthWestResize:
      return LoadCursor("northEastSouthWestResizeCursor", 7, 7);
    case WebCursorInfo::TypeNorthWestSouthEastResize:
      return LoadCursor("northWestSouthEastResizeCursor", 7, 7);
    case WebCursorInfo::TypeColumnResize:
      return [NSCursor resizeLeftRightCursor];
    case WebCursorInfo::TypeRowResize:
      return [NSCursor resizeUpDownCursor];
    case WebCursorInfo::TypeMiddlePanning:
    case WebCursorInfo::TypeMove:
      return LoadCursor("moveCursor", 7, 7);
    case WebCursorInfo::TypeVerticalText:
      return LoadCursor("verticalTextCursor", 7, 7);
    case WebCursorInfo::TypeCell:
      return LoadCursor("cellCursor", 7, 7);
    case WebCursorInfo::TypeContextMenu:
      return LoadCursor("contextMenuCursor", 3, 2);
    case WebCursorInfo::TypeAlias:
      return LoadCursor("aliasCursor", 11, 3);
    case WebCursorInfo::TypeProgress:
      return LoadCursor("progressCursor", 3, 2);
    case WebCursorInfo::TypeNoDrop:
      return LoadCursor("noDropCursor", 3, 1);
    case WebCursorInfo::TypeCopy:
      return LoadCursor("copyCursor", 3, 2);
    case WebCursorInfo::TypeNone:
      return LoadCursor("noneCursor", 7, 7);
    case WebCursorInfo::TypeNotAllowed:
      return LoadCursor("notAllowedCursor", 11, 11);
    case WebCursorInfo::TypeZoomIn:
      return LoadCursor("zoomInCursor", 7, 7);
    case WebCursorInfo::TypeZoomOut:
      return LoadCursor("zoomOutCursor", 7, 7);
    case WebCursorInfo::TypeCustom:
      return CreateCustomCursor(custom_data_, custom_size_, hotspot_);
  }
  NOTREACHED();
  return nil;
}

void WebCursor::SetCustomData(const WebImage& image) {
  if (image.isNull())
    return;

  scoped_cftyperef<CGColorSpaceRef> cg_color(
      CGColorSpaceCreateDeviceRGB());

  const WebSize& image_dimensions = image.size();
  int image_width = image_dimensions.width;
  int image_height = image_dimensions.height;

  size_t size = image_height * image_width * 4;
  custom_data_.resize(size);
  custom_size_.set_width(image_width);
  custom_size_.set_height(image_height);

  // These settings match up with the code in CreateCustomCursor() above; keep
  // them in sync.
  // TODO(avi): test to ensure that the flags here are correct for RGBA
  scoped_cftyperef<CGContextRef> context(
      CGBitmapContextCreate(&custom_data_[0],
                            image_width,
                            image_height,
                            8,
                            image_width * 4,
                            cg_color.get(),
                            kCGImageAlphaPremultipliedLast |
                            kCGBitmapByteOrder32Big));
  CGRect rect = CGRectMake(0, 0, image_width, image_height);
  CGContextDrawImage(context.get(), rect, image.getCGImageRef());
}

void WebCursor::ImageFromCustomData(WebImage* image) const {
  if (custom_data_.empty())
    return;

  scoped_cftyperef<CGImageRef> cg_image(
      CreateCGImageFromCustomData(custom_data_, custom_size_));
  *image = cg_image.get();
}

void WebCursor::InitPlatformData() {
  return;
}

bool WebCursor::SerializePlatformData(Pickle* pickle) const {
  return true;
}

bool WebCursor::DeserializePlatformData(const Pickle* pickle, void** iter) {
  return true;
}

bool WebCursor::IsPlatformDataEqual(const WebCursor& other) const {
  return true;
}

void WebCursor::CleanupPlatformData() {
  return;
}

void WebCursor::CopyPlatformData(const WebCursor& other) {
  return;
}
