// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_ftp_job.h"

#include <windows.h>
#include <wininet.h>

#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/time.h"
#include "net/base/auth.h"
#include "net/base/load_flags.h"
#include "net/base/net_util.h"
#include "net/base/wininet_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"
#include "net/base/escape.h"

using std::string;

using net::WinInetUtil;

// When building the directory listing, the period to wait before notifying
// the parent class that we wrote the data.
#define kFtpBufferTimeMs 50

static bool UnescapeAndValidatePath(const URLRequest* request,
                                    std::string* unescaped_path) {
  // Path in GURL is %-encoded UTF-8. FTP servers do not
  // understand %-escaped path so that we have to unescape leading to an
  // unescaped UTF-8 path. Then, the presence of NULL, CR and LF is checked
  // because they're not allowed in FTP.
  // TODO(jungshik) Even though RFC 2640 specifies that UTF-8 be used.
  // There are many FTP servers that use legacy encodings. For them,
  // we need to identify the encoding and convert to that encoding.
  static const std::string kInvalidChars("\x00\x0d\x0a", 3);
  *unescaped_path = UnescapeURLComponent(request->url().path(),
      UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS);
  if (unescaped_path->find_first_of(kInvalidChars) != std::string::npos) {
    SetLastError(ERROR_INTERNET_INVALID_URL);
    // GURL path should not contain '%00' which is NULL(0x00) when unescaped.
    // URLRequestFtpJob should not have been invoked for an invalid GURL.
    DCHECK(unescaped_path->find(std::string("\x00", 1)) == std::string::npos) <<
        "Path should not contain %00.";
    return false;
  }
  return true;
}

// static
URLRequestJob* URLRequestFtpJob::Factory(URLRequest* request,
                                         const std::string &scheme) {
  DCHECK(scheme == "ftp");

  if (request->url().has_port() &&
      !net::IsPortAllowedByFtp(request->url().IntPort()))
    return new URLRequestErrorJob(request, net::ERR_UNSAFE_PORT);

  return new URLRequestFtpJob(request);
}

URLRequestFtpJob::URLRequestFtpJob(URLRequest* request)
    : URLRequestInetJob(request), state_(START), is_directory_(false),
      dest_(NULL), dest_size_(0) {
}

URLRequestFtpJob::~URLRequestFtpJob() {
}

void URLRequestFtpJob::Start() {
  GURL parts(request_->url());
  const std::string& scheme = parts.scheme();

  // We should only be dealing with FTP at this point:
  DCHECK(LowerCaseEqualsASCII(scheme, "ftp"));

  SendRequest();
}

bool URLRequestFtpJob::GetMimeType(std::string* mime_type) const {
  if (!is_directory_)
    return false;

  mime_type->assign("text/html");
  return true;
}

void URLRequestFtpJob::OnCancelAuth() {
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestFtpJob::ContinueNotifyHeadersComplete));
}

void URLRequestFtpJob::OnSetAuth() {
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestFtpJob::SendRequest));
}

void URLRequestFtpJob::SendRequest() {
  state_ = CONNECTING;

  DWORD flags =
    INTERNET_FLAG_KEEP_CONNECTION |
    INTERNET_FLAG_EXISTING_CONNECT |
    INTERNET_FLAG_PASSIVE |
    INTERNET_FLAG_RAW_DATA;

  // It doesn't make sense to ask for both a cache validation and a
  // reload at the same time.
  DCHECK(!((request_->load_flags() & net::LOAD_VALIDATE_CACHE) &&
           (request_->load_flags() & net::LOAD_BYPASS_CACHE)));

  if (request_->load_flags() & net::LOAD_BYPASS_CACHE)
    flags |= INTERNET_FLAG_RELOAD;

  // Apply authentication if we have any, otherwise authenticate
  // according to FTP defaults. (See InternetConnect documentation.)
  // First, check if we have auth in cache, then check URL.
  // That way a user can re-enter credentials, and we'll try with their
  // latest input rather than always trying what they specified
  // in the url (if anything).
  string username, password;
  bool have_auth = false;
  if (server_auth_ && server_auth_->state == net::AUTH_STATE_HAVE_AUTH) {
    // Add auth info to cache
    have_auth = true;
    username = WideToUTF8(server_auth_->username);
    password = WideToUTF8(server_auth_->password);
    request_->context()->ftp_auth_cache()->Add(request_->url().GetOrigin(),
                                               server_auth_.get());
  } else {
    if (request_->url().has_username()) {
      username = request_->url().username();
      password = request_->url().has_password() ? request_->url().password() :
                 "";
      have_auth = true;
    }
  }

  int port = request_->url().has_port() ?
      request_->url().IntPort() : INTERNET_DEFAULT_FTP_PORT;

  connection_handle_ = InternetConnectA(GetTheInternet(),
                                        request_->url().host().c_str(),
                                        port,
                                        have_auth ? username.c_str() : NULL,
                                        have_auth ? password.c_str() : NULL,
                                        INTERNET_SERVICE_FTP, flags,
                                        reinterpret_cast<DWORD_PTR>(this));

  if (connection_handle_) {
    OnConnect();
  } else {
    ProcessRequestError(GetLastError());
  }
}

void URLRequestFtpJob::OnIOComplete(const AsyncResult& result) {
  if (state_ == CONNECTING) {
    switch (result.dwError) {
      case ERROR_NO_MORE_FILES:
        // url is an empty directory
        OnStartDirectoryTraversal();
        OnFinishDirectoryTraversal();
        return;
      case ERROR_INTERNET_LOGIN_FAILURE:
        // fall through
      case ERROR_INTERNET_INCORRECT_USER_NAME:
        // fall through
      case ERROR_INTERNET_INCORRECT_PASSWORD: {
        GURL origin = request_->url().GetOrigin();
        if (server_auth_ != NULL &&
            server_auth_->state == net::AUTH_STATE_HAVE_AUTH) {
          request_->context()->ftp_auth_cache()->Remove(origin);
        } else {
          server_auth_ = new net::AuthData();
        }
        server_auth_->state = net::AUTH_STATE_NEED_AUTH;

        scoped_refptr<net::AuthData> cached_auth =
            request_->context()->ftp_auth_cache()->Lookup(origin);

        if (cached_auth) {
          // Retry using cached auth data.
          SetAuth(cached_auth->username, cached_auth->password);
        } else {
          // The io completed fine, the error was due to invalid auth.
          SetStatus(URLRequestStatus());

          // Prompt for a username/password.
          NotifyHeadersComplete();
        }
        return;
      }
      case ERROR_SUCCESS:
        connection_handle_ = (HINTERNET)result.dwResult;
        OnConnect();
        return;
      case ERROR_INTERNET_EXTENDED_ERROR: {
        DWORD extended_err(ERROR_SUCCESS);
        DWORD size = 1;
        char buffer[1];
        if (!InternetGetLastResponseInfoA(&extended_err, buffer, &size))
          // We don't care about the error text here, so the only acceptable
          // error is one regarding insufficient buffer length.
          DCHECK(GetLastError() == ERROR_INSUFFICIENT_BUFFER);
        if (extended_err != ERROR_SUCCESS) {
          CleanupConnection();
          NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED,
                           WinInetUtil::OSErrorToNetError(extended_err)));
          return;
        }
        // Fall through in the case we saw ERROR_INTERNET_EXTENDED_ERROR but
        // InternetGetLastResponseInfo gave us no additional information.
      }
      default:
        CleanupConnection();
        NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED,
                         WinInetUtil::OSErrorToNetError(result.dwError)));
        return;
    }
  } else if (state_ == SETTING_CUR_DIRECTORY) {
    OnSetCurrentDirectory(result.dwError);
  } else if (state_ == FINDING_FIRST_FILE) {
    // We don't fail here if result.dwError != ERROR_SUCCESS because
    // getting an error here doesn't always mean the file is not found.
    // FindFirstFileA() issue a LIST command and may fail on some
    // ftp server when the requested object is a file. So ERROR_NO_MORE_FILES
    // from FindFirstFileA() is not a reliable criteria for valid path
    // or not, we should proceed optimistically by getting the file handle.
    if (result.dwError != ERROR_SUCCESS &&
        result.dwError != ERROR_NO_MORE_FILES) {
      DWORD result_error = result.dwError;
      CleanupConnection();
      NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED,
                       WinInetUtil::OSErrorToNetError(result_error)));
      return;
    }
    request_handle_ = (HINTERNET)result.dwResult;
    OnFindFirstFile(result.dwError);
  } else if (state_ == GETTING_DIRECTORY) {
    OnFindFile(result.dwError);
  } else if (state_ == GETTING_FILE_HANDLE) {
    if (result.dwError != ERROR_SUCCESS) {
      CleanupConnection();
      NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED,
                       WinInetUtil::OSErrorToNetError(result.dwError)));
      return;
    }
    // start reading file contents
    state_ = GETTING_FILE;
    request_handle_ = (HINTERNET)result.dwResult;
    NotifyHeadersComplete();
  } else {
    // we don't have IO outstanding.  Pass to our base class.
    URLRequestInetJob::OnIOComplete(result);
  }
}

bool URLRequestFtpJob::NeedsAuth() {
  // Note that we only have to worry about cases where an actual FTP server
  // requires auth (and not a proxy), because connecting to FTP via proxy
  // effectively means the browser communicates via HTTP, and uses HTTP's
  // Proxy-Authenticate protocol when proxy servers require auth.
  return server_auth_ && server_auth_->state == net::AUTH_STATE_NEED_AUTH;
}

void URLRequestFtpJob::GetAuthChallengeInfo(
    scoped_refptr<net::AuthChallengeInfo>* result) {
  DCHECK((server_auth_ != NULL) &&
         (server_auth_->state == net::AUTH_STATE_NEED_AUTH));
  scoped_refptr<net::AuthChallengeInfo> auth_info = new net::AuthChallengeInfo;
  auth_info->is_proxy = false;
  auth_info->host = UTF8ToWide(request_->url().host());
  auth_info->scheme = L"";
  auth_info->realm = L"";
  result->swap(auth_info);
}

void URLRequestFtpJob::OnConnect() {
  DCHECK_EQ(state_, CONNECTING);

  state_ = SETTING_CUR_DIRECTORY;
  // Setting the directory lets us determine if the URL is a file,
  // and also keeps the working directory for the FTP session in sync
  // with what is being displayed in the browser.
  if (request_->url().has_path()) {
    std::string unescaped_path;
    if (UnescapeAndValidatePath(request_, &unescaped_path) &&
        FtpSetCurrentDirectoryA(connection_handle_,
                                unescaped_path.c_str())) {
      OnSetCurrentDirectory(ERROR_SUCCESS);
    } else {
      ProcessRequestError(GetLastError());
    }
  }
}

void URLRequestFtpJob::OnSetCurrentDirectory(DWORD last_error) {
  DCHECK_EQ(state_, SETTING_CUR_DIRECTORY);

  is_directory_ = (last_error == ERROR_SUCCESS);
  // if last_error is not ERROR_SUCCESS, the requested url is either
  // a file or an invalid path. We optimistically try to read as a file,
  // and if it fails, we fail.
  state_ = FINDING_FIRST_FILE;

  std::string unescaped_path;
  bool is_path_valid = true;
  if (request_->url().has_path()) {
    is_path_valid = UnescapeAndValidatePath(request_, &unescaped_path);
  }
  if (is_path_valid &&
     (request_handle_ = FtpFindFirstFileA(connection_handle_,
                                          unescaped_path.c_str(),
                                          &find_data_, 0,
                                          reinterpret_cast<DWORD_PTR>(this)))) {
    OnFindFirstFile(GetLastError());
  } else {
    ProcessRequestError(GetLastError());
  }
}

void URLRequestFtpJob::FindNextFile() {
  DWORD last_error;
  if (InternetFindNextFileA(request_handle_, &find_data_)) {
    last_error = ERROR_SUCCESS;
  } else {
     last_error = GetLastError();
     // We'll get ERROR_NO_MORE_FILES if the directory is empty.
     if (last_error != ERROR_NO_MORE_FILES) {
       ProcessRequestError(last_error);
       return;
     }
  }
  // Use InvokeLater to call OnFindFile as it ends up calling us, so we don't
  // to blow the stack.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestFtpJob::OnFindFile, last_error));
}

void URLRequestFtpJob::OnFindFirstFile(DWORD last_error) {
  DCHECK_EQ(state_, FINDING_FIRST_FILE);
  if (!is_directory_) {
    // Note that it is not enough to just check !is_directory_ and assume
    // the URL is a file, because is_directory_ is true iff we successfully
    // set current directory to the URL path. Therefore, the URL could just
    // be an invalid path. We proceed optimistically and fail in that case.
    state_ = GETTING_FILE_HANDLE;
    std::string unescaped_path;
    if (UnescapeAndValidatePath(request_, &unescaped_path) &&
        (request_handle_ = FtpOpenFileA(connection_handle_,
                                        unescaped_path.c_str(),
                                        GENERIC_READ,
                                        INTERNET_FLAG_TRANSFER_BINARY,
                                        reinterpret_cast<DWORD_PTR>(this)))) {
      // Start reading file contents
      state_ = GETTING_FILE;
      NotifyHeadersComplete();
    } else {
      ProcessRequestError(GetLastError());
    }
  } else {
    OnStartDirectoryTraversal();
    // If we redirect in OnStartDirectoryTraversal() then this request job
    // is cancelled.
    if (request_handle_)
      OnFindFile(last_error);
  }
}

void URLRequestFtpJob::OnFindFile(DWORD last_error) {
  DCHECK_EQ(state_, GETTING_DIRECTORY);

  if (last_error == ERROR_SUCCESS) {
    // TODO(jabdelmalek): need to add icons for files/folders.
    int64 size =
        (static_cast<unsigned __int64>(find_data_.nFileSizeHigh) << 32) |
        find_data_.nFileSizeLow;

    // We don't know the encoding, and can't assume utf8, so pass the 8bit
    // directly to the browser for it to decide.
    string file_entry = net::GetDirectoryListingEntry(
        find_data_.cFileName, false, size,
        base::Time::FromFileTime(find_data_.ftLastWriteTime));
    WriteData(&file_entry, true);

    FindNextFile();
    return;
  }

  DCHECK(last_error == ERROR_NO_MORE_FILES);
  OnFinishDirectoryTraversal();
}

void URLRequestFtpJob::OnStartDirectoryTraversal() {
  state_ = GETTING_DIRECTORY;

  // Unescape the URL path and pass the raw 8bit directly to the browser.
  string html = net::GetDirectoryListingHeader(
      UnescapeURLComponent(request_->url().path(),
          UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS));

  // If this isn't top level directory (i.e. the path isn't "/",) add a link to
  // the parent directory.
  if (request_->url().path().length() > 1)
    html.append(net::GetDirectoryListingEntry("..", false, 0, base::Time()));

  WriteData(&html, true);

  NotifyHeadersComplete();
}

void URLRequestFtpJob::OnFinishDirectoryTraversal() {
  state_ = DONE;
}

int URLRequestFtpJob::WriteData(const std::string* data,
                                bool call_io_complete) {
  int written = 0;

  if (data && data->length())
    directory_html_.append(*data);

  if (dest_) {
    size_t bytes_to_copy = std::min(static_cast<size_t>(dest_size_),
                                    directory_html_.length());
    if (bytes_to_copy) {
      memcpy(dest_, directory_html_.c_str(), bytes_to_copy);
      directory_html_.erase(0, bytes_to_copy);
      dest_ = NULL;
      dest_size_ = NULL;
      written = static_cast<int>(bytes_to_copy);

      if (call_io_complete) {
        // Wait a little bit before telling the parent class that we wrote
        // data.  This avoids excessive cycles of us getting one file entry and
        // telling the parent class to Read().
        MessageLoop::current()->PostDelayedTask(FROM_HERE, NewRunnableMethod(
                this, &URLRequestFtpJob::ContinueIOComplete, written),
            kFtpBufferTimeMs);
      }
    }
  }

  return written;
}

void URLRequestFtpJob::ContinueIOComplete(int bytes_written) {
  AsyncResult result;
  result.dwResult = bytes_written;
  result.dwError = ERROR_SUCCESS;
  URLRequestInetJob::OnIOComplete(result);
}

void URLRequestFtpJob::ContinueNotifyHeadersComplete() {
  NotifyHeadersComplete();
}

int URLRequestFtpJob::CallInternetRead(char* dest, int dest_size,
                                       int *bytes_read) {
  int result;

  if (is_directory_) {
    // Copy the html that we created from the directory listing that we got
    // from InternetFindNextFile.
    DCHECK(dest_ == NULL);
    dest_ = dest;
    dest_size_ = dest_size;

    DCHECK(state_ == GETTING_DIRECTORY || state_ == DONE);
    int written = WriteData(NULL, false);
    if (written) {
      *bytes_read = written;
      result = ERROR_SUCCESS;
    } else {
      result = state_ == GETTING_DIRECTORY ? ERROR_IO_PENDING : ERROR_SUCCESS;
    }
  } else {
    DWORD bytes_to_read = dest_size;
    bytes_read_ = 0;
    // InternetReadFileEx doesn't work for asynchronous FTP, InternetReadFile
    // must be used instead.
    if (!InternetReadFile(request_handle_, dest, bytes_to_read, &bytes_read_))
      return GetLastError();

    *bytes_read = static_cast<int>(bytes_read_);
    result = ERROR_SUCCESS;
  }

  return result;
}

bool URLRequestFtpJob::GetReadBytes(const AsyncResult& result,
                                    int* bytes_read) {
  if (is_directory_) {
    *bytes_read = static_cast<int>(result.dwResult);
  } else {
    if (!result.dwResult)
      return false;

    // IE5 and later return the number of read bytes in the
    // INTERNET_ASYNC_RESULT structure.  IE4 holds on to the pointer passed in
    // to InternetReadFile and store it there.
    *bytes_read = bytes_read_;

    if (!*bytes_read)
      *bytes_read = result.dwError;
  }

  return true;
}

bool URLRequestFtpJob::IsRedirectResponse(GURL* location,
                                           int* http_status_code) {
  if (is_directory_) {
    std::string ftp_path = request_->url().path();
    if (!ftp_path.empty() && ('/' != ftp_path[ftp_path.length() - 1])) {
      ftp_path.push_back('/');
      GURL::Replacements replacements;
      replacements.SetPathStr(ftp_path);

      *location = request_->url().ReplaceComponents(replacements);
      *http_status_code = 301;  // simulate a permanent redirect
      return true;
    }
  }

  return false;
}
