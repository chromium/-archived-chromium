// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include "base/eintr_wrapper.h"
#include "base/rand_util.h"
#include "base/file_version_info_linux.h"
#include "breakpad/linux/directory_reader.h"
#include "breakpad/linux/exception_handler.h"
#include "breakpad/linux/linux_libc_support.h"
#include "breakpad/linux/linux_syscall_support.h"
#include "breakpad/linux/memory.h"

static const char kUploadURL[] =
    "https://clients2.google.com/cr/report";

// Writes the value |v| as 16 hex characters to the memory pointed at by
// |output|.
static void write_uint64_hex(char* output, uint64_t v) {
  static const char hextable[] = "0123456789abcdef";

  for (int i = 15; i >= 0; --i) {
    output[i] = hextable[v & 15];
    v >>= 4;
  }
}

pid_t UploadCrashDump(const char* filename, const char* crash_url,
                      unsigned crash_url_length) {
  // WARNING: this code runs in a compromised context. It may not call into
  // libc nor allocate memory normally.

  const int dumpfd = sys_open(filename, O_RDONLY, 0);
  if (dumpfd < 0) {
    static const char msg[] = "Cannot upload crash dump: failed to open\n";
    sys_write(2, msg, sizeof(msg));
    return -1;
  }
  struct kernel_stat st;
  if (sys_fstat(dumpfd, &st) != 0) {
    static const char msg[] = "Cannot upload crash dump: stat failed\n";
    sys_write(2, msg, sizeof(msg));
    sys_close(dumpfd);
    return -1;
  }

  google_breakpad::PageAllocator allocator;

  uint8_t* dump_data = reinterpret_cast<uint8_t*>(allocator.Alloc(st.st_size));
  if (!dump_data) {
    static const char msg[] = "Cannot upload crash dump: cannot alloc\n";
    sys_write(2, msg, sizeof(msg));
    sys_close(dumpfd);
    return -1;
  }

  sys_read(dumpfd, dump_data, st.st_size);
  sys_close(dumpfd);

  // We need to build a MIME block for uploading to the server. Since we are
  // going to fork and run wget, it needs to be written to a temp file.

  const int ufd = sys_open("/dev/urandom", O_RDONLY, 0);
  if (ufd < 0) {
    static const char msg[] = "Cannot upload crash dump because /dev/urandom"
                              " is missing\n";
    sys_write(2, msg, sizeof(msg) - 1);
    return -1;
  }

  static const char temp_file_template[] =
      "/tmp/chromium-upload-XXXXXXXXXXXXXXXX";
  char buf[sizeof(temp_file_template)];
  memcpy(buf, temp_file_template, sizeof(temp_file_template));

  int fd = -1;
  for (unsigned i = 0; i < 10; ++i) {
    uint64_t t;
    read(ufd, &t, sizeof(t));
    write_uint64_hex(buf + sizeof(buf) - (16 + 1), t);

    fd = sys_open(buf, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd >= 0)
      break;
  }

  if (fd == -1) {
    static const char msg[] = "Failed to create temporary file in /tmp: cannot "
                              "upload crash dump\n";
    sys_write(2, msg, sizeof(msg) - 1);
    sys_close(ufd);
    return -1;
  }

  // The MIME boundary is 28 hypens, followed by a 64-bit nonce and a NUL.
  char mime_boundary[28 + 16 + 1];
  my_memset(mime_boundary, '-', 28);
  uint64_t boundary_rand;
  sys_read(ufd, &boundary_rand, sizeof(boundary_rand));
  write_uint64_hex(mime_boundary + 28, boundary_rand);
  mime_boundary[28 + 16] = 0;
  sys_close(ufd);

  // The define for the product version is a wide string, so we need to
  // downconvert it.
  static const wchar_t version[] = PRODUCT_VERSION;
  static const unsigned version_len = sizeof(version) / sizeof(wchar_t);
  char version_msg[version_len];
  for (unsigned i = 0; i < version_len; ++i)
    version_msg[i] = static_cast<char>(version[i]);

  // The MIME block looks like this:
  //   BOUNDARY \r\n (0, 1)
  //   Content-Disposition: form-data; name="prod" \r\n \r\n (2..6)
  //   Chrome_Linux \r\n (7, 8)
  //   BOUNDARY \r\n (9, 10)
  //   Content-Disposition: form-data; name="ver" \r\n \r\n (11..15)
  //   1.2.3.4 \r\n (16, 17)
  //   BOUNDARY \r\n (18, 19)
  //
  //   zero or more:
  //   Content-Disposition: form-data; name="url-chunk-1" \r\n \r\n (0..5)
  //   abcdef \r\n (6, 7)
  //   BOUNDARY \r\n (8, 9)
  //
  //   Content-Disposition: form-data; name="dump"; filename="dump" \r\n (0,1,2)
  //   Content-Type: application/octet-stream \r\n \r\n (3,4,5)
  //   <dump contents> (6)
  //   \r\n BOUNDARY -- \r\n (7,8,9,10)

  static const char rn[] = {'\r', '\n'};
  static const char form_data_msg[] = "Content-Disposition: form-data; name=\"";
  static const char prod_msg[] = "prod";
  static const char quote_msg[] = {'"'};
  static const char chrome_linux_msg[] = "Chrome_Linux";
  static const char ver_msg[] = "ver";
  static const char dashdash_msg[] = {'-', '-'};
  static const char dump_msg[] = "upload_file_minidump\"; filename=\"dump\"";
  static const char content_type_msg[] =
      "Content-Type: application/octet-stream";
  static const char url_chunk_msg[] = "url-chunk-";

  struct kernel_iovec iov[20];
  iov[0].iov_base = mime_boundary;
  iov[0].iov_len = sizeof(mime_boundary) - 1;
  iov[1].iov_base = const_cast<char*>(rn);
  iov[1].iov_len = sizeof(rn);

  iov[2].iov_base = const_cast<char*>(form_data_msg);
  iov[2].iov_len = sizeof(form_data_msg) - 1;
  iov[3].iov_base = const_cast<char*>(prod_msg);
  iov[3].iov_len = sizeof(prod_msg) - 1;
  iov[4].iov_base = const_cast<char*>(quote_msg);
  iov[4].iov_len = sizeof(quote_msg);
  iov[5].iov_base = const_cast<char*>(rn);
  iov[5].iov_len = sizeof(rn);
  iov[6].iov_base = const_cast<char*>(rn);
  iov[6].iov_len = sizeof(rn);

  iov[7].iov_base = const_cast<char*>(chrome_linux_msg);
  iov[7].iov_len = sizeof(chrome_linux_msg) - 1;
  iov[8].iov_base = const_cast<char*>(rn);
  iov[8].iov_len = sizeof(rn);

  iov[9].iov_base = mime_boundary;
  iov[9].iov_len = sizeof(mime_boundary) - 1;
  iov[10].iov_base = const_cast<char*>(rn);
  iov[10].iov_len = sizeof(rn);

  iov[11].iov_base = const_cast<char*>(form_data_msg);
  iov[11].iov_len = sizeof(form_data_msg) - 1;
  iov[12].iov_base = const_cast<char*>(ver_msg);
  iov[12].iov_len = sizeof(ver_msg) - 1;
  iov[13].iov_base = const_cast<char*>(quote_msg);
  iov[13].iov_len = sizeof(quote_msg);
  iov[14].iov_base = const_cast<char*>(rn);
  iov[14].iov_len = sizeof(rn);
  iov[15].iov_base = const_cast<char*>(rn);
  iov[15].iov_len = sizeof(rn);

  iov[16].iov_base = const_cast<char*>(version_msg);
  iov[16].iov_len = sizeof(version_msg) - 1;
  iov[17].iov_base = const_cast<char*>(rn);
  iov[17].iov_len = sizeof(rn);

  iov[18].iov_base = mime_boundary;
  iov[18].iov_len = sizeof(mime_boundary) - 1;
  iov[19].iov_base = const_cast<char*>(rn);
  iov[19].iov_len = sizeof(rn);

  sys_writev(fd, iov, 20);

  if (crash_url_length) {
    unsigned i = 0, done = 0;
    static const unsigned kMaxCrashChunkSize = 64;

    while (crash_url_length) {
      char num[16];
      const unsigned num_len = my_int_len(++i);
      my_itos(num, i, num_len);

      iov[0].iov_base = const_cast<char*>(form_data_msg);
      iov[0].iov_len = sizeof(form_data_msg) - 1;
      iov[1].iov_base = const_cast<char*>(url_chunk_msg);
      iov[1].iov_len = sizeof(url_chunk_msg) - 1;
      iov[2].iov_base = num;
      iov[2].iov_len = num_len;
      iov[3].iov_base = const_cast<char*>(quote_msg);
      iov[3].iov_len = sizeof(quote_msg);
      iov[4].iov_base = const_cast<char*>(rn);
      iov[4].iov_len = sizeof(rn);
      iov[5].iov_base = const_cast<char*>(rn);
      iov[5].iov_len = sizeof(rn);

      const unsigned len = crash_url_length > kMaxCrashChunkSize ?
                           kMaxCrashChunkSize : crash_url_length;
      iov[6].iov_base = const_cast<char*>(crash_url + done);
      iov[6].iov_len = len;
      iov[7].iov_base = const_cast<char*>(rn);
      iov[7].iov_len = sizeof(rn);
      iov[8].iov_base = mime_boundary;
      iov[8].iov_len = sizeof(mime_boundary) - 1;
      iov[9].iov_base = const_cast<char*>(rn);
      iov[9].iov_len = sizeof(rn);

      sys_writev(fd, iov, 10);

      done += len;
      crash_url_length -= len;
    }
  }

  iov[0].iov_base = const_cast<char*>(form_data_msg);
  iov[0].iov_len = sizeof(form_data_msg) - 1;
  iov[1].iov_base = const_cast<char*>(dump_msg);
  iov[1].iov_len = sizeof(dump_msg) - 1;
  iov[2].iov_base = const_cast<char*>(rn);
  iov[2].iov_len = sizeof(rn);

  iov[3].iov_base = const_cast<char*>(content_type_msg);
  iov[3].iov_len = sizeof(content_type_msg) - 1;
  iov[4].iov_base = const_cast<char*>(rn);
  iov[4].iov_len = sizeof(rn);
  iov[5].iov_base = const_cast<char*>(rn);
  iov[5].iov_len = sizeof(rn);

  iov[6].iov_base = dump_data;
  iov[6].iov_len = st.st_size;

  iov[7].iov_base = const_cast<char*>(rn);
  iov[7].iov_len = sizeof(rn);
  iov[8].iov_base = mime_boundary;
  iov[8].iov_len = sizeof(mime_boundary) - 1;
  iov[9].iov_base = const_cast<char*>(dashdash_msg);
  iov[9].iov_len = sizeof(dashdash_msg);
  iov[10].iov_base = const_cast<char*>(rn);
  iov[10].iov_len = sizeof(rn);

  sys_writev(fd, iov, 11);

  sys_close(fd);

  // The --header argument to wget looks like:
  //   --header=Content-Type: multipart/form-data; boundary=XYZ
  // where the boundary has two fewer leading '-' chars
  static const char header_msg[] =
      "--header=Content-Type: multipart/form-data; boundary=";
  char* const header = reinterpret_cast<char*>(allocator.Alloc(
      sizeof(header_msg) - 1 + sizeof(mime_boundary) - 2));
  memcpy(header, header_msg, sizeof(header_msg) - 1);
  memcpy(header + sizeof(header_msg) - 1, mime_boundary + 2,
         sizeof(mime_boundary) - 2);
  // We grab the NUL byte from the end of |mime_boundary|.

  // The --post-file argument to wget looks like:
  //   --post-file=/tmp/...
  static const char post_file_msg[] = "--post-file=";
  char* const post_file = reinterpret_cast<char*>(allocator.Alloc(
       sizeof(post_file_msg) - 1 + sizeof(buf)));
  memcpy(post_file, post_file_msg, sizeof(post_file_msg) - 1);
  memcpy(post_file + sizeof(post_file_msg) - 1, buf, sizeof(buf));

  const pid_t child = sys_fork();
  if (!child) {
    // This code is called both when a browser is crashing (in which case,
    // nothing really matters any more) and when a renderer crashes, in which
    // case we need to continue.
    //
    // Since we are a multithreaded app, if we were just to fork(), we might
    // grab file descriptors which have just been created in another thread and
    // hold them open for too long.
    //
    // Thus, we have to loop and try and close everything.
    const int fd = sys_open("/proc/self/fd", O_DIRECTORY | O_RDONLY, 0);
    if (fd < 0) {
      for (unsigned i = 3; i < 8192; ++i)
        sys_close(i);
    } else {
      google_breakpad::DirectoryReader reader(fd);
      const char* name;
      while (reader.GetNextEntry(&name)) {
        int i;
        if (my_strtoui(&i, name) && i > 2 && i != fd)
          sys_close(fd);
        reader.PopEntry();
      }

      sys_close(fd);
    }

    sys_setsid();

    // Leave one end of a pipe in the wget process and watch for it getting
    // closed by the wget process exiting.
    int fds[2];
    sys_pipe(fds);

    const pid_t child = sys_fork();
    if (child) {
      sys_close(fds[1]);
      char buf[32];
      HANDLE_EINTR(read(fds[0], buf, sizeof(buf) - 1));
      buf[sizeof(buf) - 1] = 0;
      static const char msg[] = "\nCrash dump id: ";
      sys_write(2, msg, sizeof(msg) - 1);
      sys_write(2, buf, my_strlen(buf));
      sys_write(2, "\n", 1);
      sys_unlink(filename);
      sys_unlink(buf);
      sys__exit(0);
    }

    sys_close(fds[0]);
    sys_dup2(fds[1], 3);
    static const char* const kWgetBinary = "/usr/bin/wget";
    const char* args[] = {
      kWgetBinary,
      header,
      post_file,
      kUploadURL,
      "-O",  // output reply to fd 3
      "/dev/fd/3",
      NULL,
    };

    execv("/usr/bin/wget", const_cast<char**>(args));
    static const char msg[] = "Cannot update crash dump: cannot exec "
                              "/usr/bin/wget\n";
    sys_write(2, msg, sizeof(msg) - 1);
    sys__exit(1);
  }

  return child;
}

static bool CrashDone(const char* dump_path,
                      const char* minidump_id,
                      void* context,
                      bool succeeded) {
  // WARNING: this code runs in a compromised context. It may not call into
  // libc nor allocate memory normally.
  if (!succeeded)
    return false;

  google_breakpad::PageAllocator allocator;
  const unsigned dump_path_len = my_strlen(dump_path);
  const unsigned minidump_id_len = my_strlen(minidump_id);
  char *const path = reinterpret_cast<char*>(allocator.Alloc(
      dump_path_len + 1 /* '/' */ + minidump_id_len + 
      4 /* ".dmp" */ + 1 /* NUL */));
  memcpy(path, dump_path, dump_path_len);
  path[dump_path_len] = '/';
  memcpy(path + dump_path_len + 1, minidump_id, minidump_id_len);
  memcpy(path + dump_path_len + 1 + minidump_id_len, ".dmp", 4);
  path[dump_path_len + 1 + minidump_id_len + 4] = 0;

  UploadCrashDump(path, NULL, 0);

  return true;
}

void EnableCrashDumping() {
  // We leak this object.

  new google_breakpad::ExceptionHandler("/tmp", NULL, CrashDone, NULL,
                                        true /* install handlers */);
}
