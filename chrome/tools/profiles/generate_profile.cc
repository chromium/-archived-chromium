// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This program generates a user profile and history by randomly generating
// data and feeding it to the history service.

#include "chrome/tools/profiles/thumbnail-inl.h"

#include "base/at_exit.h"
#include "base/file_path.h"
#include "base/gfx/jpeg_codec.h"
#include "base/icu_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/history/history.h"
#include "chrome/common/thumbnail_score.h"
#include "SkBitmap.h"

using base::Time;

// Probabilities of different word lengths, as measured from Darin's profile.
//   kWordLengthProbabilities[n-1] = P(word of length n)
const float kWordLengthProbabilities[] = { 0.069f, 0.132f, 0.199f,
  0.137f, 0.088f, 0.115f, 0.081f, 0.055f, 0.034f, 0.021f, 0.019f, 0.018f,
  0.007f, 0.007f, 0.005f, 0.004f, 0.003f, 0.003f, 0.003f };

// Return a float uniformly in [0,1].
// Useful for making probabilistic decisions.
float RandomFloat() {
  return rand() / static_cast<float>(RAND_MAX);
}

// Return an integer uniformly in [min,max).
int RandomInt(int min, int max) {
  return min + (rand() % (max-min));
}

// Return a string of |count| lowercase random characters.
std::wstring RandomChars(int count) {
  std::wstring str;
  for (int i = 0; i < count; ++i)
    str += L'a' + rand() % 26;
  return str;
}

std::wstring RandomWord() {
  // TODO(evanm): should we instead use the markov chain based
  // version of this that I already wrote?

  // Sample a word length from kWordLengthProbabilities.
  float sample = RandomFloat();
  int i;
  for (i = 0; i < arraysize(kWordLengthProbabilities); ++i) {
    sample -= kWordLengthProbabilities[i];
    if (sample < 0) break;
  }
  const int word_length = i + 1;
  return RandomChars(word_length);
}

// Return a string of |count| random words.
std::wstring RandomWords(int count) {
  std::wstring str;
  for (int i = 0; i < count; ++i) {
    if (!str.empty())
      str += L' ';
    str += RandomWord();
  }
  return str;
}

// Return a random URL-looking string.
GURL ConstructRandomURL() {
  return GURL(std::wstring(L"http://") + RandomChars(3) + L".com/" +
      RandomChars(RandomInt(5,20)));
}

// Return a random page title-looking string.
std::wstring ConstructRandomTitle() {
  return RandomWords(RandomInt(3, 15));
}

// Return a random string that could function as page contents.
std::wstring ConstructRandomPage() {
  return RandomWords(RandomInt(10, 4000));
}

// Insert a batch of |batch_size| URLs, starting at pageid |page_id|.
// When history_only is set, we will not generate thumbnail or full text data.
void InsertURLBatch(const std::wstring& profile_dir, int page_id,
                    int batch_size, bool history_only) {
  scoped_refptr<HistoryService> history_service(new HistoryService);
  if (!history_service->Init(FilePath::FromWStringHack(profile_dir), NULL)) {
    printf("Could not init the history service\n");
    exit(1);
  }

  // Probability of following a link on the current "page"
  // (vs randomly jumping to a new page).
  const float kFollowLinkProbability = 0.85f;
  // Probability of visiting a page we've visited before.
  const float kRevisitLinkProbability = 0.1f;
  // Probability of a URL being "good enough" to revisit.
  const float kRevisitableURLProbability = 0.05f;
  // Probability of a URL being the end of a redirect chain.
  const float kRedirectProbability = 0.05f;

  // A list of URLs that we sometimes revisit.
  std::vector<GURL> revisit_urls;

  // Scoping value for page IDs (required by the history service).
  void* id_scope = reinterpret_cast<void*>(1);

  printf("Inserting %d URLs...\n", batch_size);
  GURL previous_url;
  PageTransition::Type transition = PageTransition::TYPED;
  const int end_page_id = page_id + batch_size;
  for (; page_id < end_page_id; ++page_id) {
    // Randomly decide whether this new URL simulates following a link or
    // whether it's a jump to a new URL.
    if (!previous_url.is_empty() && RandomFloat() < kFollowLinkProbability) {
      transition = PageTransition::LINK;
    } else {
      previous_url = GURL();
      transition = PageTransition::TYPED;
    }

    // Pick a URL, either newly at random or from our list of previously
    // visited URLs.
    GURL url;
    if (!revisit_urls.empty() && RandomFloat() < kRevisitLinkProbability) {
      // Draw a URL from revisit_urls at random.
      url = revisit_urls[RandomInt(0, static_cast<int>(revisit_urls.size()))];
    } else {
      url = ConstructRandomURL();
    }

    // Randomly construct a redirect chain.
    HistoryService::RedirectList redirects;
    if (RandomFloat() < kRedirectProbability) {
      const int redir_count = RandomInt(1, 4);
      for (int i = 0; i < redir_count; ++i)
        redirects.push_back(ConstructRandomURL());
      redirects.push_back(url);
    }

    // Add all of this information to the history service.
    history_service->AddPage(url,
                             id_scope, page_id,
                             previous_url, transition,
                             redirects);
    ThumbnailScore score(0.75, false, false);
    history_service->SetPageTitle(url, ConstructRandomTitle());
    if (!history_only) {
      history_service->SetPageContents(url, ConstructRandomPage());
      if (RandomInt(0, 2) == 0) {
        scoped_ptr<SkBitmap> bitmap(
            JPEGCodec::Decode(kGoogleThumbnail, sizeof(kGoogleThumbnail)));
        history_service->SetPageThumbnail(url, *bitmap, score);
      } else {
        scoped_ptr<SkBitmap> bitmap(
            JPEGCodec::Decode(kWeewarThumbnail, sizeof(kWeewarThumbnail)));
        history_service->SetPageThumbnail(url, *bitmap, score);
      }
    }

    previous_url = url;

    if (revisit_urls.empty() || RandomFloat() < kRevisitableURLProbability)
      revisit_urls.push_back(url);
  }

  printf("Letting the history service catch up...\n");
  history_service->SetOnBackendDestroyTask(new MessageLoop::QuitTask);
  history_service->Cleanup();
  history_service = NULL;
  MessageLoop::current()->Run();
}

int main(int argc, const char* argv[]) {
  base::EnableTerminationOnHeapCorruption();
  base::AtExitManager exit_manager;

  int next_arg = 1;
  bool history_only = false;
  if (strcmp(argv[next_arg], "--history-only") == 0) {
    history_only = true;
    next_arg++;
  }

  // We require two arguments: urlcount and profiledir.
  if (argc - next_arg < 2) {
    printf("usage: %s [--history-only] <urlcount> <profiledir>\n", argv[0]);
    printf("\n  --history-only Generate only history, not thumbnails or full");
    printf("\n                 text index data.\n\n");
    return -1;
  }

  const int url_count = atoi(argv[next_arg++]);
  std::wstring profile_dir = UTF8ToWide(argv[next_arg++]);

  MessageLoop main_message_loop;

  srand(static_cast<unsigned int>(Time::Now().ToInternalValue()));
  icu_util::Initialize();

  // The maximum number of URLs to insert into history in one batch.
  const int kBatchSize = 2000;
  int page_id = 0;
  while (page_id < url_count) {
    const int batch_size = std::min(kBatchSize, url_count - page_id);
    InsertURLBatch(profile_dir, page_id, batch_size, history_only);
    page_id += batch_size;
  }

  return 0;
}
