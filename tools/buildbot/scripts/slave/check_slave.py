#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" A tool to check the configuration of a build slave and output the
    errors.
    This script must be run on Windows.
"""

import os
import sys
import subprocess
import win32api
import win32con

# Registry key where the Visual Studio SP1 updater registers itself.
VS_SP1_KEY = ('SOFTWARE\\Microsoft\\Active Setup\\Installed Components\\'
              '{D93F9C7C-AB57-44C8-BAD6-1494674BCAF7}')

# Registry key where the DEP settings are stored.
APP_COMPAT = ('SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\'
              'AppCompatFlags\\NoExecuteState')

# Registry key where the performance settings for the cache are stored.
CACHE_KEY = 'SYSTEM\CurrentControlSet\Control\Session Manager\Memory Management'

# Registry key where the performance settings for the process priority are
# stored.
BACKGROUND = 'SYSTEM\CurrentControlSet\Control\PriorityControl'

# Registry key where the system restore settings are stored.
SYSTEM_RESTORE = 'SOFTWARE\Microsoft\Windows NT\CurrentVersion\SystemRestore'

# Predefined registry root hives.
HKLM = win32con.HKEY_LOCAL_MACHINE
HKCU = win32con.HKEY_CURRENT_USER

MESSAGES = {'VSCHECK'    : ['Visual Studio: Installed.',
                            'Cannot find Visual Studio.'],
            'VSSP1'      : ['Visual Studio SP Check: SP1 Installed.',
                            'Visual Studio Service Pack 1 is not installed.'],
            'GCLIENT'    : ['depot_tools: Installed.',
                            'Cannot find depot_tools.'],
            'SCRIPT'     : ['Scripts: Installed.',
                            'Cannot find the scripts.'],
            'DT_PATH'    : ['Path: depot_tools found.',
                            'Cannot find depot_tools in the path.'],
            'PERF'       : ['Performance settings: set.',
                            'The best performances are not set to Background '
                            'Services and System Cache.'],
            'RUNKEY'     : ['Run key: empty.',
                            'You might want to consider removing these items '
                            'in the run key.'],
            'RESTORE'    : ['System Restore: Disabled.',
                            'System restore is enabled.'],
            'STARTUP'    : ['Startup Folder: Empty.',
                            'You might want to consider removing these items '
                            'in your startup folder.'],
            'OLDBUILD'   : ['Build directories: Ok.',
                            'Some slave folders have old build directories.'],
            'SHORTCUT'   : ['run_slave shortcut: present on the desktop.',
                            'There is no run_slave.bat shortcut on the '
                            'desktop.'],
            'PROCEXP'    : ['Process Explorer: present on the desktop.',
                            'There is no procexp.exe on the desktop.'],
            'PROCMON'    : ['Process Monitor: present on the desktop.',
                            'There is no procmon.exe on the desktop.'],
            'DEFRAG'     : ['Defrag task: correctly scheduled.',
                            'The defrag task is not scheduled or has never '
                            'run.'],
            'DEP'        : ['DEP: Enabled',
                            'DEP is not enabled on the machine'],
           }

def QueryKeyValue(key, subkey, value):
  """ Open a key and return a value
  """
  query = win32api.RegOpenKeyEx(key, subkey)

  (value, type) = win32api.RegQueryValueEx(query, value)
  return value


class SlaveChecker:
  """ Check the configuration of a slave and output the errors to stdout.
      A method returns True if the configuration is correct and False if it's
      not. In the case of a failure, self._message can be updated to contain a
      description of the error.
  """

  def __init__(self):
    self._system_drive = os.environ['SystemDrive'] + '\\'
    self._visual_studio = os.path.join(os.environ['ProgramFiles'],
                                       'Microsoft Visual Studio 8', 'Common7',
                                       'IDE', 'devenv.exe')

    self._depot_tools = os.path.join(self._system_drive, 'b', 'depot_tools')
    self._gclient = os.path.join(self._depot_tools, 'gclient.bat')

    self._scripts = os.path.join(self._system_drive, 'b', 'scripts')
    self._slave = os.path.join(self._system_drive, 'b', 'slave')

    desktop = os.path.join(os.environ['USERPROFILE'], 'Desktop')
    self._run_shortcut = os.path.join(desktop, 'run_slave.bat.lnk')
    self._procexp = os.path.join(desktop, 'procexp.exe')
    self._procmon = os.path.join(desktop, 'procmon.exe')

    self._message = []

  def _UpdateMessage(self, message):
    """ Adds one line to self._message.
    """
    self._message.append('%s%s' % (' '*9, message))

  def _DisplayResult(self, result, key):
    if result:
      print '[OK]   ' + MESSAGES[key][0]
    else:
      print '[FAIL] ' + MESSAGES[key][1]
      if self._message:
        print '%sMore Info:' % (' '*7)
        print '\n'.join(self._message)

    self._message = []

  def _CheckVSSp1(self):
    """Checks if the service pack 1 is installed for visual studio.
    """
    try:
      win32api.RegOpenKeyEx(HKLM, VS_SP1_KEY)
    except win32api.error:
      return False
    return True

  def _CheckDepotToolsInPath(self):
    """Checks if depot_tools is in the path
    """
    if os.environ['PATH'].find(self._depot_tools) == -1:
      return False
    return True

  def _CheckPerformanceSettings(self):
    """Checks if the performances settings are set for Background Services and
       System Cache.
    """
    priority = QueryKeyValue(HKLM, BACKGROUND, 'Win32PrioritySeparation')
    cache = QueryKeyValue(HKLM, CACHE_KEY, 'LargeSystemCache')

    if priority == 24 and cache == 1:
      return True
    return False

  def _CheckRunKey(self):
    """Checks if there are items in the Run key. Returns True if it's empty.
    """
    hklm_run = win32api.RegOpenKeyEx(win32con.HKEY_LOCAL_MACHINE,
                   'SOFTWARE\Microsoft\Windows\CurrentVersion\Run')
    hkcu_run = win32api.RegOpenKeyEx(win32con.HKEY_CURRENT_USER,
                   'Software\Microsoft\Windows\CurrentVersion\Run')

    empty = True
    try:
      id = 0
      while(True):
        (str, obj, type) = win32api.RegEnumValue(hklm_run, id)
        if str:
          empty = False
          self._UpdateMessage('Found HKLM: %s' % str)
        id = id + 1
    except win32api.error:
      pass

    try:
      id = 0
      while(True):
        (str, obj, type) = win32api.RegEnumValue(hkcu_run, id)
        if str:
          empty = False
          self._UpdateMessage('Found HKCU: %s' % str)
        id = id + 1
    except win32api.error:
      pass

    return empty

  def _CheckStartupFolder(self):
    """ Returns True if the Startup folders are empty.
    """
    current_startup = os.path.join(os.environ['USERPROFILE'], 'Start Menu',
                                   'Programs', 'Startup')
    all_users_startup = os.path.join(os.environ['ALLUSERSPROFILE'],
                                     'Start Menu', 'Programs', 'Startup')

    current = os.listdir(current_startup)
    all_users = os.listdir(all_users_startup)

    if len(current) == 0 and len(all_users) == 0:
      return True

    for file in current:
      self._UpdateMessage('Found: %s' % file)
    for file in all_users:
      self._UpdateMessage('Found: %s' % file)

    return False

  def _CheckSystemRestore(self):
    """ Returns True if system restore is disabled.
    """
    disable_restore = QueryKeyValue(HKLM, SYSTEM_RESTORE, 'DisableSR')

    if disable_restore == 1:
      return True
    return False

  def _CheckBuildDirectory(self):
    """ Returns True if there is no old build directory that needs to be
        deleted.
    """
    dirs = os.listdir(self._slave)

    all_empty = True
    for directory in dirs:
      if not os.path.isdir(directory):
        continue
      # Check if there is more than 1 build dir in this directory.
      all_builds = [x for x in os.listdir(directory)
                    if os.path.isdir(x) and x.find('build') != -1]
      if len(all_builds) > 1:
        self._UpdateMessage('Check: %s' % cur_dir)
        all_empty = False

    return all_empty

  def _CheckDefrag(self):
     """ Returns True if there is a defrag task set up on the machine.
         We know that the task exists when "schtasks.exe /query /v"
         returns a line that has "defrag" in it but not "Never".
     """
     scheduled_tasks = subprocess.Popen(['schtasks.exe', '/query', '/v'],
                                        stdout=subprocess.PIPE, bufsize=0)
     for line in scheduled_tasks.stdout:
       if line.find('defrag') != -1 and line.find('Never') == -1:
         return True

     self._UpdateMessage('Run: schtasks /create /TN defrag /RU '
                         '"google\chrome-bot" /SC daily /st '
                         '04:00:00 /TR "defrag %systemdrive%"')
     return False

  def _CheckDEP(self):
    """Checks if Date Execution Prevention is enabled on the machine.
    """
    try:
      dep = QueryKeyValue(HKLM, APP_COMPAT, 'LastNoExecuteRadioButtonState')
      # Usually when DEP is activated, the value of this key is 13013,
      # otherwise it's non-existent or 13012.
      # TODO(nsylvain): Find a better way to get the DEP state.
      if dep == 13013:
        return True
    except win32api.error:
      return False
    return False

  def RunTest(self):
    """Checks the configuration of a slave and report the errors.
    """
    self._DisplayResult(os.path.exists(self._visual_studio), 'VSCHECK')
    self._DisplayResult(self._CheckVSSp1(), 'VSSP1')
    self._DisplayResult(os.path.exists(self._gclient), 'GCLIENT')
    self._DisplayResult(self._CheckDepotToolsInPath(), 'DT_PATH')
    self._DisplayResult(os.path.exists(self._scripts), 'SCRIPT')
    self._DisplayResult(self._CheckPerformanceSettings(), 'PERF')
    self._DisplayResult(self._CheckRunKey(), 'RUNKEY')
    self._DisplayResult(self._CheckSystemRestore(), 'RESTORE')
    self._DisplayResult(self._CheckStartupFolder(), 'STARTUP')
    self._DisplayResult(self._CheckBuildDirectory(), 'OLDBUILD')
    self._DisplayResult(os.path.exists(self._run_shortcut), 'SHORTCUT')
    self._DisplayResult(os.path.exists(self._procexp), 'PROCEXP')
    self._DisplayResult(os.path.exists(self._procmon), 'PROCMON')
    self._DisplayResult(self._CheckDefrag(), 'DEFRAG')
    self._DisplayResult(self._CheckDEP(), 'DEP')

    print """


Checklist:
[ 1] Install the certificate.
[ 2] Go to Add/Remove programs and uninstall everything not needed.
[ 3] Disable the Anti-Virus.
[ 4] Configure buildbot.tac
[ 5] Configure info\\host and info\\admin.
    """


if '__main__' == __name__:
  slave_checker = SlaveChecker()
  sys.exit(slave_checker.RunTest())
