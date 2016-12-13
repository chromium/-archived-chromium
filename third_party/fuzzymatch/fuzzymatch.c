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
 *   # apt-get install libleptonica libleptonica-dev libtiff4-dev
 *
 * Build with:
 *   % gcc -o fuzzymatch fuzzymatch.c -llept -ljpeg -ltiff -lpng -lz -lm -Wall -O2
 *
 * Options:
 *   --highlight: write highlight.png which is a copy of the first image
 *     argument where the differing areas (after noise removal) are ringed
 *     in red
 *   --no-ignore-scrollbars: usually the rightmost 15px of the image are
 *     ignored to account for scrollbars. Use this flag to include them in
 *     consideration
 */

#include <unistd.h>
#include <stdio.h>
#include <leptonica/allheaders.h>

static int
usage(const char *argv0) {
  fprintf(stderr, "Usage: %s [--highlight] [--no-ignore-scrollbars] "
                  "[--output filename] "
                  "<input a> <input b>\n", argv0);
  return 1;
}

int
main(int argc, char **argv) {
  if (argc < 3)
    return usage(argv[0]);

  char highlight = 0;
  char ignore_scrollbars = 1;
  /* Default output filename; can be overridden by command line. */
  const char *output_filename = "highlight.png";

  int argi = 1;

  for (; argi < argc; ++argi) {
    if (strcmp("--highlight", argv[argi]) == 0) {
      highlight = 1;
    } else if (strcmp("--no-ignore-scrollbars", argv[argi]) == 0) {
      ignore_scrollbars = 0;
    } else if (strcmp("--output", argv[argi]) == 0) {
      if (argi + 1 >= argc) {
        fprintf(stderr, "missing argument to --output\n");
        return 1;
      }
      output_filename = argv[++argi];
    } else {
      break;
    }
  }

  if (argc - argi < 2)
    return usage(argv[0]);

  PIX *a = pixRead(argv[argi]);
  PIX *b = pixRead(argv[argi + 1]);

  if (!a) {
    fprintf(stderr, "Failed to open %s\n", argv[argi]);
    return 1;
  }

  if (!b) {
    fprintf(stderr, "Failed to open %s\n", argv[argi + 1]);
    return 1;
  }

  if (pixGetWidth(a) != pixGetWidth(b) ||
      pixGetHeight(a) != pixGetHeight(b)) {
    fprintf(stderr, "Inputs are difference sizes\n");
    return 1;
  }

  PIX *delta = pixAbsDifference(a, b);
  pixInvert(delta, delta);
  if (!highlight)
    pixDestroy(&a);
  pixDestroy(&b);

  PIX *deltagray = pixConvertRGBToGray(delta, 0, 0, 0);
  pixDestroy(&delta);

  PIX *deltabinary = pixThresholdToBinary(deltagray, 254);
  PIX *deltabinaryclipped;
  const int clipwidth = pixGetWidth(deltabinary) - 15;
  const int clipheight = pixGetHeight(deltabinary) - 15;

  if (ignore_scrollbars && clipwidth > 0 && clipheight > 0) {
    BOX *clip = boxCreate(0, 0, clipwidth, clipheight);

    deltabinaryclipped = pixClipRectangle(deltabinary, clip, NULL);
    boxDestroy(&clip);
    pixDestroy(&deltabinary);
  } else {
    deltabinaryclipped = deltabinary;
    deltabinary = NULL;
  }

  PIX *hopened = pixOpenBrick(NULL, deltabinaryclipped, 3, 1);
  PIX *vopened = pixOpenBrick(NULL, deltabinaryclipped, 1, 3);
  pixDestroy(&deltabinaryclipped);

  PIX *opened = pixOr(NULL, hopened, vopened);
  pixDestroy(&hopened);
  pixDestroy(&vopened);

  l_int32 count;
  pixCountPixels(opened, &count, NULL);
  fprintf(stderr, "%d\n", count);

  if (count && highlight) {
    PIX *d1 = pixDilateBrick(NULL, opened, 7, 7);
    PIX *d2 = pixDilateBrick(NULL, opened, 3, 3);
    pixInvert(d2, d2);
    pixAnd(d1, d1, d2);
    pixPaintThroughMask(a, d1, 0, 0, 0xff << 24);
    pixWrite(output_filename, a, IFF_PNG);
  }

  return count > 0;
}
