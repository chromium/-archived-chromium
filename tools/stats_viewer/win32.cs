// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

namespace StatsViewer {
  /// <summary>
  /// Win32 API constants, structs, and wrappers for access via C#.
  /// </summary>
  class Win32 {
    #region Constants
    public enum MapAccess {
      FILE_MAP_COPY = 0x0001,
      FILE_MAP_WRITE = 0x0002,
      FILE_MAP_READ = 0x0004,
      FILE_MAP_ALL_ACCESS = 0x001f,
    }

    public const int GENERIC_READ = unchecked((int)0x80000000);
    public const int GENERIC_WRITE = unchecked((int)0x40000000);
    public const int OPEN_ALWAYS = 4;
    public static readonly IntPtr INVALID_HANDLE_VALUE = new IntPtr(-1);
    #endregion

    [DllImport("kernel32", SetLastError=true, CharSet=CharSet.Auto)]
    public static extern IntPtr CreateFile ( 
       String lpFileName, int dwDesiredAccess, int dwShareMode,
       IntPtr lpSecurityAttributes, int dwCreationDisposition,
       int dwFlagsAndAttributes, IntPtr hTemplateFile);

    [DllImport("kernel32", SetLastError=true)]
    public static extern IntPtr MapViewOfFile (
       IntPtr hFileMappingObject, int dwDesiredAccess, int dwFileOffsetHigh,
       int dwFileOffsetLow, int dwNumBytesToMap);

    [DllImport("kernel32", SetLastError=true, CharSet=CharSet.Auto)]
    public static extern IntPtr OpenFileMapping (
       int dwDesiredAccess, bool bInheritHandle, String lpName);

    [DllImport("kernel32", SetLastError=true)]
    public static extern bool UnmapViewOfFile (IntPtr lpBaseAddress);

    [DllImport("kernel32", SetLastError = true)]
    public static extern bool CloseHandle(IntPtr handle);
  }
}
