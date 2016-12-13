// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/renderer/renderer_sandbox_support_linux.h"

#include "base/global_descriptors_posix.h"
#include "base/pickle.h"
#include "base/unix_domain_socket_posix.h"
#include "chrome/common/chrome_descriptors.h"
#include "chrome/common/sandbox_methods_linux.h"

namespace renderer_sandbox_support {

std::string getFontFamilyForCharacters(const uint16_t* utf16, size_t num_utf16) {
  Pickle request;
  request.WriteInt(LinuxSandbox::METHOD_GET_FONT_FAMILY_FOR_CHARS);
  request.WriteInt(num_utf16);
  for (size_t i = 0; i < num_utf16; ++i)
    request.WriteUInt32(utf16[i]);

  uint8_t buf[512];
  const int sandbox_fd =
    kSandboxIPCChannel + base::GlobalDescriptors::kBaseDescriptor;
  const ssize_t n = base::SendRecvMsg(sandbox_fd, buf, sizeof(buf), NULL,
                                      request);

  std::string family_name;
  if (n != -1) {
    Pickle reply(reinterpret_cast<char*>(buf), n);
    void* pickle_iter = NULL;
    reply.ReadString(&pickle_iter, &family_name);
  }

  return family_name;
}

}  // namespace render_sandbox_support
