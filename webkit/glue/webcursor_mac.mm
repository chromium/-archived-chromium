// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "webkit/glue/webcursor.h"

#include "config.h"
#include "PlatformCursor.h"
#include "RetainPtr.h"

#undef LOG
#include "base/logging.h"

using WebCore::PlatformCursor;

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

NSCursor* CreateCustomCursor(const std::vector<char>& custom_data,
                             const gfx::Size& custom_size,
                             const gfx::Point& hotspot) {
  RetainPtr<CGColorSpace> cg_color(AdoptCF, CGColorSpaceCreateDeviceRGB());
  // this is safe since we're not going to draw into the context we're creating
  void* data = const_cast<void *>(static_cast<const void*>(&custom_data[0]));
  // settings here match SetCustomData() below; keep in sync
  RetainPtr<CGContextRef> context(AdoptCF, CGBitmapContextCreate(
                                             data,
                                             custom_size.width(),
                                             custom_size.height(),
                                             8,
                                             custom_size.width()*4,
                                             cg_color.get(),
                                             kCGImageAlphaPremultipliedLast |
                                             kCGBitmapByteOrder32Big));
  RetainPtr<CGImage> cg_image(AdoptCF,
                              CGBitmapContextCreateImage(context.get()));

  RetainPtr<NSBitmapImageRep> ns_bitmap(
      AdoptNS, [[NSBitmapImageRep alloc] initWithCGImage:cg_image.get()]);
  RetainPtr<NSImage> cursor_image([[NSImage alloc] init]);
  [cursor_image.get() addRepresentation:ns_bitmap.get()];
  DCHECK(cursor_image);
  return [[[NSCursor alloc] initWithImage:cursor_image.get()
                                  hotSpot:NSMakePoint(hotspot.x(),
                                                      hotspot.y())]
          autorelease];
}

}

// We're matching Safari's cursor choices; see platform/mac/CursorMac.mm
NSCursor* WebCursor::GetCursor() const {
  switch (Type_) {
    case PlatformCursor::TypePointer:
      return [NSCursor arrowCursor];
    case PlatformCursor::TypeCross:
      return LoadCursor("crossHairCursor", 11, 11);
    case PlatformCursor::TypeHand:
      return LoadCursor("linkCursor", 6, 1);
    case PlatformCursor::TypeIBeam:
      return [NSCursor IBeamCursor];
    case PlatformCursor::TypeWait:
      return LoadCursor("waitCursor", 7, 7);
    case PlatformCursor::TypeHelp:
      return LoadCursor("helpCursor", 8, 8);
    case PlatformCursor::TypeEastResize:
    case PlatformCursor::TypeEastPanning:
      return LoadCursor("eastResizeCursor", 14, 7);
    case PlatformCursor::TypeNorthResize:
    case PlatformCursor::TypeNorthPanning:
      return LoadCursor("northResizeCursor", 7, 1);
    case PlatformCursor::TypeNorthEastResize:
    case PlatformCursor::TypeNorthEastPanning:
      return LoadCursor("northEastResizeCursor", 14, 1);
    case PlatformCursor::TypeNorthWestResize:
    case PlatformCursor::TypeNorthWestPanning:
      return LoadCursor("northWestResizeCursor", 0, 0);
    case PlatformCursor::TypeSouthResize:
    case PlatformCursor::TypeSouthPanning:
      return LoadCursor("southResizeCursor", 7, 14);
    case PlatformCursor::TypeSouthEastResize:
    case PlatformCursor::TypeSouthEastPanning:
      return LoadCursor("southEastResizeCursor", 14, 14);
    case PlatformCursor::TypeSouthWestResize:
    case PlatformCursor::TypeSouthWestPanning:
      return LoadCursor("southWestResizeCursor", 1, 14);
    case PlatformCursor::TypeWestResize:
    case PlatformCursor::TypeWestPanning:
      return LoadCursor("westResizeCursor", 1, 7);
    case PlatformCursor::TypeNorthSouthResize:
      return LoadCursor("northSouthResizeCursor", 7, 7);
    case PlatformCursor::TypeEastWestResize:
      return LoadCursor("eastWestResizeCursor", 7, 7);
    case PlatformCursor::TypeNorthEastSouthWestResize:
      return LoadCursor("northEastSouthWestResizeCursor", 7, 7);
    case PlatformCursor::TypeNorthWestSouthEastResize:
      return LoadCursor("northWestSouthEastResizeCursor", 7, 7);
    case PlatformCursor::TypeColumnResize:
      return [NSCursor resizeLeftRightCursor];
    case PlatformCursor::TypeRowResize:
      return [NSCursor resizeUpDownCursor];
    case PlatformCursor::TypeMiddlePanning:
    case PlatformCursor::TypeMove:
      return LoadCursor("moveCursor", 7, 7);
    case PlatformCursor::TypeVerticalText:
      return LoadCursor("verticalTextCursor", 7, 7);
    case PlatformCursor::TypeCell:
      return LoadCursor("cellCursor", 7, 7);
    case PlatformCursor::TypeContextMenu:
      return LoadCursor("contextMenuCursor", 3, 2);
    case PlatformCursor::TypeAlias:
      return LoadCursor("aliasCursor", 11, 3);
    case PlatformCursor::TypeProgress:
      return LoadCursor("progressCursor", 3, 2);
    case PlatformCursor::TypeNoDrop:
      return LoadCursor("noDropCursor", 3, 1);
    case PlatformCursor::TypeCopy:
      return LoadCursor("copyCursor", 3, 2);
    case PlatformCursor::TypeNone:
      return LoadCursor("noneCursor", 7, 7);
    case PlatformCursor::TypeNotAllowed:
      return LoadCursor("notAllowedCursor", 11, 11);
    case PlatformCursor::TypeZoomIn:
      return LoadCursor("zoomInCursor", 7, 7);
    case PlatformCursor::TypeZoomOut:
      return LoadCursor("zoomOutCursor", 7, 7);
    case PlatformCursor::TypeCustom:
      return CreateCustomCursor(custom_data_, custom_size_, hotspot_);
  }
  NOTREACHED();
  return nil;
}

void WebCursor::SetCustomData(WebCore::Image* image) {
  WebCore::NativeImagePtr image_ptr = image->nativeImageForCurrentFrame();
  if (!image_ptr)
    return;
  
  RetainPtr<CGColorSpace> cg_color(AdoptCF, CGColorSpaceCreateDeviceRGB());
  
  size_t size = CGImageGetHeight(image_ptr)*CGImageGetWidth(image_ptr)*4;
  custom_data_.resize(size);
  custom_size_.set_width(CGImageGetWidth(image_ptr));
  custom_size_.set_height(CGImageGetHeight(image_ptr));
  
  // These settings match up with the code in CreateCustomCursor() above; keep
  // them in sync.
  // TODO(avi): test to ensure that the flags here are correct for RGBA
  RetainPtr<CGContextRef> context(AdoptCF, CGBitmapContextCreate(
                                               &custom_data_[0],
                                               CGImageGetWidth(image_ptr),
                                               CGImageGetHeight(image_ptr),
                                               8,
                                               CGImageGetWidth(image_ptr)*4,
                                               cg_color.get(),
                                               kCGImageAlphaPremultipliedLast |
                                               kCGBitmapByteOrder32Big));
  CGRect rect = CGRectMake(0, 0,
                           CGImageGetWidth(image_ptr),
                           CGImageGetHeight(image_ptr));
  CGContextDrawImage(context.get(), rect, image_ptr);
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
