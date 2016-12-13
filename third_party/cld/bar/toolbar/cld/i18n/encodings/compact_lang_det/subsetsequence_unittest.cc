// Copyright 2008 Google Inc. All Rights Reserved.
// Author: dsites@google.com (Dick Sites)
/*
#include "testing/base/public/gunit.h"
#include "testing/lib/strings/overrun_sensitive_memory_block.h"
#include "cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/subsetsequence.h"

// This always passes. It is just scaffolidng to exercise the subsequence
// facility, which is likely to get abandoned soon. dsites 2008.11.17
//
TEST(SubsetSequence, foo) {
  uint8 dst[120];

  // Create 120-element vector
  printf("Creating %d items:\n", 120);
  SubsetSequence ss;
  for (int i = 0; i < 120; ++i) {
    ss.Add(i);
  }

  // Extract various lengths
  for (int n = 120; n >= 0; --n) {
    ss.Extract(n, dst);
    printf("[%d] ", n);
    for (int i = 0; i < n; ++i) {
      printf("%d ", dst[i]);
    }
    printf("\n");
  }

  printf("\n");
  printf("\n");

  // Create 120-element vector of 7 items each
  printf("Creating %d items:\n", 120);
  ss.Init();
  for (int i = 0; i < 120; ++i) {
    ss.Add(i / 7);
  }

  // Extract various lengths
  for (int n = 120; n >= 0; --n) {
    ss.Extract(n, dst);
    printf("[%d] ", n);
    for (int i = 0; i < n; ++i) {
      printf("%d ", dst[i]);
    }
    printf("\n");
  }

  printf("\n");
  printf("\n");


  // Create 400 element vector of patterns
  int nn1 = 400;
  int divisor = (nn1 + 239) / 240;  // Max inserted value = 240
  printf("Creating %d items:\n", nn1);
  ss.Init();
  for (int i = 0; i < nn1; ++i) {
    ss.Add(i / divisor);
  }

  // Extract 12-item summary lengths
  int n1 = 12;
  ss.Extract(n1, dst);
  printf("[%d] ", n1);
  for (int i = 0; i < n1; ++i) {
    printf("%d ", dst[i]);
  }
  printf("\n");

  printf("\n");
  printf("\n");

  // Create 10**n element vector of patterns
  int pow_10 = 1;
  for (int nn = 0; nn < 9; ++nn) {
    printf("Creating %d items:\n", pow_10);
    int divisor = (pow_10 + 239) / 240;  // Max inserted value = 240
    ss.Init();
    for (int i = 0; i < pow_10; ++i) {
      ss.Add(i / divisor);
    }

    // Extract 12-item summary lengths
    int n = 12;
    ss.Extract(n, dst);
    printf("[%d] ", n);
    for (int i = 0; i < n; ++i) {
      printf("%d ", dst[i]);
    }
    printf("\n");

    pow_10 *= 10;
  }

}
*/
