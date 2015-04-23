// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/eintr_wrapper.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"

namespace {

enum ParsingState {
  KEY_NAME,
  KEY_VALUE
};

// Reads /proc/<pid>/stat and populates |proc_stats| with the values split by
// spaces.
void GetProcStats(pid_t pid, std::vector<std::string>* proc_stats) {
  FilePath stat_file("/proc");
  stat_file = stat_file.Append(IntToString(pid));
  stat_file = stat_file.Append("stat");
  std::string mem_stats;
  if (!file_util::ReadFileToString(stat_file, &mem_stats))
    return;
  SplitString(mem_stats, ' ', proc_stats);
}

}  // namespace

namespace base {

ProcessId GetParentProcessId(ProcessHandle process) {
  FilePath stat_file("/proc");
  stat_file = stat_file.Append(IntToString(process));
  stat_file = stat_file.Append("status");
  std::string status;
  if (!file_util::ReadFileToString(stat_file, &status))
    return -1;

  StringTokenizer tokenizer(status, ":\n");
  ParsingState state = KEY_NAME;
  std::string last_key_name;
  while (tokenizer.GetNext()) {
    switch (state) {
      case KEY_NAME:
        last_key_name = tokenizer.token();
        state = KEY_VALUE;
        break;
      case KEY_VALUE:
        DCHECK(!last_key_name.empty());
        if (last_key_name == "PPid") {
          pid_t ppid = StringToInt(tokenizer.token());
          return ppid;
        }
        state = KEY_NAME;
        break;
    }
  }
  NOTREACHED();
  return -1;
}

FilePath GetProcessExecutablePath(ProcessHandle process) {
  FilePath stat_file("/proc");
  stat_file = stat_file.Append(IntToString(process));
  stat_file = stat_file.Append("exe");
  char exename[2048];
  ssize_t len = readlink(stat_file.value().c_str(), exename, sizeof(exename));
  if (len < 1) {
    // No such process.  Happens frequently in e.g. TerminateAllChromeProcesses
    return FilePath();
  }
  return FilePath(std::string(exename, len));
}

bool LaunchApp(const std::vector<std::string>& argv,
               const file_handle_mapping_vector& fds_to_remap,
               bool wait, ProcessHandle* process_handle) {
  pid_t pid = fork();
  if (pid < 0)
    return false;

  if (pid == 0) {
    InjectiveMultimap fd_shuffle;
    for (file_handle_mapping_vector::const_iterator
        it = fds_to_remap.begin(); it != fds_to_remap.end(); ++it) {
      fd_shuffle.push_back(InjectionArc(it->first, it->second, false));
    }

    if (!ShuffleFileDescriptors(fd_shuffle))
      exit(127);

    // If we are using the SUID sandbox, it sets a magic environment variable
    // ("SBX_D"), so we remove that variable from the environment here on the
    // off chance that it's already set.
    unsetenv("SBX_D");

    CloseSuperfluousFds(fd_shuffle);

    scoped_array<char*> argv_cstr(new char*[argv.size() + 1]);
    for (size_t i = 0; i < argv.size(); i++)
      argv_cstr[i] = const_cast<char*>(argv[i].c_str());
    argv_cstr[argv.size()] = NULL;
    execvp(argv_cstr[0], argv_cstr.get());
    LOG(ERROR) << "LaunchApp: exec failed!, argv_cstr[0] " << argv_cstr[0]
        << ", errno " << errno;
    exit(127);
  } else {
    if (wait)
      HANDLE_EINTR(waitpid(pid, 0, 0));

    if (process_handle)
      *process_handle = pid;
  }

  return true;
}

bool LaunchApp(const CommandLine& cl,
               bool wait, bool start_hidden,
               ProcessHandle* process_handle) {
  file_handle_mapping_vector no_files;
  return LaunchApp(cl.argv(), no_files, wait, process_handle);
}

NamedProcessIterator::NamedProcessIterator(const std::wstring& executable_name,
                                           const ProcessFilter* filter)
    : executable_name_(executable_name), filter_(filter) {
  procfs_dir_ = opendir("/proc");
}

NamedProcessIterator::~NamedProcessIterator() {
  if (procfs_dir_) {
    closedir(procfs_dir_);
    procfs_dir_ = NULL;
  }
}

const ProcessEntry* NamedProcessIterator::NextProcessEntry() {
  bool result = false;
  do {
    result = CheckForNextProcess();
  } while (result && !IncludeEntry());

  if (result)
    return &entry_;

  return NULL;
}

bool NamedProcessIterator::CheckForNextProcess() {
  // TODO(port): skip processes owned by different UID

  dirent* slot = 0;
  const char* openparen;
  const char* closeparen;

  // Arbitrarily guess that there will never be more than 200 non-process
  // files in /proc.  Hardy has 53.
  int skipped = 0;
  const int kSkipLimit = 200;
  while (skipped < kSkipLimit) {
    slot = readdir(procfs_dir_);
    // all done looking through /proc?
    if (!slot)
      return false;

    // If not a process, keep looking for one.
    bool notprocess = false;
    int i;
    for (i = 0; i < NAME_MAX && slot->d_name[i]; ++i) {
       if (!isdigit(slot->d_name[i])) {
         notprocess = true;
         break;
       }
    }
    if (i == NAME_MAX || notprocess) {
      skipped++;
      continue;
    }

    // Read the process's status.
    char buf[NAME_MAX + 12];
    sprintf(buf, "/proc/%s/stat", slot->d_name);
    FILE *fp = fopen(buf, "r");
    if (!fp)
      return false;
    const char* result = fgets(buf, sizeof(buf), fp);
    fclose(fp);
    if (!result)
      return false;

    // Parse the status.  It is formatted like this:
    // %d (%s) %c %d ...
    // pid (name) runstate ppid
    // To avoid being fooled by names containing a closing paren, scan
    // backwards.
    openparen = strchr(buf, '(');
    closeparen = strrchr(buf, ')');
    if (!openparen || !closeparen)
      return false;
    char runstate = closeparen[2];

    // Is the process in 'Zombie' state, i.e. dead but waiting to be reaped?
    // Allowed values: D R S T Z
    if (runstate != 'Z')
      break;

    // Nope, it's a zombie; somebody isn't cleaning up after their children.
    // (e.g. WaitForProcessesToExit doesn't clean up after dead children yet.)
    // There could be a lot of zombies, can't really decrement i here.
  }
  if (skipped >= kSkipLimit) {
    NOTREACHED();
    return false;
  }

  entry_.pid = atoi(slot->d_name);
  entry_.ppid = atoi(closeparen + 3);

  // TODO(port): read pid's commandline's $0, like killall does.  Using the
  // short name between openparen and closeparen won't work for long names!
  int len = closeparen - openparen - 1;
  if (len > NAME_MAX)
    len = NAME_MAX;
  memcpy(entry_.szExeFile, openparen + 1, len);
  entry_.szExeFile[len] = 0;

  return true;
}

bool NamedProcessIterator::IncludeEntry() {
  // TODO(port): make this also work for non-ASCII filenames
  if (WideToASCII(executable_name_) != entry_.szExeFile)
    return false;
  if (!filter_)
    return true;
  return filter_->Includes(entry_.pid, entry_.ppid);
}

// On linux, we return vsize.
size_t ProcessMetrics::GetPagefileUsage() const {
  std::vector<std::string> proc_stats;
  GetProcStats(process_, &proc_stats);
  const size_t kVmSize = 22;
  if (proc_stats.size() > kVmSize)
    return static_cast<size_t>(StringToInt(proc_stats[kVmSize]));
  return 0;
}

size_t ProcessMetrics::GetPeakPagefileUsage() const {
  // http://crbug.com/16251
  return 0;
}

// On linux, we return RSS.
size_t ProcessMetrics::GetWorkingSetSize() const {
  std::vector<std::string> proc_stats;
  GetProcStats(process_, &proc_stats);
  const size_t kVmRss = 23;
  if (proc_stats.size() > kVmRss) {
    size_t num_pages = static_cast<size_t>(StringToInt(proc_stats[kVmRss]));
    return num_pages * getpagesize();
  }
  return 0;
}

size_t ProcessMetrics::GetPeakWorkingSetSize() const {
  // http://crbug.com/16251
  return 0;
}

size_t ProcessMetrics::GetPrivateBytes() const {
  // http://crbug.com/16251
  return 0;
}

bool ProcessMetrics::GetWorkingSetKBytes(WorkingSetKBytes* ws_usage) const {
  // http://crbug.com/16251
  return false;
}

// To have /proc/self/io file you must enable CONFIG_TASK_IO_ACCOUNTING
// in your kernel configuration.
bool ProcessMetrics::GetIOCounters(IoCounters* io_counters) const {
  std::string proc_io_contents;
  FilePath io_file("/proc");
  io_file = io_file.Append(IntToString(process_));
  io_file = io_file.Append("io");
  if (!file_util::ReadFileToString(io_file, &proc_io_contents))
    return false;

  (*io_counters).OtherOperationCount = 0;
  (*io_counters).OtherTransferCount = 0;

  StringTokenizer tokenizer(proc_io_contents, ": \n");
  ParsingState state = KEY_NAME;
  std::string last_key_name;
  while (tokenizer.GetNext()) {
    switch (state) {
      case KEY_NAME:
        last_key_name = tokenizer.token();
        state = KEY_VALUE;
        break;
      case KEY_VALUE:
        DCHECK(!last_key_name.empty());
        if (last_key_name == "syscr") {
          (*io_counters).ReadOperationCount = StringToInt64(tokenizer.token());
        } else if (last_key_name == "syscw") {
          (*io_counters).WriteOperationCount = StringToInt64(tokenizer.token());
        } else if (last_key_name == "rchar") {
          (*io_counters).ReadTransferCount = StringToInt64(tokenizer.token());
        } else if (last_key_name == "wchar") {
          (*io_counters).WriteTransferCount = StringToInt64(tokenizer.token());
        }
        state = KEY_NAME;
        break;
    }
  }
  return true;
}

}  // namespace base
