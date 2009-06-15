/* libs/graphics/ports/SkFontHost_fontconfig_direct.cpp
**
** Copyright 2009, Google Inc.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

// http://code.google.com/p/chromium/wiki/LinuxSandboxIPC

#include "SkFontHost_fontconfig_ipc.h"

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include "base/pickle.h"
#include "base/unix_domain_socket_posix.h"

FontConfigIPC::FontConfigIPC(int fd)
    : fd_(fd) {
}

FontConfigIPC::~FontConfigIPC() {
    close(fd_);
}

static ssize_t SyncIPC(int fd, uint8_t* reply, unsigned reply_len, int* result_fd,
                       const Pickle& request) {
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, fds) == -1)
        return false;

    std::vector<int> fd_vector;
    fd_vector.push_back(fds[1]);
    if (!base::SendMsg(fd, request.data(), request.size(), fd_vector)) {
      close(fds[0]);
      close(fds[1]);
      return -1;
    }
    close(fds[1]);

    fd_vector.clear();
    const ssize_t r = base::RecvMsg(fds[0], reply, reply_len, &fd_vector);
    close(fds[0]);
    if (r == -1)
      return -1;

    if ((fd_vector.size() > 0 && result_fd == NULL) || fd_vector.size() > 1) {
      for (std::vector<int>::const_iterator
           i = fd_vector.begin(); i != fd_vector.end(); ++i) {
        close(*i);
      }

      return -1;
    }

    if (result_fd) {
      if (fd_vector.size() == 0) {
        *result_fd = -1;
      } else {
        *result_fd = fd_vector[0];
      }
    }

    return r;
}

bool FontConfigIPC::Match(std::string* result_family,
                          unsigned* result_fileid,
                          bool fileid_valid, unsigned fileid,
                          const std::string& family, int is_bold,
                          int is_italic) {
    Pickle request;
    request.WriteInt(METHOD_MATCH);
    request.WriteBool(fileid_valid);
    if (fileid_valid)
      request.WriteUInt32(fileid);
    request.WriteBool(is_bold);
    request.WriteBool(is_italic);
    request.WriteString(family);

    uint8_t reply_buf[512];
    const ssize_t r = SyncIPC(fd_, reply_buf, sizeof(reply_buf), NULL, request);
    if (r == -1)
      return false;

    Pickle reply(reinterpret_cast<char*>(reply_buf), r);
    void* iter = NULL;
    bool result;
    if (!reply.ReadBool(&iter, &result))
      return false;
    if (!result)
      return false;

    uint32_t reply_fileid;
    std::string reply_family;
    if (!reply.ReadUInt32(&iter, &reply_fileid) ||
        !reply.ReadString(&iter, &reply_family)) {
      return false;
    }

    *result_fileid = reply_fileid;
    if (result_family)
      *result_family = reply_family;

    return true;
}

int FontConfigIPC::Open(unsigned fileid) {
    Pickle request;
    request.WriteInt(METHOD_OPEN);
    request.WriteUInt32(fileid);

    int result_fd = -1;
    uint8_t reply_buf[256];
    const ssize_t r = SyncIPC(fd_, reply_buf, sizeof(reply_buf), &result_fd, request);

    if (r == -1)
      return -1;

    Pickle reply(reinterpret_cast<char*>(reply_buf), r);
    bool result;
    void* iter = NULL;
    if (!reply.ReadBool(&iter, &result) ||
        !result) {
      if (result_fd)
        close(result_fd);
      return -1;
    }

    return result_fd;
}
