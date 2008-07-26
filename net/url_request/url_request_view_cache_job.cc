// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "net/url_request/url_request_view_cache_job.h"

#include "base/string_util.h"
#include "net/base/escape.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/http/http_response_info.h"

#define VIEW_CACHE_HEAD \
  "<html><body><table>"

#define VIEW_CACHE_TAIL \
  "</table></body></html>"

static void HexDump(const char *buf, size_t buf_len, std::string* result) {
  const size_t kMaxRows = 16;
  int offset = 0;

  const unsigned char *p;
  while (buf_len) {
    StringAppendF(result, "%08x:  ", offset);
    offset += kMaxRows;

    p = (const unsigned char *) buf;

    size_t i;
    size_t row_max = std::min(kMaxRows, buf_len);

    // print hex codes:
    for (i = 0; i < row_max; ++i)
      StringAppendF(result, "%02x  ", *p++);
    for (i = row_max; i < kMaxRows; ++i)
      result->append("    ");

    // print ASCII glyphs if possible:
    p = (const unsigned char *) buf;
    for (i = 0; i < row_max; ++i, ++p) {
      if (*p < 0x7F && *p > 0x1F) {
        AppendEscapedCharForHTML(*p, result);
      } else {
        result->push_back('.');
      }
    }

    result->push_back('\n');

    buf += row_max;
    buf_len -= row_max;
  }
}

static std::string FormatEntryInfo(disk_cache::Entry* entry) {
  std::string key = EscapeForHTML(entry->GetKey());
  std::string row =
      "<tr><td><a href=\"view-cache:" + key + "\">" + key + "</a></td></tr>";
  return row;
}

static std::string FormatEntryDetails(disk_cache::Entry* entry) {
  std::string result = EscapeForHTML(entry->GetKey());

  net::HttpResponseInfo response;
  net::HttpCache::ReadResponseInfo(entry, &response);

  if (response.headers) {
    result.append("<hr><pre>");
    result.append(EscapeForHTML(response.headers->GetStatusLine()));
    result.push_back('\n');

    void* iter = NULL;
    std::string name, value;
    while (response.headers->EnumerateHeaderLines(&iter, &name, &value)) {
      result.append(EscapeForHTML(name));
      result.append(": ");
      result.append(EscapeForHTML(value));
      result.push_back('\n');
    }
    result.append("</pre>");
  }

  for (int i = 0; i < 2; ++i) {
    result.append("<hr><pre>");

    int data_size = entry->GetDataSize(i);

    char* data = new char[data_size];
    if (entry->ReadData(i, 0, data, data_size, NULL) == data_size)
      HexDump(data, data_size, &result);

    result.append("</pre>");
  }

  return result;
}

static std::string FormatStatistics(disk_cache::Backend* disk_cache) {
  std::vector<std::pair<std::string, std::string> > stats;
  disk_cache->GetStats(&stats);
  std::string result;

  for (size_t index = 0; index < stats.size(); index++) {
    result.append(stats[index].first);
    result.append(": ");
    result.append(stats[index].second);
    result.append("<br/>\n");
  }

  return result;
}

// static
URLRequestJob* URLRequestViewCacheJob::Factory(URLRequest* request,
                                               const std::string& scheme) {
  return new URLRequestViewCacheJob(request);
}

bool URLRequestViewCacheJob::GetData(std::string* mime_type,
                                     std::string* charset,
                                     std::string* data) const {
  mime_type->assign("text/html");
  charset->assign("UTF-8");

  disk_cache::Backend* disk_cache = GetDiskCache();
  if (!disk_cache) {
    data->assign("no disk cache");
    return true;
  }

  if (request_->url().spec() == "view-cache:") {
    data->assign(VIEW_CACHE_HEAD);
    void* iter = NULL;
    disk_cache::Entry* entry;
    while (disk_cache->OpenNextEntry(&iter, &entry)) {
      data->append(FormatEntryInfo(entry));
      entry->Close();
    }
    data->append(VIEW_CACHE_TAIL);
  } else if (request_->url().spec() == "view-cache:stats") {
    data->assign(FormatStatistics(disk_cache));
  } else {
    disk_cache::Entry* entry;
    if (disk_cache->OpenEntry(request_->url().path(), &entry)) {
      data->assign(FormatEntryDetails(entry));
      entry->Close();
    } else {
      data->assign("no matching cache entry");
    }
  }
  return true;
}

disk_cache::Backend* URLRequestViewCacheJob::GetDiskCache() const {
  if (!request_->context())
    return NULL;

  if (!request_->context()->http_transaction_factory())
    return NULL;

  net::HttpCache* http_cache =
      request_->context()->http_transaction_factory()->GetCache();
  if (!http_cache)
    return NULL;

  return http_cache->disk_cache();
}
