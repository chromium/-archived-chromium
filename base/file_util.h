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

// This file contains utility functions for dealing with the local
// filesystem.

#ifndef BASE_FILE_UTIL_H__
#define BASE_FILE_UTIL_H__

#include <windows.h>
#include <stack>
#include <string>

#include "base/basictypes.h"

namespace file_util {

//-----------------------------------------------------------------------------
// Constants

extern const wchar_t kPathSeparator;


//-----------------------------------------------------------------------------
// Functions that operate purely on a path string w/o touching the filesystem:

// Returns true if the given path ends with a path separator character.
bool EndsWithSeparator(std::wstring* path);

// Modifies a string by trimming all trailing separators from the end.
void TrimTrailingSeparator(std::wstring* dir);

// Strips the topmost directory from the end of 'dir'.  Assumes 'dir' does not
// refer to a file.
// If 'dir' is a root directory, return without change.
void UpOneDirectory(std::wstring* dir);

// Strips the topmost directory from the end of 'dir'.  Assumes 'dir' does not
// refer to a file.
// If 'dir' is a root directory, the result becomes empty string.
void UpOneDirectoryOrEmpty(std::wstring* dir);

// Strips the filename component from the end of 'path'.
void TrimFilename(std::wstring* path);

// Returns the filename portion of 'path', without any leading \'s or /'s.
std::wstring GetFilenameFromPath(const std::wstring& path);

// Returns "jpg" for path "C:\pics\jojo.jpg", or an empty string if
// the file has no extension.
std::wstring GetFileExtensionFromPath(const std::wstring& path);

// Returns the directory component of a path, without the trailing
// path separator, or an empty string on error. The function does not
// check for the existence of the path, so if it is passed a directory
// without the trailing \, it will interpret the last component of the
// path as a file and chomp it. This does not support relative paths.
// Examples:
// path == "C:\pics\jojo.jpg",     returns "C:\pics"
// path == "C:\Windows\system32\", returns "C:\Windows\system32"
// path == "C:\Windows\system32",  returns "C:\Windows"
std::wstring GetDirectoryFromPath(const std::wstring& path);

// Appends new_ending to path, adding a separator between the two if necessary.
void AppendToPath(std::wstring* path, const std::wstring& new_ending);

// Inserts |suffix| after the file name portion of |path| but before the
// extension.
// Examples:
// path == "C:\pics\jojo.jpg" suffix == " (1)", returns "C:\pics\jojo (1).jpg"
// path == "jojo.jpg"         suffix == " (1)", returns "jojo (1).jpg"
// path == "C:\pics\jojo"     suffix == " (1)", returns "C:\pics\jojo (1)"
// path == "C:\pics.old\jojo" suffix == " (1)", returns "C:\pics.old\jojo (1)"
void InsertBeforeExtension(std::wstring* path, const std::wstring& suffix);

// Replaces characters in 'file_name' that are illegal for file names with
// 'replace_char'. 'file_name' must not be a full or relative path, but just the
// file name component. Any leading or trailing whitespace in 'file_name' is
// removed.
// Example:
//   file_name == "bad:file*name?.txt", changed to: "bad-file-name-.txt" when
//   'replace_char' is '-'.
void ReplaceIllegalCharacters(std::wstring* file_name, int replace_char);

// Replaces the extension of |file_name| with |extension|.  If |file_name|
// does not have an extension, them |extension| is added.  If |extention| is
// empty, then the extension is removed from |file_name|.
void ReplaceExtension(std::wstring* file_name, const std::wstring& extension);

//-----------------------------------------------------------------------------
// Functions that involve filesystem access or modification:

// Returns the number of files matching the current path that were
// created on or after the given FILETIME.  Doesn't count ".." or ".".
// Filetime is UTC filetime, not LocalFiletime.
int CountFilesCreatedAfter(const std::wstring& path,
                           const FILETIME& file_time);

// Deletes the given path, whether it's a file or a directory.
// If it's a directory, it's perfectly happy to delete all of the
// directory's contents.  Passing true to recursive deletes
// subdirectories and their contents as well.
// Returns true if successful, false otherwise.
//
// WARNING: USING THIS WITH recursive==true IS EQUIVALENT
//          TO "rm -rf", SO USE WITH CAUTION.
bool Delete(const std::wstring& path, bool recursive);

// Moves the given path, whether it's a file or a directory.
// Returns true if successful, false otherwise.
bool Move(const std::wstring& from_path, const std::wstring& to_path);

// Copies a single file. Use CopyDirectory to copy directories.
bool CopyFile(const std::wstring& from_path, const std::wstring& to_path);

// Copies the given path, and optionally all subdirectories and their contents
// as well.
// If there are files existing under to_path, always overwrite.
// Returns true if successful, false otherwise.
// Dont't use wildcards on the names, it may stop working without notice.
//
// If you only need to copy a file use CopyFile, it's faster.
bool CopyDirectory(const std::wstring& from_path, const std::wstring& to_path,
                   bool recursive);

// Returns true if the given path exists on the local filesystem,
// false otherwise.
bool PathExists(const std::wstring& path);

// Returns true if the given path is writable by the user, false otherwise.
bool PathIsWritable(const std::wstring& path);

// Gets the creation time of the given file (expressed in the local timezone),
// and returns it via the creation_time parameter.  Returns true if successful,
// false otherwise.
bool GetFileCreationLocalTime(const std::wstring& filename,
                              LPSYSTEMTIME creation_time);

// Same as above, but takes a previously-opened file handle instead of a name.
bool GetFileCreationLocalTimeFromHandle(HANDLE file_handle,
                                        LPSYSTEMTIME creation_time);

// Returns true if the contents of the two files given are equal, false
// otherwise.  If either file can't be read, returns false.
bool ContentsEqual(const std::wstring& filename1,
                   const std::wstring& filename2);

// Read the file at |path| into |contents|, returning true on success.
// Useful for unit tests.
bool ReadFileToString(const std::wstring& path, std::string* contents);

// Resolve Windows shortcut (.LNK file)
// Argument path specifies a valid LNK file. On success, return true and put
// the URL into path. If path is a invalid .LNK file, return false.
bool ResolveShortcut(std::wstring* path);

// Create a Windows shortcut (.LNK file)
// This method creates a shortcut link using the information given. Ensure
// you have initialized COM before calling into this function. 'source'
// and 'destination' parameters are required, everything else can be NULL.
// 'source' is the existing file, 'destination' is the new link file to be
// created; for best resoults pass the filename with the .lnk extension.
// The 'icon' can specify a dll or exe in which case the icon index is the
// resource id.
// Note that if the shortcut exists it will overwrite it.
bool CreateShortcutLink(const wchar_t *source, const wchar_t *destination,
                        const wchar_t *working_dir, const wchar_t *arguments,
                        const wchar_t *description, const wchar_t *icon,
                        int icon_index);

// Update a Windows shortcut (.LNK file). This method assumes the shortcut
// link already exists (otherwise false is returned). Ensure you have
// initialized COM before calling into this function. Only 'destination'
// parameter is required, everything else can be NULL (but if everthing else
// is NULL no changes are made to the shortcut). 'destination' is the link
// file to be updated. For best results pass the filename with the .lnk
// extension.
bool UpdateShortcutLink(const wchar_t *source, const wchar_t *destination,
                        const wchar_t *working_dir, const wchar_t *arguments,
                        const wchar_t *description, const wchar_t *icon,
                        int icon_index);

// Get the temporary directory provided by the system.
bool GetTempDir(std::wstring* path);

// Creates a temporary file name, but does it not create the file. It accesses
// the disk to do this, however. The full path is placed in 'temp_file', and the
// function returns true if was successful in creating the file name.
bool CreateTemporaryFileName(std::wstring* temp_file);

// Create a new directory under TempPath. If prefix is provided, the new
// directory name is in the format of prefixyyyy.
// If success, return true and output the full path of the directory created.
bool CreateNewTempDirectory(const std::wstring& prefix,
                            std::wstring* new_temp_path);

// Creates a directory, as well as creating any parent directories, if they
// don't exist. Returns 'true' on successful creation.
bool CreateDirectory(const std::wstring& full_path);

// Returns the file size. Returns true on success.
bool GetFileSize(const std::wstring& file_path, int64* file_size);

// Reads the given number of bytes from the file into the buffer.  Returns
// the number of read bytes, or -1 on error.
int ReadFile(const std::wstring& filename, char* data, int size);

// Writes the given buffer into the file, overwriting any data that was
// previously there.  Returns the number of bytes written, or -1 on error.
int WriteFile(const std::wstring& filename, const char* data, int size);

// A class for enumerating the files in a provided path. The order of the
// results is not guaranteed.
//
// DO NOT USE FROM THE MAIN THREAD of your application unless it is a test
// program where latency does not matter. This class is blocking.
class FileEnumerator {
 public:
  enum FILE_TYPE {
    FILES                 = 0x1,
    DIRECTORIES           = 0x2,
    FILES_AND_DIRECTORIES = 0x3
  };

  // |root_path| is the starting directory to search for. It may or may not end
  // in a slash.
  //
  // If |recursive| is true, this will enumerate all matches in any
  // subdirectories matched as well. It does a breadth-first search, so all
  // files in one directory will be returned before any files in a
  // subdirectory.
  //
  // The last parameter is an optional pattern for which files to match. This
  // works like a Windows file pattern. For example, "*.txt" or "Foo???.doc".
  // If unspecified, this will match all files.
  FileEnumerator(const std::wstring& root_path,
                 bool recursive,
                 FileEnumerator::FILE_TYPE file_type);
  FileEnumerator(const std::wstring& root_path,
                 bool recursive,
                 FileEnumerator::FILE_TYPE file_type,
                 const std::wstring& pattern);
  ~FileEnumerator();

  // Returns an empty string if there are no more results.
  std::wstring Next();

 private:
  std::wstring root_path_;
  bool recursive_;
  FILE_TYPE file_type_;
  std::wstring pattern_;  // Empty when we want to find everything.

  // Set to true when there is a find operation open. This way, we can lazily
  // start the operations when the caller calls Next().
  bool is_in_find_op_;

  // A stack that keeps track of which subdirectories we still need to
  // enumerate in the breadth-first search.
  std::stack<std::wstring> pending_paths_;

  WIN32_FIND_DATA find_data_;
  HANDLE find_handle_;

  DISALLOW_EVIL_CONSTRUCTORS(FileEnumerator);
};

// Renames a file using the MoveFileEx API and ensures that the target file gets
// the correct security descriptor in the new path.
bool RenameFileAndResetSecurityDescriptor(
    const std::wstring& source_file_path,
    const std::wstring& target_file_path);

}  // namespace file_util

#endif  // BASE_FILE_UTIL_H__
