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

"""Does scraping for all currently-known versions of Chrome"""

import pywintypes
import types

from drivers import keyboard
from drivers import mouse
from drivers import windowing

# TODO: this has moved, use some logic to find it. For now,
# expects a subst k:.
DEFAULT_PATH = r"k:\chrome.exe"

def InvokeBrowser(path):
  """Invoke the Chrome browser.
  
  Args:
    path: full path to browser
      
  Returns:
    A tuple of (main window, process handle, address bar, render pane)
  """
    
  # Reuse an existing instance of the browser if we can find one. This
  # may not work correctly, especially if the window is behind other windows.
  
  # TODO(jhaas): make this work with Vista
  wnds = windowing.FindChildWindows(0, "Chrome_XPFrame")
  if len(wnds):
    wnd = wnds[0]
    proc = None
  else:
    # Invoke Chrome
    (proc, wnd) = windowing.InvokeAndWait(path)
  
  # Get windows we'll need
  address_bar = windowing.FindChildWindow(wnd, "Chrome_AutocompleteEdit")
  render_pane = GetChromeRenderPane(wnd)
  
  return (wnd, proc, address_bar, render_pane)

  
def Scrape(urls, outdir, size, pos, timeout, kwargs):
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
  if "path" in kwargs and kwargs["path"]: path = kwargs["path"]
  else: path = DEFAULT_PATH
  
  (wnd, proc, address_bar, render_pane) = InvokeBrowser(path)
  
  # Resize and reposition the frame
  windowing.MoveAndSizeWindow(wnd, pos, size, render_pane)
  
  # Visit each URL we're given
  if type(urls) in types.StringTypes: urls = [urls]

  timedout = False
  
  for url in urls:
    # Double-click in the address bar, type the name, and press Enter
    mouse.ClickInWindow(address_bar)
    keyboard.TypeString(url, 0.1)
    keyboard.TypeString("\n")
    
    # Wait for the page to finish loading
    load_time = windowing.WaitForThrobber(wnd, (20, 16, 36, 32), timeout)
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
  
  if proc:
    windowing.SetForegroundWindow(wnd)
    
    # Send Alt-F4, then wait for process to end
    keyboard.TypeString(r"{\4}", use_modifiers=True)
    if not windowing.WaitForProcessExit(proc, timeout):
      windowing.EndProcess(proc)
      return "crashed"
    
  if timedout:
    return "timeout"
  
  return None


def Time(urls, size, timeout, kwargs):
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
        (wnd, proc, address_bar, render_pane) = InvokeBrowser(path)
        
        # Resize and reposition the frame
        windowing.MoveAndSizeWindow(wnd, (0,0), size, render_pane)
        
      # Double-click in the address bar, type the name, and press Enter
      mouse.ClickInWindow(address_bar)
      keyboard.TypeString(url, 0.1)
      keyboard.TypeString("\n")
      
      # Wait for the page to finish loading
      load_time = windowing.WaitForThrobber(wnd, (20, 16, 36, 32), timeout)
      
      timedout = load_time < 0
      
      if timedout:
        load_time = "timeout"
        
        # Send an alt-F4 to make the browser close; if this times out,
        # we've probably got a crash
        windowing.SetForegroundWindow(wnd)
      
        keyboard.TypeString(r"{\4}", use_modifiers=True)
        if not windowing.WaitForProcessExit(proc, timeout):
          windowing.EndProcess(proc)
          load_time = "crashed"
        proc = None
    except pywintypes.error:
      proc = None
      load_time = "crashed"
            
    ret.append( (url, load_time) )

  if proc:    
    windowing.SetForegroundWindow(wnd)
    keyboard.TypeString(r"{\4}", use_modifiers=True)
    if not windowing.WaitForProcessExit(proc, timeout):
      windowing.EndProcess(proc)

  return ret


if __name__ == "__main__":
  # We're being invoked rather than imported, so run some tests
  path = r"c:\sitecompare\scrapes\chrome\0.1.97.0"
  windowing.PreparePath(path)
  
  # Scrape three sites and save the results
  Scrape([
    "http://www.microsoft.com",
    "http://www.google.com",
    "http://www.sun.com"],
         path, (1024, 768), (0, 0))
