// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NativeImageSkia_h
#define NativeImageSkia_h

#include "SkBitmap.h"
#include "IntSize.h"

// This object is used as the "native image" in our port. When WebKit uses
// "NativeImagePtr", it is a pointer to this type. It is an SkBitmap, but also
// stores a cached resized image.
class NativeImageSkia : public SkBitmap {
public:
    NativeImageSkia();

    // Returns the number of bytes of image data. This includes the cached
    // resized version if there is one.
    int decodedSize() const;

    // Sets the data complete flag. This is called by the image decoder when
    // all data is complete, and used by us to know whether we can cache
    // resized images.
    void setDataComplete() { m_isDataComplete = true; }

    // Returns true if the entire image has been decoded.
    bool isDataComplete() const { return m_isDataComplete; }

    // We can keep a resized version of the bitmap cached on this object.
    // This function will return true if there is a cached version of the
    // given image subset with the given dimensions.
    bool hasResizedBitmap(int width, int height) const;

    // This will return an existing resized image, or generate a new one of
    // the specified size and store it in the cache. Subsetted images can not
    // be cached unless the subset is the entire bitmap.
    SkBitmap resizedBitmap(int width, int height) const;

    // Returns true if the given resize operation should either resize the whole
    // image and cache it, or resize just the part it needs and throw the result
    // away.
    //
    // On the one hand, if only a small subset is desired, then we will waste a
    // lot of time resampling the entire thing, so we only want to do exactly
    // what's required. On the other hand, resampling the entire bitmap is
    // better if we're going to be using it more than once (like a bitmap
    // scrolling on and off the screen. Since we only cache when doing the
    // entire thing, it's best to just do it up front.
    bool shouldCacheResampling(int dest_width,
                               int dest_height,
                               int dest_subset_width,
                               int dest_subset_height) const;

private:
    // Set to true when the data is complete. Before the entire image has
    // loaded, we do not want to cache a resize.
    bool m_isDataComplete;

    // The cached bitmap. This will be empty() if there is no cached image.
    mutable SkBitmap m_resizedImage;

    // References how many times that the image size has been requested for
    // the last size.
    //
    // Every time we get a request, if it matches the m_lastRequestSize, we'll
    // increment the counter, and if not, we'll reset the counter and save the
    // size.
    //
    // This allows us to see if many requests have been made for the same
    // resized image, we know that we should probably cache it, even if all of
    // those requests individually are small and would not otherwise be cached.
    mutable WebCore::IntSize m_lastRequestSize;
    mutable int m_resizeRequests;
};

#endif  // NativeImageSkia_h

