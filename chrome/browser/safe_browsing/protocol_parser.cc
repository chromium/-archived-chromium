// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Parse the data returned from the SafeBrowsing v2.1 protocol response.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <Winsock2.h>
#elif defined(OS_POSIX)
#include <arpa/inet.h>
#endif

#include "chrome/browser/safe_browsing/protocol_parser.h"

#include "base/logging.h"
#include "base/string_util.h"

namespace {
// Helper function for quick scans of a line oriented protocol. Note that we use
//   std::string::assign(const charT* s, size_type n)
// to copy data into 'line'. This form of 'assign' does not call strlen on
// 'input', which is binary data and is not NULL terminated. 'input' may also
// contain valid NULL bytes in the payload, which a strlen based copy would
// truncate.
bool GetLine(const char* input, int input_len, std::string* line) {
  const char* pos = input;
  while (pos && (pos - input < input_len)) {
    if (*pos == '\n') {
      line->assign(input, pos - input);
      return true;
    }
    ++pos;
  }
  return false;
}
}

//------------------------------------------------------------------------------
// SafeBrowsingParser implementation

SafeBrowsingProtocolParser::SafeBrowsingProtocolParser() {
}

bool SafeBrowsingProtocolParser::ParseGetHash(
    const char* chunk_data,
    int chunk_len,
    const std::string& key,
    bool* re_key,
    std::vector<SBFullHashResult>* full_hashes) {
  full_hashes->clear();
  int length = chunk_len;
  const char* data = chunk_data;

  int offset;
  std::string line;
  if (!key.empty()) {
    if (!GetLine(data, length, &line))
      return false;  // Error! Bad GetHash result.

    if (line == "e:pleaserekey") {
      *re_key = true;
      return true;
    }

    offset = static_cast<int>(line.size()) + 1;
    data += offset;
    length -= offset;

    if (!safe_browsing_util::VerifyMAC(key, line, data, length))
      return false;
  }

  while (length > 0) {
    if (!GetLine(data, length, &line))
      return false;

    offset = static_cast<int>(line.size()) + 1;
    data += offset;
    length -= offset;

    std::vector<std::string> cmd_parts;
    SplitString(line, ':', &cmd_parts);
    if (cmd_parts.size() != 3)
      return false;

    SBFullHashResult full_hash;
    full_hash.list_name = cmd_parts[0];
    full_hash.add_chunk_id = atoi(cmd_parts[1].c_str());
    int full_hash_len = atoi(cmd_parts[2].c_str());

    // Ignore hash results from lists we don't recognize.
    if (safe_browsing_util::GetListId(full_hash.list_name) < 0) {
      data += full_hash_len;
      length -= full_hash_len;
      continue;
    }

    while (full_hash_len > 0) {
      DCHECK(static_cast<size_t>(full_hash_len) >= sizeof(SBFullHash));
      memcpy(&full_hash.hash, data, sizeof(SBFullHash));
      full_hashes->push_back(full_hash);
      data += sizeof(SBFullHash);
      length -= sizeof(SBFullHash);
      full_hash_len -= sizeof(SBFullHash);
    }
  }

  return length == 0;
}

void SafeBrowsingProtocolParser::FormatGetHash(
   const std::vector<SBPrefix>& prefixes, std::string* request) {
  DCHECK(request);

  // Format the request for GetHash.
  request->append(StringPrintf("%d:%d\n",
                               sizeof(SBPrefix),
                               sizeof(SBPrefix) * prefixes.size()));
  for (size_t i = 0; i < prefixes.size(); ++i) {
    request->append(reinterpret_cast<const char*>(&prefixes[i]),
                    sizeof(SBPrefix));
  }
}

bool SafeBrowsingProtocolParser::ParseUpdate(
    const char* chunk_data,
    int chunk_len,
    const std::string& key,
    int* next_update_sec,
    bool* re_key,
    bool* reset,
    std::vector<SBChunkDelete>* deletes,
    std::vector<ChunkUrl>* chunk_urls) {
  DCHECK(next_update_sec);
  DCHECK(deletes);
  DCHECK(chunk_urls);

  int length = chunk_len;
  const char* data = chunk_data;

  // Populated below.
  std::string list_name;

  while (length > 0) {
    std::string cmd_line;
    if (!GetLine(data, length, &cmd_line))
      return false;  // Error: bad list format!

    std::vector<std::string> cmd_parts;
    SplitString(cmd_line, ':', &cmd_parts);
    if (cmd_parts.empty())
      return false;
    const std::string& command = cmd_parts[0];
    if (cmd_parts.size() != 2 && command[0] != 'u')
      return false;

    const int consumed = static_cast<int>(cmd_line.size()) + 1;
    data += consumed;
    length -= consumed;
    if (length < 0)
      return false;  // Parsing error.

    // Differentiate on the first character of the command (which is usually
    // only one character, with the exception of the 'ad' and 'sd' commands).
    switch (command[0]) {
      case 'a':
      case 's': {
        // Must be either an 'ad' (add-del) or 'sd' (sub-del) chunk. We must
        // have also parsed the list name before getting here, or the add-del
        // or sub-del will have no context.
        if (command.size() != 2 || command[1] != 'd' || list_name.empty())
          return false;
        SBChunkDelete chunk_delete;
        chunk_delete.is_sub_del = command[0] == 's';
        StringToRanges(cmd_parts[1], &chunk_delete.chunk_del);
        chunk_delete.list_name = list_name;
        deletes->push_back(chunk_delete);
        break;
      }

      case 'e':
        if (cmd_parts[1] != "pleaserekey")
          return false;
        *re_key = true;
        break;

      case 'i':
        // The line providing the name of the list (i.e. 'goog-phish-shavar').
        list_name = cmd_parts[1];
        break;

      case 'm':
        // Verify that the MAC of the remainer of this chunk is what we expect.
        if (!key.empty() &&
            !safe_browsing_util::VerifyMAC(key, cmd_parts[1], data, length))
          return false;
        break;

      case 'n':
        // The line providing the next earliest time (in seconds) to re-query.
        *next_update_sec = atoi(cmd_parts[1].c_str());
        break;

      case 'u': {
        // The redirect command is of the form: u:<url>,<mac> where <url> can
        // contain multiple colons, commas or any valid URL characters. We scan
        // backwards in the string looking for the first ',' we encounter and
        // assume that everything before that is the URL and everything after
        // is the MAC (if the MAC was requested).
        std::string mac;
        std::string redirect_url(cmd_line, 2);  // Skip the initial "u:".
        if (!key.empty()) {
          std::string::size_type mac_pos = redirect_url.rfind(',');
          if (mac_pos == std::string::npos)
            return false;
          mac = redirect_url.substr(mac_pos + 1);
          redirect_url = redirect_url.substr(0, mac_pos);
        }

        ChunkUrl chunk_url;
        chunk_url.url = redirect_url;
        chunk_url.list_name = list_name;
        if (!key.empty())
          chunk_url.mac = mac;
        chunk_urls->push_back(chunk_url);
        break;
      }

      case 'r':
        if (cmd_parts[1] != "pleasereset")
          return false;
        *reset = true;
        break;

      default:
        // According to the spec, we ignore commands we don't understand.
        break;
    }
  }

  return true;
}

bool SafeBrowsingProtocolParser::ParseChunk(const char* data,
                                            int length,
                                            const std::string& key,
                                            const std::string& mac,
                                            bool* re_key,
                                            std::deque<SBChunk>* chunks) {
  int remaining = length;
  const char* chunk_data = data;

  if (!key.empty() &&
      !safe_browsing_util::VerifyMAC(key, mac, data, length)) {
    return false;
  }

  while (remaining > 0) {
    std::string cmd_line;
    if (!GetLine(chunk_data, length, &cmd_line))
      return false;  // Error: bad chunk format!

    const int line_len = static_cast<int>(cmd_line.length()) + 1;
    std::vector<std::string> cmd_parts;
    SplitString(cmd_line, ':', &cmd_parts);

    // Handle a possible re-key command.
    if (cmd_parts.size() != 4) {
      if (cmd_parts.size() == 2 &&
          cmd_parts[0] == "e" &&
          cmd_parts[1] == "pleaserekey") {
        *re_key = true;
        chunk_data += line_len;
        remaining -= line_len;
        continue;
      }
      return false;
    }

    // Process the chunk data.
    const int chunk_number = atoi(cmd_parts[1].c_str());
    const int hash_len = atoi(cmd_parts[2].c_str());
    if (hash_len != sizeof(SBPrefix) && hash_len != sizeof(SBFullHash)) {
      SB_DLOG(INFO) << "ParseChunk got unknown hashlen " << hash_len;
      return false;
    }

    const int chunk_len = atoi(cmd_parts[3].c_str());
    chunk_data += line_len;
    remaining -= line_len;

    chunks->push_back(SBChunk());
    chunks->back().chunk_number = chunk_number;

    if (cmd_parts[0] == "a") {
      chunks->back().is_add = true;
      if (!ParseAddChunk(chunk_data, chunk_len, hash_len, &chunks->back().hosts))
        return false;  // Parse error.
    } else if (cmd_parts[0] == "s") {
      chunks->back().is_add = false;
      if (!ParseSubChunk(chunk_data, chunk_len, hash_len, &chunks->back().hosts))
        return false;  // Parse error.
    } else {
      NOTREACHED();
      return false;
    }

    chunk_data += chunk_len;
    remaining -= chunk_len;
    if (remaining < 0)
      return false;  // Parse error.
  }

  DCHECK(remaining == 0);

  return true;
}

bool SafeBrowsingProtocolParser::ParseAddChunk(
    const char* data, int data_len, int hash_len,
    std::deque<SBChunkHost>* hosts) {

  int remaining = data_len;
  const char* chunk_data = data;
  const int min_size = sizeof(SBPrefix) + 1;

  while (remaining >= min_size) {
    SBPrefix host;
    int prefix_count;
    ReadHostAndPrefixCount(&chunk_data, &remaining, &host, &prefix_count);
    SBEntry::Type type = hash_len == sizeof(SBPrefix) ?
        SBEntry::ADD_PREFIX : SBEntry::ADD_FULL_HASH;
    SBEntry* entry;
    int index_start = 0;

    // If a host has more than 255 prefixes, then subsequent entries are used.
    // Check if this is the case, and if so put them in one SBEntry since the
    // database code assumes that all prefixes from the same host and chunk are
    // in one SBEntry.
    if (!hosts->empty() && hosts->back().host == host &&
        hosts->back().entry->HashLen() == hash_len) {
      // Reuse the SBChunkHost, but need to create a new SBEntry since we have
      // more prefixes.
      index_start = hosts->back().entry->prefix_count();
      entry = hosts->back().entry->Enlarge(prefix_count);
      hosts->back().entry = entry;
    } else {
      entry = SBEntry::Create(type, prefix_count);
      SBChunkHost chunk_host;
      chunk_host.host = host;
      chunk_host.entry = entry;
      hosts->push_back(chunk_host);
    }

    if (!ReadPrefixes(&chunk_data, &remaining, entry, prefix_count, index_start))
      return false;
  }

  return remaining == 0;
}

bool SafeBrowsingProtocolParser::ParseSubChunk(
    const char* data, int data_len, int hash_len,
    std::deque<SBChunkHost>* hosts) {

  int remaining = data_len;
  const char* chunk_data = data;
  const int min_size = 2 * sizeof(SBPrefix) + 1;

  while (remaining >= min_size) {
    SBPrefix host;
    int prefix_count;
    ReadHostAndPrefixCount(&chunk_data, &remaining, &host, &prefix_count);
    SBEntry::Type type = hash_len == sizeof(SBPrefix) ?
        SBEntry::SUB_PREFIX : SBEntry::SUB_FULL_HASH;
    SBEntry* entry = SBEntry::Create(type, prefix_count);

    SBChunkHost chunk_host;
    chunk_host.host = host;
    chunk_host.entry = entry;
    hosts->push_back(chunk_host);

    if (prefix_count == 0) {
      // There is only an add chunk number (no prefixes).
      entry->set_chunk_id(ReadChunkId(&chunk_data, &remaining));
      continue;
    }

    if (!ReadPrefixes(&chunk_data, &remaining, entry, prefix_count, 0))
      return false;
  }

  return remaining == 0;
}


void SafeBrowsingProtocolParser::ReadHostAndPrefixCount(
    const char** data, int* remaining, SBPrefix* host, int* count) {
  // Next 4 bytes are the host prefix.
  memcpy(host, *data, sizeof(SBPrefix));
  *data += sizeof(SBPrefix);
  *remaining -= sizeof(SBPrefix);

  // Next 1 byte is the prefix count (could be zero, but never negative).
  *count = static_cast<unsigned char>(**data);
  *data += 1;
  *remaining -= 1;
}

int SafeBrowsingProtocolParser::ReadChunkId(
    const char** data, int* remaining) {
  int chunk_number;
  memcpy(&chunk_number, *data, sizeof(chunk_number));
  *data += sizeof(chunk_number);
  *remaining -= sizeof(chunk_number);
  return htonl(chunk_number);
}

bool SafeBrowsingProtocolParser::ReadPrefixes(
    const char** data, int* remaining, SBEntry* entry, int count,
    int index_start) {
  int hash_len = entry->HashLen();
  for (int i = 0; i < count; ++i) {
    if (entry->IsSub()) {
      entry->SetChunkIdAtPrefix(index_start + i, ReadChunkId(data, remaining));
      if (*remaining <= 0)
        return false;
    }

    if (hash_len == sizeof(SBPrefix)) {
      entry->SetPrefixAt(index_start + i,
                         *reinterpret_cast<const SBPrefix*>(*data));
    } else {
      entry->SetFullHashAt(index_start + i,
                           *reinterpret_cast<const SBFullHash*>(*data));
    }
    *data += hash_len;
    *remaining -= hash_len;
    if (*remaining < 0)
      return false;
  }

  return true;
}

bool SafeBrowsingProtocolParser::ParseNewKey(const char* chunk_data,
                                             int chunk_length,
                                             std::string* client_key,
                                             std::string* wrapped_key) {
  DCHECK(client_key && wrapped_key);
  client_key->clear();
  wrapped_key->clear();

  const char* data = chunk_data;
  int remaining = chunk_length;

  while (remaining > 0) {
    std::string line;
    if (!GetLine(data, remaining, &line))
      return false;

    std::vector<std::string> cmd_parts;
    SplitString(line, ':', &cmd_parts);
    if (cmd_parts.size() != 3)
      return false;

    if (static_cast<int>(cmd_parts[2].size()) != atoi(cmd_parts[1].c_str()))
      return false;

    if (cmd_parts[0] == "clientkey") {
      client_key->assign(cmd_parts[2]);
    } else if (cmd_parts[0] == "wrappedkey") {
      wrapped_key->assign(cmd_parts[2]);
    } else {
      return false;
    }

    data += line.size() + 1;
    remaining -= static_cast<int>(line.size()) + 1;
  }

  if (client_key->empty() || wrapped_key->empty())
    return false;

  return true;
}
