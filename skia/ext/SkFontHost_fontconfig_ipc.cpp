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

FontConfigIPC::FontConfigIPC(int fd)
    : fd_(fd) {
}

FontConfigIPC::~FontConfigIPC() {
    close(fd_);
}

static ssize_t SyncIPC(int fd, const struct iovec* output, unsigned output_len,
                       int* result_fd, const struct iovec* input,
                       unsigned input_len) {
    char cmsg_buf[CMSG_SPACE(sizeof(int))];

    int fds[2];
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, fds) == -1)
        return false;

    struct msghdr msg = {0};
    msg.msg_iov = const_cast<struct iovec*>(input);
    msg.msg_iovlen = input_len;
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    *(int *) CMSG_DATA(cmsg) = fds[1];
    msg.msg_controllen = cmsg->cmsg_len;

    ssize_t r;
    do {
      r = sendmsg(fd, &msg, 0);
    } while (r == -1 && errno == EINTR);
    close(fds[1]);

    if (r == -1) {
        close(fds[0]);
        return -1;
    }

    memset(&msg, 0, sizeof(msg));

    msg.msg_iov = const_cast<struct iovec*>(output);
    msg.msg_iovlen = output_len;
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    do {
        r = recvmsg(fds[0], &msg, 0);
    } while (r == -1 && errno == EINTR);
    close(fds[0]);

    if (r == -1 || msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC))
        return -1;

    cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg && cmsg->cmsg_level == SOL_SOCKET &&
        cmsg->cmsg_type == SCM_RIGHTS) {
        const int fd = *(int*) CMSG_DATA(cmsg);
        if (!result_fd)
            close(fd);
        *result_fd = fd;
    }

    return r;
}

bool FontConfigIPC::Match(std::string* result_family,
                          unsigned* result_fileid,
                          bool fileid_valid, unsigned fileid,
                          const std::string& family, int is_bold,
                          int is_italic) {
    MatchRequest request;
    request.method = METHOD_MATCH;
    request.fileid_valid = fileid_valid;
    request.fileid = fileid;
    request.is_bold = is_bold;
    request.is_italic = is_italic;

    if (family.size() > 255)
        return false;
    request.family_len = family.size();

    struct iovec out_iov[2];
    out_iov[0].iov_base = &request;
    out_iov[0].iov_len = sizeof(request);
    if (family.size()) {
        out_iov[1].iov_base = const_cast<char*>(family.data());
        out_iov[1].iov_len = family.size();
    }

    MatchReply reply;
    char filename_buf[PATH_MAX];

    struct iovec in_iov[2];
    in_iov[0].iov_base = &reply;
    in_iov[0].iov_len = sizeof(reply);
    in_iov[1].iov_base = filename_buf;
    in_iov[1].iov_len = sizeof(filename_buf);

    const ssize_t r = SyncIPC(fd_, in_iov, 2, NULL, out_iov,
                              family.size() ? 2 : 1);

    if (r == -1 || r < sizeof(reply))
        return false;

    if (reply.result == 0)
        return false;

    *result_fileid = reply.result_fileid;
    if (r != sizeof(reply) + reply.filename_len)
        return false;
    if (result_family)
      *result_family = std::string(filename_buf, reply.filename_len);

    return true;
}

int FontConfigIPC::Open(unsigned fileid) {
    OpenRequest request;
    OpenReply reply;
    const struct iovec out_iov = {&request, sizeof(request)};
    const struct iovec in_iov = {&reply, sizeof(reply)};
    int result_fd = -1;

    request.method = METHOD_OPEN;
    request.fileid = fileid;
    const ssize_t r = SyncIPC(fd_, &in_iov, 1, &result_fd, &out_iov, 1);
    if (r == -1 || r != sizeof(reply) || reply.result == 0) {
        if (result_fd >= 0)
            close(result_fd);
        return -1;
    }

    return result_fd;
}
