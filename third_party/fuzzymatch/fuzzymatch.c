// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* Fuzzy pixel test matching.
 *
 * This is designed to compare two layout test images (RGB 800x600) and manage
 * to ignore the noise caused by the font renderers choosing slightly different
 * pixels.
 *
 *    A             B
 *    |             |
 *    |--->delta<---|
 *           |
 *           V
 *          gray
 *           |
 *           V
 *         binary
 *           |
 *    |-------------|
 *    |             |
 *   1x3           3x1   Morphological openings
 *    |             |
 *    |-----OR------|
 *           |
 *           V
 *     count pixels
 *
 * The result is that three different pixels in a row (vertically or
 * horizontally) will cause the match to fail and the binary exits with code 1.
 * Otherwise the binary exists with code 0.
 *
 * This requires leptonica to be installed. On Ubuntu do
 *   # apt-get install libleptonica libleptonica-dev
 *
 * Build with:
 *   % gcc -o fuzzymatch fuzzymatch.c -llept -ljpeg -ltiff -lpng -lz -lm -Wall -O2
 */

#include <unistd.h>
#include <stdio.h>
#include <leptonica/allheaders.h>

static int
usage(const char *argv0) {
  fprintf(stderr, "Usage: %s <input a> <input b>\n", argv0);
  return 1;
}

int
main(int argc, char **argv) {
  if (argc != 3)
    return usage(argv[0]);

  PIX *a = pixRead(argv[1]);
  PIX *b = pixRead(argv[2]);

  if (!a) {
    fprintf(stderr, "Failed to open %s\n", argv[1]);
    return 1;
  }

  if (!b) {
    fprintf(stderr, "Failed to open %s\n", argv[1]);
    return 1;
  }

  if (pixGetWidth(a) != pixGetWidth(b) ||
      pixGetHeight(a) != pixGetHeight(b)) {
    fprintf(stderr, "Inputs are difference sizes\n");
    return 1;
  }

  PIX *delta = pixAbsDifference(a, b);
  pixInvert(delta, delta);
  pixDestroy(&a);
  pixDestroy(&b);

  PIX *deltagray = pixConvertRGBToGray(delta, 0, 0, 0);
  pixDestroy(&delta);

  PIX *deltabinary = pixThresholdToBinary(deltagray, 254);

  PIX *hopened = pixOpenBrick(NULL, deltabinary, 3, 1);
  PIX *vopened = pixOpenBrick(NULL, deltabinary, 1, 3);
  pixDestroy(&deltabinary);

  PIX *opened = pixOr(NULL, hopened, vopened);
  pixDestroy(&hopened);
  pixDestroy(&vopened);

  l_int32 count;
  pixCountPixels(opened, &count, NULL);
  fprintf(stderr, "%d\n", count);

  return count;
}
