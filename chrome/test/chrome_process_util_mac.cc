// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chrome_process_util.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/process_util.h"
#include "base/string_util.h"

// Yes, this is impossibly lame. This horrible hack is Good Enough, though,
// because it's not used in production code, but just for testing.
//
// We could make this better by creating a system through which all instances of
// Chromium can communicate. ProcessSingleton does that for Windows and Linux,
// but the Mac doesn't implement it as its system services handle it. It's not
// worth implementing just for this.
//
// We could do something similar to what Linux does, and use |fuser| to find a
// file that the app ordinarily opens within the data dir. However, fuser is
// broken on Leopard, and does not detect files that lsof shows are open.
//
// What's going on here is that during ui_tests, the Chromium application is
// launched using the --user-data-dir command line option. By examining the
// output of ps, we can find the appropriately-launched Chromium process. Note
// that this _does_ work for paths with spaces. The command line that ps gives
// is just the argv separated with spaces. There's no escaping spaces as a shell
// would do, so a straight string comparison will work just fine.
//
// TODO(avi):see if there is a better way

base::ProcessId ChromeBrowserProcessId(const FilePath& data_dir) {
  std::vector<std::string> argv;
  argv.push_back("ps");
  argv.push_back("-xw");

  std::string ps_output;
  if (!base::GetAppOutput(CommandLine(argv), &ps_output))
    return -1;

  std::vector<std::string> lines;
  SplitString(ps_output, '\n', &lines);

  for (size_t i=0; i<lines.size(); ++i) {
    const std::string& line = lines[i];
    if (line.find(data_dir.value()) != std::string::npos &&
        line.find("type=renderer") == std::string::npos) {
      int pid = StringToInt(line);  // pid is at beginning of line
      return pid==0 ? -1 : pid;
    }
  }

  return -1;
}

MacChromeProcessInfoList GetRunningMacProcessInfo(
    const ChromeProcessList &process_list) {
  MacChromeProcessInfoList result;

  // Build up the ps command line
  std::vector<std::string> cmdline;
  cmdline.push_back("ps");
  cmdline.push_back("-o");
  cmdline.push_back("pid=,rsz=,vsz=");  // fields we need, no headings
  ChromeProcessList::const_iterator process_iter;
  for (process_iter = process_list.begin();
       process_iter != process_list.end();
       ++process_iter) {
    cmdline.push_back("-p");
    cmdline.push_back(StringPrintf("%d", *process_iter));
  }

  // Invoke it
  std::string ps_output;
  if (!base::GetAppOutput(CommandLine(cmdline), &ps_output))
    return result;  // All the pids might have exited

  // Process the results
  std::vector<std::string> ps_output_lines;
  SplitString(ps_output, '\n', &ps_output_lines);
  std::vector<std::string>::const_iterator line_iter;
  for (line_iter = ps_output_lines.begin();
       line_iter != ps_output_lines.end();
       ++line_iter) {
    std::string line(CollapseWhitespaceASCII(*line_iter, false));
    std::vector<std::string> values;
    SplitString(line, ' ', &values);
    if (values.size() == 3) {
      MacChromeProcessInfo proc_info;
      proc_info.pid = StringToInt(values[0]);
      proc_info.rsz_in_kb = StringToInt(values[1]);
      proc_info.vsz_in_kb = StringToInt(values[2]);
      if (proc_info.pid && proc_info.rsz_in_kb && proc_info.vsz_in_kb)
        result.push_back(proc_info);
    }
  }

  return result;
}
