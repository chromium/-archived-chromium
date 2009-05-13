// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/icon_util.h"

#include "app/win_util.h"
#include "base/file_util.h"
#include "base/gfx/size.h"
#include "base/logging.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"

// Defining the dimensions for the icon images. We store only one value because
// we always resize to a square image; that is, the value 48 means that we are
// going to resize the given bitmap to a 48 by 48 pixels bitmap.
//
// The icon images appear in the icon file in same order in which their
// corresponding dimensions appear in the |icon_dimensions_| array, so it is
// important to keep this array sorted. Also note that the maximum icon image
// size we can handle is 255 by 255.
const int IconUtil::icon_dimensions_[] = {
  8,    // Recommended by the MSDN as a nice to have icon size.
  10,   // Used by the Shell (e.g. for shortcuts).
  14,   // Recommended by the MSDN as a nice to have icon size.
  16,   // Toolbar, Application and Shell icon sizes.
  22,   // Recommended by the MSDN as a nice to have icon size.
  24,   // Used by the Shell (e.g. for shortcuts).
  32,   // Toolbar, Dialog and Wizard icon size.
  40,   // Quick Launch.
  48,   // Alt+Tab icon size.
  64,   // Recommended by the MSDN as a nice to have icon size.
  96,   // Recommended by the MSDN as a nice to have icon size.
  128   // Used by the Shell (e.g. for shortcuts).
};

HICON IconUtil::CreateHICONFromSkBitmap(const SkBitmap& bitmap) {
  // Only 32 bit ARGB bitmaps are supported. We also try to perform as many
  // validations as we can on the bitmap.
  SkAutoLockPixels bitmap_lock(bitmap);
  if ((bitmap.getConfig() != SkBitmap::kARGB_8888_Config) ||
      (bitmap.width() <= 0) || (bitmap.height() <= 0) ||
      (bitmap.getPixels() == NULL)) {
    return NULL;
  }

  // We start by creating a DIB which we'll use later on in order to create
  // the HICON. We use BITMAPV5HEADER since the bitmap we are about to convert
  // may contain an alpha channel and the V5 header allows us to specify the
  // alpha mask for the DIB.
  BITMAPV5HEADER bitmap_header;
  InitializeBitmapHeader(&bitmap_header, bitmap.width(), bitmap.height());
  void* bits;
  HDC hdc = ::GetDC(NULL);
  HBITMAP dib;
  dib = ::CreateDIBSection(hdc, reinterpret_cast<BITMAPINFO*>(&bitmap_header),
                           DIB_RGB_COLORS, &bits, NULL, 0);
  DCHECK(dib);
  ::ReleaseDC(NULL, hdc);
  memcpy(bits, bitmap.getPixels(), bitmap.width() * bitmap.height() * 4);

  // Icons are generally created using an AND and XOR masks where the AND
  // specifies boolean transparency (the pixel is either opaque or
  // transparent) and the XOR mask contains the actual image pixels. However,
  // since our bitmap has an alpha channel, the AND monochrome bitmap won't
  // actually be used for computing the pixel transparency. Since every icon
  // must have an AND mask bitmap, we go ahead and create one so that we can
  // associate it with the ICONINFO structure we'll later pass to
  // ::CreateIconIndirect(). The monochrome bitmap is created such that all the
  // pixels are opaque.
  HBITMAP mono_bitmap = ::CreateBitmap(bitmap.width(), bitmap.height(),
                                       1, 1, NULL);
  DCHECK(mono_bitmap);
  ICONINFO icon_info;
  icon_info.fIcon = TRUE;
  icon_info.xHotspot = 0;
  icon_info.yHotspot = 0;
  icon_info.hbmMask = mono_bitmap;
  icon_info.hbmColor = dib;
  HICON icon = ::CreateIconIndirect(&icon_info);
  ::DeleteObject(dib);
  ::DeleteObject(mono_bitmap);
  return icon;
}

SkBitmap* IconUtil::CreateSkBitmapFromHICON(HICON icon, const gfx::Size& s) {
  // We start with validating parameters.
  ICONINFO icon_info;
  if (!icon || !(::GetIconInfo(icon, &icon_info)) ||
      !icon_info.fIcon || (s.width() <= 0) || (s.height() <= 0)) {
    return NULL;
  }

  // Allocating memory for the SkBitmap object. We are going to create an ARGB
  // bitmap so we should set the configuration appropriately.
  SkBitmap* bitmap = new SkBitmap;
  DCHECK(bitmap);
  bitmap->setConfig(SkBitmap::kARGB_8888_Config, s.width(), s.height());
  bitmap->allocPixels();
  SkAutoLockPixels bitmap_lock(*bitmap);

  // Now we should create a DIB so that we can use ::DrawIconEx in order to
  // obtain the icon's image.
  BITMAPV5HEADER h;
  InitializeBitmapHeader(&h, s.width(), s.height());
  HDC dc = ::GetDC(NULL);
  unsigned int* bits;
  HBITMAP dib = ::CreateDIBSection(dc,
                                   reinterpret_cast<BITMAPINFO*>(&h),
                                   DIB_RGB_COLORS,
                                   reinterpret_cast<void**>(&bits),
                                   NULL,
                                   0);
  DCHECK(dib);
  HDC dib_dc = CreateCompatibleDC(dc);
  DCHECK(dib_dc);
  ::SelectObject(dib_dc, dib);

  // Windows icons are defined using two different masks. The XOR mask, which
  // represents the icon image and an AND mask which is a monochrome bitmap
  // which indicates the transparency of each pixel.
  //
  // To make things more complex, the icon image itself can be an ARGB bitmap
  // and therefore contain an alpha channel which specifies the transparency
  // for each pixel. Unfortunately, there is no easy way to determine whether
  // or not a bitmap has an alpha channel and therefore constructing the bitmap
  // for the icon is nothing but straightforward.
  //
  // The idea is to read the AND mask but use it only if we know for sure that
  // the icon image does not have an alpha channel. The only way to tell if the
  // bitmap has an alpha channel is by looking through the pixels and checking
  // whether there are non-zero alpha bytes.
  //
  // We start by drawing the AND mask into our DIB.
  memset(bits, 0, s.width() * s.height() * 4);
  ::DrawIconEx(dib_dc, 0, 0, icon, s.width(), s.height(), 0, NULL, DI_MASK);

  // Capture boolean opacity. We may not use it if we find out the bitmap has
  // an alpha channel.
  bool* opaque = new bool[s.width() * s.height()];
  DCHECK(opaque);
  int x, y;
  for (y = 0; y < s.height(); ++y) {
    for (x = 0; x < s.width(); ++x)
      opaque[(y * s.width()) + x] = !bits[(y * s.width()) + x];
  }

  // Then draw the image itself which is really the XOR mask.
  memset(bits, 0, s.width() * s.height() * 4);
  ::DrawIconEx(dib_dc, 0, 0, icon, s.width(), s.height(), 0, NULL, DI_NORMAL);
  memcpy(bitmap->getPixels(),
         static_cast<void*>(bits),
         s.width() * s.height() * 4);

  // Finding out whether the bitmap has an alpha channel.
  bool bitmap_has_alpha_channel = false;
  unsigned int* p = static_cast<unsigned int*>(bitmap->getPixels());
  for (y = 0; y < s.height(); ++y) {
    for (x = 0; x < s.width(); ++x) {
      if ((*p & 0xff000000) != 0) {
        bitmap_has_alpha_channel = true;
        break;
      }
      p++;
    }

    if (bitmap_has_alpha_channel) {
      break;
    }
  }

  // If the bitmap does not have an alpha channel, we need to build it using
  // the previously captured AND mask. Otherwise, we are done.
  if (!bitmap_has_alpha_channel) {
    p = static_cast<unsigned int*>(bitmap->getPixels());
    for (y = 0; y < s.height(); ++y) {
      for (x = 0; x < s.width(); ++x) {
        DCHECK_EQ((*p & 0xff000000), 0);
        if (opaque[(y * s.width()) + x]) {
          *p |= 0xff000000;
        } else {
          *p &= 0x00ffffff;
        }
        p++;
      }
    }
  }

  delete [] opaque;
  ::DeleteDC(dib_dc);
  ::DeleteObject(dib);
  ::ReleaseDC(NULL, dc);

  return bitmap;
}

bool IconUtil::CreateIconFileFromSkBitmap(const SkBitmap& bitmap,
                                          const std::wstring& icon_file_name) {
  // Only 32 bit ARGB bitmaps are supported. We also make sure the bitmap has
  // been properly initialized.
  SkAutoLockPixels bitmap_lock(bitmap);
  if ((bitmap.getConfig() != SkBitmap::kARGB_8888_Config) ||
      (bitmap.height() <= 0) || (bitmap.width() <= 0) ||
      (bitmap.getPixels() == NULL)) {
    return false;
  }

  // We start by creating the file.
  win_util::ScopedHandle icon_file(::CreateFile(icon_file_name.c_str(),
                                                GENERIC_WRITE,
                                                0,
                                                NULL,
                                                CREATE_ALWAYS,
                                                FILE_ATTRIBUTE_NORMAL,
                                                NULL));

  if (icon_file.Get() == INVALID_HANDLE_VALUE) {
    return false;
  }

  // Creating a set of bitmaps corresponding to the icon images we'll end up
  // storing in the icon file. Each bitmap is created by resizing the given
  // bitmap to the desired size.
  std::vector<SkBitmap> bitmaps;
  CreateResizedBitmapSet(bitmap, &bitmaps);
  int bitmap_count = static_cast<int>(bitmaps.size());
  DCHECK_GT(bitmap_count, 0);

  // Computing the total size of the buffer we need in order to store the
  // images in the desired icon format.
  int buffer_size = ComputeIconFileBufferSize(bitmaps);
  unsigned char* buffer = new unsigned char[buffer_size];
  DCHECK_NE(buffer, static_cast<unsigned char*>(NULL));
  memset(buffer, 0, buffer_size);

  // Setting the information in the structures residing within the buffer.
  // First, we set the information which doesn't require iterating through the
  // bitmap set and then we set the bitmap specific structures. In the latter
  // step we also copy the actual bits.
  ICONDIR* icon_dir = reinterpret_cast<ICONDIR*>(buffer);
  icon_dir->idType = kResourceTypeIcon;
  icon_dir->idCount = bitmap_count;
  int icon_dir_count = bitmap_count - 1;
  int offset = sizeof(ICONDIR) + (sizeof(ICONDIRENTRY) * icon_dir_count);
  for (int i = 0; i < bitmap_count; i++) {
    ICONIMAGE* image = reinterpret_cast<ICONIMAGE*>(buffer + offset);
    DCHECK_LT(offset, buffer_size);
    int icon_image_size = 0;
    SetSingleIconImageInformation(bitmaps[i],
                                  i,
                                  icon_dir,
                                  image,
                                  offset,
                                  &icon_image_size);
    DCHECK_GT(icon_image_size, 0);
    offset += icon_image_size;
  }
  DCHECK_EQ(offset, buffer_size);

  // Finally, writing the data info the file.
  DWORD bytes_written;
  bool delete_file = false;
  if (!WriteFile(icon_file.Get(), buffer, buffer_size, &bytes_written, NULL) ||
      bytes_written != buffer_size) {
    delete_file = true;
  }

  ::CloseHandle(icon_file.Take());
  delete [] buffer;
  if (delete_file) {
    bool success = file_util::Delete(icon_file_name, false);
    DCHECK(success);
  }

  return !delete_file;
}

int IconUtil::GetIconDimensionCount() {
  return sizeof(icon_dimensions_) / sizeof(icon_dimensions_[0]);
}

void IconUtil::InitializeBitmapHeader(BITMAPV5HEADER* header, int width,
                                      int height) {
  DCHECK(header);
  memset(header, 0, sizeof(BITMAPV5HEADER));
  header->bV5Size = sizeof(BITMAPV5HEADER);

  // Note that icons are created using top-down DIBs so we must negate the
  // value used for the icon's height.
  header->bV5Width = width;
  header->bV5Height = -height;
  header->bV5Planes = 1;
  header->bV5Compression = BI_RGB;

  // Initializing the bitmap format to 32 bit ARGB.
  header->bV5BitCount = 32;
  header->bV5RedMask = 0x00FF0000;
  header->bV5GreenMask = 0x0000FF00;
  header->bV5BlueMask = 0x000000FF;
  header->bV5AlphaMask = 0xFF000000;

  // Use the system color space.  The default value is LCS_CALIBRATED_RGB, which
  // causes us to crash if we don't specify the approprite gammas, etc.  See
  // <http://msdn.microsoft.com/en-us/library/ms536531(VS.85).aspx> and
  // <http://b/1283121>.
  header->bV5CSType = LCS_WINDOWS_COLOR_SPACE;
}

void IconUtil::SetSingleIconImageInformation(const SkBitmap& bitmap,
                                             int index,
                                             ICONDIR* icon_dir,
                                             ICONIMAGE* icon_image,
                                             int image_offset,
                                             int* image_byte_count) {
  DCHECK_GE(index, 0);
  DCHECK_NE(icon_dir, static_cast<ICONDIR*>(NULL));
  DCHECK_NE(icon_image, static_cast<ICONIMAGE*>(NULL));
  DCHECK_GT(image_offset, 0);
  DCHECK_NE(image_byte_count, static_cast<int*>(NULL));

  // We start by computing certain image values we'll use later on.
  int xor_mask_size;
  int and_mask_size;
  int bytes_in_resource;
  ComputeBitmapSizeComponents(bitmap,
                              &xor_mask_size,
                              &and_mask_size,
                              &bytes_in_resource);

  icon_dir->idEntries[index].bWidth = static_cast<BYTE>(bitmap.width());
  icon_dir->idEntries[index].bHeight = static_cast<BYTE>(bitmap.height());
  icon_dir->idEntries[index].wPlanes = 1;
  icon_dir->idEntries[index].wBitCount = 32;
  icon_dir->idEntries[index].dwBytesInRes = bytes_in_resource;
  icon_dir->idEntries[index].dwImageOffset = image_offset;
  icon_image->icHeader.biSize = sizeof(BITMAPINFOHEADER);

  // The width field in the BITMAPINFOHEADER structure accounts for the height
  // of both the AND mask and the XOR mask so we need to multiply the bitmap's
  // height by 2. The same does NOT apply to the width field.
  icon_image->icHeader.biHeight = bitmap.height() * 2;
  icon_image->icHeader.biWidth = bitmap.width();
  icon_image->icHeader.biPlanes = 1;
  icon_image->icHeader.biBitCount = 32;

  // We use a helper function for copying to actual bits from the SkBitmap
  // object into the appropriate space in the buffer. We use a helper function
  // (rather than just copying the bits) because there is no way to specify the
  // orientation (bottom-up vs. top-down) of a bitmap residing in a .ico file.
  // Thus, if we just copy the bits, we'll end up with a bottom up bitmap in
  // the .ico file which will result in the icon being displayed upside down.
  // The helper function copies the image into the buffer one scanline at a
  // time.
  //
  // Note that we don't need to initialize the AND mask since the memory
  // allocated for the icon data buffer was initialized to zero. The icon we
  // create will therefore use an AND mask containing only zeros, which is OK
  // because the underlying image has an alpha channel. An AND mask containing
  // only zeros essentially means we'll initially treat all the pixels as
  // opaque.
  unsigned char* image_addr = reinterpret_cast<unsigned char*>(icon_image);
  unsigned char* xor_mask_addr = image_addr + sizeof(BITMAPINFOHEADER);
  CopySkBitmapBitsIntoIconBuffer(bitmap, xor_mask_addr, xor_mask_size);
  *image_byte_count = bytes_in_resource;
}

void IconUtil::CopySkBitmapBitsIntoIconBuffer(const SkBitmap& bitmap,
                                              unsigned char* buffer,
                                              int buffer_size) {
  SkAutoLockPixels bitmap_lock(bitmap);
  unsigned char* bitmap_ptr = static_cast<unsigned char*>(bitmap.getPixels());
  int bitmap_size = bitmap.height() * bitmap.width() * 4;
  DCHECK_EQ(buffer_size, bitmap_size);
  for (int i = 0; i < bitmap_size; i += bitmap.width() * 4) {
    memcpy(buffer + bitmap_size - bitmap.width() * 4 - i,
           bitmap_ptr + i,
           bitmap.width() * 4);
  }
}

void IconUtil::CreateResizedBitmapSet(const SkBitmap& bitmap_to_resize,
                                      std::vector<SkBitmap>* bitmaps) {
  DCHECK_NE(bitmaps, static_cast<std::vector<SkBitmap>* >(NULL));
  DCHECK_EQ(static_cast<int>(bitmaps->size()), 0);

  bool inserted_original_bitmap = false;
  for (int i = 0; i < GetIconDimensionCount(); i++) {
    // If the dimensions of the bitmap we are resizing are the same as the
    // current dimensions, then we should insert the bitmap and not a resized
    // bitmap. If the bitmap's dimensions are smaller, we insert our bitmap
    // first so that the bitmaps we return in the vector are sorted based on
    // their dimensions.
    if (!inserted_original_bitmap) {
      if ((bitmap_to_resize.width() == icon_dimensions_[i]) &&
          (bitmap_to_resize.height() == icon_dimensions_[i])) {
        bitmaps->push_back(bitmap_to_resize);
        inserted_original_bitmap = true;
        continue;
      }

      if ((bitmap_to_resize.width() < icon_dimensions_[i]) &&
          (bitmap_to_resize.height() < icon_dimensions_[i])) {
        bitmaps->push_back(bitmap_to_resize);
        inserted_original_bitmap = true;
      }
    }
    bitmaps->push_back(skia::ImageOperations::Resize(
        bitmap_to_resize, skia::ImageOperations::RESIZE_LANCZOS3,
        icon_dimensions_[i], icon_dimensions_[i]));
  }

  if (!inserted_original_bitmap) {
    bitmaps->push_back(bitmap_to_resize);
  }
}

int IconUtil::ComputeIconFileBufferSize(const std::vector<SkBitmap>& set) {
  // We start by counting the bytes for the structures that don't depend on the
  // number of icon images. Note that sizeof(ICONDIR) already accounts for a
  // single ICONDIRENTRY structure, which is why we subtract one from the
  // number of bitmaps.
  int total_buffer_size = 0;
  total_buffer_size += sizeof(ICONDIR);
  int bitmap_count = static_cast<int>(set.size());
  total_buffer_size += sizeof(ICONDIRENTRY) * (bitmap_count - 1);
  int dimension_count = GetIconDimensionCount();
  DCHECK_GE(bitmap_count, dimension_count);

  // Add the bitmap specific structure sizes.
  for (int i = 0; i < bitmap_count; i++) {
    int xor_mask_size;
    int and_mask_size;
    int bytes_in_resource;
    ComputeBitmapSizeComponents(set[i],
                                &xor_mask_size,
                                &and_mask_size,
                                &bytes_in_resource);
    total_buffer_size += bytes_in_resource;
  }
  return total_buffer_size;
}

void IconUtil::ComputeBitmapSizeComponents(const SkBitmap& bitmap,
                                           int* xor_mask_size,
                                           int* and_mask_size,
                                           int* bytes_in_resource) {
  // The XOR mask size is easy to calculate since we only deal with 32bpp
  // images.
  *xor_mask_size = bitmap.width() * bitmap.height() * 4;

  // Computing the AND mask is a little trickier since it is a monochrome
  // bitmap (regardless of the number of bits per pixels used in the XOR mask).
  // There are two things we must make sure we do when computing the AND mask
  // size:
  //
  // 1. Make sure the right number of bytes is allocated for each AND mask
  //    scan line in case the number of pixels in the image is not divisible by
  //    8. For example, in a 15X15 image, 15 / 8 is one byte short of
  //    containing the number of bits we need in order to describe a single
  //    image scan line so we need to add a byte. Thus, we need 2 bytes instead
  //    of 1 for each scan line.
  //
  // 2. Make sure each scan line in the AND mask is 4 byte aligned (so that the
  //    total icon image has a 4 byte alignment). In the 15X15 image example
  //    above, we can not use 2 bytes so we increase it to the next multiple of
  //    4 which is 4.
  //
  // Once we compute the size for a singe AND mask scan line, we multiply that
  // number by the image height in order to get the total number of bytes for
  // the AND mask. Thus, for a 15X15 image, we need 15 * 4 which is 60 bytes
  // for the monochrome bitmap representing the AND mask.
  int and_line_length = (bitmap.width() + 7) >> 3;
  and_line_length = (and_line_length + 3) & ~3;
  *and_mask_size = and_line_length * bitmap.height();
  int masks_size = *xor_mask_size + *and_mask_size;
  *bytes_in_resource = masks_size + sizeof(BITMAPINFOHEADER);
}
