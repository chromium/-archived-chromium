/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// A basic C++ wrapper for a zip file
// Adapted from miniunz.c from minizip open source code by Gilles Vollant.

/*
  Copyright (C) 1998-2005 Gilles Vollant

  This unzip package allow creates .ZIP file, compatible with PKZip 2.04g
  WinZip, InfoZip tools and compatible.
  Multi volume ZipFile (span) are not supported.
  Encryption compatible with pkzip 2.04g only supported
  Old compressions used by old PKZip 1.x are not supported

  For uncompress .zip file, look at unzip.h


  I WAIT FEEDBACK at mail info@winimage.com
  Visit also http://www.winimage.com/zLibDll/unzip.html for evolution

  Condition of use and distribution are the same than zlib :

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
  claim that you wrote the original software. If you use this software
  in a product, an acknowledgment in the product documentation would be
  appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
  misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

*/


/* Examples Usages:

   int result;
   ZipArchive archive("test.zip", &result);

   if (result == UNZ_OK) {
   archive.Print();

   vector<ZipFileInfo> infolist;
   archive.GetInformationList(&infolist);

   // get all file information and print all file names
   for (int i = 0; i < infolist.size(); ++i) {
   printf("[%d] %s\n", i + 1, infolist[i].name.c_str() );
   }

   // can get information based on file name in the archive
   // print information about the first file
   if (infolist.size() > 0) {
   ZipFileInfo info;
   result = archive.GetFileInfo(infolist[0].name, &info);

   if (result == UNZ_OK) {
   info.Print(true);
   }
   }


   size_t data_size;
   char *data = archive.GetFileData(infolist[0].name, &data_size);
   printf("data = %p : data_size = %d\n", data, data_size);
*/

#ifndef O3D_IMPORT_CROSS_ZIP_ARCHIVE_H_
#define O3D_IMPORT_CROSS_ZIP_ARCHIVE_H_

#include <string>
#include <vector>

#include "contrib/minizip/unzip.h"

#if defined(OS_WIN)
// Windows #defines this.
#undef DeleteFile
#endif

// structure containing the unz_file_info information plus the file name
struct ZipFileInfo : public unz_file_info {
  std::string name;

  void  Print(bool print_header);  // prints info to stdout
};

class ZipArchive {
 public:
  // Returns UNZ_OK in |result| on success
  ZipArchive(const std::string &zip_filename, int *result);
  virtual ~ZipArchive();

  // The returned filenames should adhere to the zip archive spec
  // (UTF8 with '/' as the path separator)
  // If the zip file is badly constructed then this assumption may be invalid.
  // returns UNZ_OK on success
  int  GetInformationList(std::vector<ZipFileInfo> *infolist);

  // Returns information for |filename| in |*info|
  // returns UNZ_OK if successful
  virtual int GetFileInfo(const std::string &filename, ZipFileInfo *info);

  // Extracts a single file and returns a pointer to the file's content.
  // Returns NULL if |filename| doesn't match any in the archive
  // or an error occurs.  The caller must call free() on the returned pointer
  // the buffer allocated will actually be one byte greater then reported in
  // |size|.  And this extra byte is set to zero.  This way the data may
  // be interpreted as a string, and doesn't harm anything else.
  //
  virtual char  *GetFileData(const std::string &filename, size_t *size);

  // |relative_path| is taken to be relative to |root_path|
  // It may contain relative path elements ("../")
  //
  // Extracts a single file and returns a pointer to the file's content.
  // Returns NULL if |filename| doesn't match any in the archive
  // or an error occurs.  The caller must call free() on the returned pointer
  //
  virtual char  *GetRelativeFileData(const std::string &relative_path,
                                     const std::string &root_path,
                                     size_t *size);

  // Extracts the entire archive to disk (relative to current working dir)
  // returns UNZ_OK on success
  int   Extract();

  // Extracts a single file to disk (relative to current working dir)
  // returns UNZ_OK on success
  int   ExtractOneFile(const std::string &filename,
                       int opt_extract_without_path,
                       int opt_overwrite,
                       const char *password);

  // Extracts |filename| from the archive, saves to a temp file, and sets
  // the path to the temp file in |temp_filename|
  // returns |true| on success
  //
  bool GetTempFileFromFile(const std::string &filename,
                           std::string *temp_filename);

  // So we can delete the temp file we made
  static void DeleteFile(const std::string &filename);

  // Lists content of archive to stdout
  // returns UNZ_OK on success
  int   Print();

  // Tests the given file to see if it is a zip file.
  // (Added by Google)
  static bool IsZipFile(const std::string& filename);

 protected:
  void  ChangeFileDate(const char *filename, uLong dosdate, tm_unz tmu_date);

  int   MyMkDir(const char *dirname);

  int   MakeDir(const char *newdir);

  int   ExtractCurrentFile(const int *popt_extract_without_path,
                           int *popt_overwrite,
                           const char *password);

  int   DoExtract(int opt_extract_without_path,
                  int opt_overwrite,
                  const char *password);

  void  RemoveLastPathComponent(std::string *string);

  void  ConvertRelativeToAbsolutePath(std::string *path,
                                      const std::string &root_path);

  void GetActualFilename(const std::string &filename,
                         std::string *actual_filename);

  std::string     zip_filename_;
  unzFile         zip_file_ref_;
};

#endif  // O3D_IMPORT_CROSS_ZIP_ARCHIVE_H_
