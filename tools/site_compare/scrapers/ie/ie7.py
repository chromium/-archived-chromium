#!/usr/bin/python2.4
# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Does scraping for all known versions of IE."""

import pywintypes
import time
import types

from drivers import keyboard
from drivers import mouse
from drivers import windowing

# Default version
version = "7.0.5730.1"

DEFAULT_PATH = r"c:\program files\internet explorer\iexplore.exe"

def GetBrowser(path):
  """Invoke the IE browser and return the process, frame, and content window.

  Args:
    path: full path to browser
      
  Returns:
    A tuple of (process handle, render pane)
  """
  if not path: path = DEFAULT_PATH
  
  (iewnd, ieproc, address_bar, render_pane, tab_window) = InvokeBrowser(path)
  return (ieproc, iewnd, render_pane)


def InvokeBrowser(path):
  """Invoke the IE browser.
  
  Args:
    path: full path to browser
      
  Returns:
    A tuple of (main window, process handle, address bar,
                render_pane, tab_window)
  """
  # Invoke IE
  (ieproc, iewnd) = windowing.InvokeAndWait(path)
  
  # Get windows we'll need
  for tries in xrange(10):
    try:
      address_bar = windowing.FindChildWindow(
        iewnd, "WorkerW|Navigation Bar/ReBarWindow32/"
        "Address Band Root/ComboBoxEx32/ComboBox/Edit")
      render_pane = windowing.FindChildWindow(
        iewnd, "TabWindowClass/Shell DocObject View")
      tab_window = windowing.FindChildWindow(
        iewnd, "CommandBarClass/ReBarWindow32/TabBandClass/DirectUIHWND")
    except IndexError:
      time.sleep(1)
      continue
    break
  
  return (iewnd, ieproc, address_bar, render_pane, tab_window)


def Scrape(urls, outdir, size, pos, timeout=20, **kwargs):
  """Invoke a browser, send it to a series of URLs, and save its output.
    
  Args:
    urls: list of URLs to scrape
    outdir: directory to place output
    size: size of browser window to use
    pos: position of browser window
    timeout: amount of time to wait for page to load
    kwargs: miscellaneous keyword args
  
  Returns:
    None if success, else an error string
  """
  path = r"c:\program files\internet explorer\iexplore.exe"
  
  if "path" in kwargs and kwargs["path"]: path = kwargs["path"]

  (iewnd, ieproc, address_bar, render_pane, tab_window) = (
    InvokeBrowser(path) )
    
  # Resize and reposition the frame
  windowing.MoveAndSizeWindow(iewnd, pos, size, render_pane)
  
  # Visit each URL we're given
  if type(urls) in types.StringTypes: urls = [urls]
  
  timedout = False
  
  for url in urls:
    
    # Double-click in the address bar, type the name, and press Enter
    mouse.DoubleClickInWindow(address_bar)
    keyboard.TypeString(url)
    keyboard.TypeString("\n")
    
    # Wait for the page to finish loading
    load_time = windowing.WaitForThrobber(
      tab_window, (6, 8, 22, 24), timeout)
    timedout = load_time < 0

    if timedout:
      break
    
    # Scrape the page
    image = windowing.ScrapeWindow(render_pane)
    
    # Save to disk
    if "filename" in kwargs:
      if callable(kwargs["filename"]):
        filename = kwargs["filename"](url)
      else:
        filename = kwargs["filename"]
    else:
      filename = windowing.URLtoFilename(url, outdir, ".bmp")
    image.save(filename)
  
  windowing.EndProcess(ieproc)
  
  if timedout:
    return "timeout"
    
    
def Time(urls, size, timeout, **kwargs):
  """Measure how long it takes to load each of a series of URLs
  
  Args:
    urls: list of URLs to time
    size: size of browser window to use
    timeout: amount of time to wait for page to load
    kwargs: miscellaneous keyword args
  
  Returns:
    A list of tuples (url, time). "time" can be "crashed" or "timeout"
  """
  if "path" in kwargs and kwargs["path"]: path = kwargs["path"]
  else: path = DEFAULT_PATH
  proc = None
  
  # Visit each URL we're given
  if type(urls) in types.StringTypes: urls = [urls]
  
  ret = []
  for url in urls:
    try:
      # Invoke the browser if necessary
      if not proc:
        (wnd, proc, address_bar, render_pane, tab_window) = InvokeBrowser(path)
        
        # Resize and reposition the frame
        windowing.MoveAndSizeWindow(wnd, (0,0), size, render_pane)
        
      # Double-click in the address bar, type the name, and press Enter
      mouse.DoubleClickInWindow(address_bar)
      keyboard.TypeString(url)
      keyboard.TypeString("\n")
      
      # Wait for the page to finish loading
      load_time = windowing.WaitForThrobber(
        tab_window, (6, 8, 22, 24), timeout)
      timedout = load_time < 0
      
      if timedout:
        load_time = "timeout"
        
        # Send an alt-F4 to make the browser close; if this times out,
        # we've probably got a crash
        keyboard.TypeString(r"{\4}", use_modifiers=True)
        if not windowing.WaitForProcessExit(proc, timeout):
          windowing.EndProcess(proc)
          load_time = "crashed"
        proc = None
    except pywintypes.error:
      load_time = "crashed"
      proc = None
      
    ret.append( (url, load_time) )
    
  # Send an alt-F4 to make the browser close; if this times out,
  # we've probably got a crash
  if proc:
    keyboard.TypeString(r"{\4}", use_modifiers=True)
    if not windowing.WaitForProcessExit(proc, timeout):
      windowing.EndProcess(proc)

  return ret

    
if __name__ == "__main__":
  # We're being invoked rather than imported, so run some tests
  path = r"c:\sitecompare\scrapes\ie7\7.0.5380.11"
  windowing.PreparePath(path)

  # Scrape three sites and save the results
  Scrape(
    ["http://www.microsoft.com",
     "http://www.google.com",
     "http://www.sun.com"],
    path, (1024, 768), (0, 0))
