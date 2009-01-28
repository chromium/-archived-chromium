// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_WEB_DATA_SERVICE_H__
#define CHROME_BROWSER_WEBDATA_WEB_DATA_SERVICE_H__

#include <map>

#include "base/basictypes.h"
#include "base/lock.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/thread.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/common/scoped_vector.h"
#include "webkit/glue/autofill_form.h"

#if defined(OS_WIN)
struct IE7PasswordInfo;
#endif
struct PasswordForm;
class FilePath;
class GURL;
class ShutdownTask;
class TemplateURL;

////////////////////////////////////////////////////////////////////////////////
//
// WebDataService is a generic data repository for meta data associated with
// web pages. All data is retrieved and archived in an asynchronous way.
//
// All requests return a handle. The handle can be used to cancel the request.
//
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
//
// WebDataService results
//
////////////////////////////////////////////////////////////////////////////////

//
// Result types
//
typedef enum {
  BOOL_RESULT = 1,       // WDResult<bool>
  KEYWORDS_RESULT,       // WDResult<WDKeywordsResult>
  INT64_RESULT,          // WDResult<int64>
  PASSWORD_RESULT,       // WDResult<std::vector<PasswordForm*>>
  PASSWORD_IE7_RESULT,   // WDResult<IE7PasswordInfo>
  WEB_APP_IMAGES,        // WDResult<WDAppImagesResult>
  AUTOFILL_VALUE_RESULT, // WDResult<std::vector<std::wstring>>
} WDResultType;

// Result from GetWebAppImages.
struct WDAppImagesResult {
  WDAppImagesResult() : has_all_images(false) {}

  // True if SetWebAppHasAllImages(true) was invoked.
  bool has_all_images;

  // The images, may be empty.
  std::vector<SkBitmap> images;
};

struct WDKeywordsResult {
  std::vector<TemplateURL*> keywords;
  // Identifies the ID of the TemplateURL that is the default search. A value of
  // 0 indicates there is no default search provider.
  int64 default_search_provider_id;
  // Version of the builin keywords. A value of 0 indicates a first run.
  int builtin_keyword_version;
};

//
// The top level class for a result.
//
class WDTypedResult {
 public:
  virtual ~WDTypedResult() {}

  // Return the result type.
  WDResultType GetType() const {
    return type_;
  }

 protected:
  WDTypedResult(WDResultType type) : type_(type) {
  }

 private:
  WDResultType type_;
  DISALLOW_EVIL_CONSTRUCTORS(WDTypedResult);
};

// A result containing one specific pointer or literal value.
template <class T> class WDResult : public WDTypedResult {
 public:

  WDResult(WDResultType type, T v) : WDTypedResult(type), value_(v) {
  }

  virtual ~WDResult() {
  }

  // Return a single value result.
  T GetValue() const {
    return value_;
  }

 private:
  T value_;

  DISALLOW_EVIL_CONSTRUCTORS(WDResult);
};

template <class T> class WDObjectResult : public WDTypedResult {
 public:
  explicit WDObjectResult(WDResultType type) : WDTypedResult(type) {
  }

  T* GetValue() const {
    return &value_;
  }

 private:
  // mutable to keep GetValue() const.
  mutable T value_;
  DISALLOW_EVIL_CONSTRUCTORS(WDObjectResult);
};

class WebDataServiceConsumer;

class WebDataService : public base::RefCountedThreadSafe<WebDataService> {
 public:

  // All requests return an opaque handle of the following type.
  typedef int Handle;

  WebDataService();
  ~WebDataService();

  // Initializes the web data service. Returns false on failure
  // Takes the path of the profile directory as its argument.
  bool Init(const FilePath& profile_path);

  // Shutdown the web data service. The service can no longer be used after this
  // call.
  void Shutdown();

  // Returns false if Shutdown() has been called.
  bool IsRunning();

  //////////////////////////////////////////////////////////////////////////////
  //
  // Internal requests
  //
  // Every request is processed using a request object. The object contains
  // both the request parameters and the results.
  //////////////////////////////////////////////////////////////////////////////
  class WebDataRequest {
   public:
    WebDataRequest(WebDataService* service,
                   Handle handle,
                   WebDataServiceConsumer* consumer);

    virtual ~WebDataRequest();

    Handle GetHandle() const;
    WebDataServiceConsumer* GetConsumer() const;
    bool IsCancelled() const;

    // This can be invoked from any thread. From this point we assume that
    // our consumer_ reference is invalid.
    void Cancel();

    // Invoked by the service when this request has been completed.
    // This will notify the service in whatever thread was used to create this
    // request.
    void RequestComplete();

    // The result is owned by the request.
    void SetResult(WDTypedResult* r);
    const WDTypedResult* GetResult() const;

   private:
    scoped_refptr<WebDataService> service_;
    MessageLoop* message_loop_;
    Handle handle_;
    bool canceled_;
    WebDataServiceConsumer* consumer_;
    WDTypedResult* result_;

    DISALLOW_EVIL_CONSTRUCTORS(WebDataRequest);
  };

  //
  // Internally we use instances of the following template to represent
  // requests.
  //
  template <class T>
  class GenericRequest : public WebDataRequest {
   public:
    GenericRequest(WebDataService* service,
                   Handle handle,
                   WebDataServiceConsumer* consumer,
                   T arg) : WebDataRequest(service, handle, consumer),
                            arg_(arg) {
    }

    virtual ~GenericRequest() {
    }

    T GetArgument() {
      return arg_;
    }

   private:
    T arg_;
  };

  template <class T, class U>
  class GenericRequest2 : public WebDataRequest {
   public:
    GenericRequest2(WebDataService* service,
                    Handle handle,
                    WebDataServiceConsumer* consumer,
                    T arg1,
                    U arg2)
        : WebDataRequest(service, handle, consumer),
          arg1_(arg1),
          arg2_(arg2) {
    }

    virtual ~GenericRequest2() { }

    T GetArgument1() {
      return arg1_;
    }

    U GetArgument2() {
      return arg2_;
    }

   private:
    T arg1_;
    U arg2_;
  };

  //////////////////////////////////////////////////////////////////////////////
  //
  // Keywords
  //
  //////////////////////////////////////////////////////////////////////////////

  // As the database processes requests at a later date, all deletion is
  // done on the background thread.
  //
  // Many of the keyword related methods do not return a handle. This is because
  // the caller (TemplateURLModel) does not need to know when the request is
  // done.
  void AddKeyword(const TemplateURL& url);

  void RemoveKeyword(const TemplateURL& url);

  void UpdateKeyword(const TemplateURL& url);

  // Fetches the keywords.
  // On success, consumer is notified with WDResult<std::vector<TemplateURL*>.
  Handle GetKeywords(WebDataServiceConsumer* consumer);

  // Sets the keywords used for the default search provider.
  void SetDefaultSearchProvider(const TemplateURL* url);

  // Sets the version of the builtin keywords.
  void SetBuiltinKeywordVersion(int version);

  //////////////////////////////////////////////////////////////////////////////
  //
  // Web Apps
  //
  //////////////////////////////////////////////////////////////////////////////

  // Sets the image for the specified web app. A web app can have any number of
  // images, but only one at a particular size. If there was an image for the
  // web app at the size of the given image it is replaced.
  void SetWebAppImage(const GURL& app_url, const SkBitmap& image);

  // Sets whether all the images have been downloaded for the specified web app.
  void SetWebAppHasAllImages(const GURL& app_url, bool has_all_images);

  // Removes all images for the specified web app.
  void RemoveWebApp(const GURL& app_url);

  // Fetches the images and whether all images have been downloaded for the
  // specified web app.
  Handle GetWebAppImages(const GURL& app_url, WebDataServiceConsumer* consumer);

  //////////////////////////////////////////////////////////////////////////////
  //
  // Password manager
  //
  //////////////////////////////////////////////////////////////////////////////

  // Updates the remembered password form.
  void UpdateLogin(const PasswordForm& form);

  // Adds |form| to the list of remembered password forms.
  void AddLogin(const PasswordForm& form);

  // Removes |form| from the list of remembered password forms.
  void RemoveLogin(const PasswordForm& form);

  // Removes all logins created in the specified daterange
  void RemoveLoginsCreatedBetween(const base::Time delete_begin,
                                  const base::Time delete_end);

  // Removes all logins created on or after the date passed in.
  void RemoveLoginsCreatedAfter(const base::Time delete_begin);

  // Gets a list of password forms that match |form|.
  // |consumer| will be notified when the request is done. The result is of
  // type WDResult<std::vector<PasswordForm*>>.
  // The result will be null on failure. The |consumer| owns all PasswordForm's.
  Handle GetLogins(const PasswordForm& form, WebDataServiceConsumer* consumer);

  // Gets the complete list of password forms that have not been blacklisted and
  // are thus auto-fillable.
  // |consumer| will be notified when the request is done. The result is of
  // type WDResult<std::vector<PasswordForm*>>.
  // The result will be null on failure.  The |consumer| owns all PasswordForms.
  Handle GetAllAutofillableLogins(WebDataServiceConsumer* consumer);

  // Gets the complete list of password forms.
  // |consumer| will be notified when the request is done. The result is of
  // type WDResult<std::vector<PasswordForm*>>.
  // The result will be null on failure. The |consumer| owns all PasswordForm's.
  Handle GetAllLogins(WebDataServiceConsumer* consumer);

#if defined(OS_WIN)
  // Adds |info| to the list of imported passwords from ie7/ie8.
  void AddIE7Login(const IE7PasswordInfo& info);

  // Removes |info| from the list of imported passwords from ie7/ie8.
  void RemoveIE7Login(const IE7PasswordInfo& info);
 
  // Get the login matching the information in |info|. |consumer| will be
  // notified when the request is done. The result is of type
  // WDResult<IE7PasswordInfo>.
  // If there is no match, the fields of the IE7PasswordInfo will be empty.
  Handle GetIE7Login(const IE7PasswordInfo& info,
                     WebDataServiceConsumer* consumer);
#endif  // defined(OS_WIN)

  // Cancel any pending request. You need to call this method if your
  // WebDataServiceConsumer is about to be deleted.
  void CancelRequest(Handle h);

  //////////////////////////////////////////////////////////////////////////////
  //
  // Autofill.
  //
  //////////////////////////////////////////////////////////////////////////////

  // Schedules a task to add form elements to the web database.
  void AddAutofillFormElements(
      const std::vector<AutofillForm::Element>& elements);

  // Initiates the request for a vector of values which have been entered in
  // form input fields named |name|.  The method OnWebDataServiceRequestDone of
  // |consumer| gets called back when the request is finished, with the vector
  // included in the argument |result|.
  Handle GetFormValuesForElementName(const std::wstring& name,
                                     const std::wstring& prefix,
                                     int limit,
                                     WebDataServiceConsumer* consumer);

  // Removes form elements recorded for autofill from the database.
  void RemoveFormElementsAddedBetween(const base::Time& delete_begin,
                                      const base::Time& delete_end);

 protected:
  friend class TemplateURLModelTest;
  friend class TemplateURLModelTestingProfile;
  friend class WebDataServiceTest;
  friend class WebDataRequest;

  // This is invoked by the unit test; path is the path of the Web Data file.
  bool InitWithPath(const FilePath& path);

  // Invoked by request implementations when a request has been processed.
  void RequestCompleted(Handle h);

  // Register the request as a pending request.
  void RegisterRequest(WebDataRequest* request);

  //////////////////////////////////////////////////////////////////////////////
  //
  // The following methods are only invoked in the web data service thread.
  //
  //////////////////////////////////////////////////////////////////////////////
 private:
  friend class ShutdownTask;

  typedef GenericRequest2<std::vector<const TemplateURL*>,
                          std::vector<TemplateURL*> > SetKeywordsRequest;

  // Initialize the database with the provided path.
  void InitializeDatabase(const FilePath& path);

  // Commit any pending transaction and deletes the database.
  void ShutdownDatabase();

  // Commit the current transaction and creates a new one.
  void Commit();

  //////////////////////////////////////////////////////////////////////////////
  //
  // Keywords.
  //
  //////////////////////////////////////////////////////////////////////////////
  void AddKeywordImpl(GenericRequest<TemplateURL>* request);
  void RemoveKeywordImpl(GenericRequest<TemplateURL::IDType>* request);
  void UpdateKeywordImpl(GenericRequest<TemplateURL>* request);
  void GetKeywordsImpl(WebDataRequest* request);
  void SetDefaultSearchProviderImpl(GenericRequest<TemplateURL::IDType>* r);
  void SetBuiltinKeywordVersionImpl(GenericRequest<int>* r);

  //////////////////////////////////////////////////////////////////////////////
  //
  // Password manager.
  //
  //////////////////////////////////////////////////////////////////////////////
  void AddLoginImpl(GenericRequest<PasswordForm>* request);
  void UpdateLoginImpl(GenericRequest<PasswordForm>* request);
  void RemoveLoginImpl(GenericRequest<PasswordForm>* request);
  void RemoveLoginsCreatedBetweenImpl(
      GenericRequest2<base::Time, base::Time>* request);
  void GetLoginsImpl(GenericRequest<PasswordForm>* request);
  void GetAllAutofillableLoginsImpl(WebDataRequest* request);
  void GetAllLoginsImpl(WebDataRequest* request);
#if defined(OS_WIN)
  void AddIE7LoginImpl(GenericRequest<IE7PasswordInfo>* request);
  void RemoveIE7LoginImpl(GenericRequest<IE7PasswordInfo>* request);
  void GetIE7LoginImpl(GenericRequest<IE7PasswordInfo>* request);
#endif  // defined(OS_WIN)

  //////////////////////////////////////////////////////////////////////////////
  //
  // Autofill.
  //
  //////////////////////////////////////////////////////////////////////////////
  void AddAutofillFormElementsImpl(
      GenericRequest<std::vector<AutofillForm::Element> >* request);
  void GetFormValuesForElementNameImpl(WebDataRequest* request,
      const std::wstring& name, const std::wstring& prefix, int limit);
  void RemoveFormElementsAddedBetweenImpl(
      GenericRequest2<base::Time, base::Time>* request);

  //////////////////////////////////////////////////////////////////////////////
  //
  // Web Apps.
  //
  //////////////////////////////////////////////////////////////////////////////

  void SetWebAppImageImpl(GenericRequest2<GURL,SkBitmap>* request);

  void SetWebAppHasAllImagesImpl(GenericRequest2<GURL,bool>* request);

  void RemoveWebAppImpl(GenericRequest<GURL>* request);

  void GetWebAppImagesImpl(GenericRequest<GURL>* request);

  base::Thread* thread() { return thread_; }

 private:

  // Schedule a task on our worker thread.
  void ScheduleTask(Task* t);

  // Schedule a commit if one is not already pending.
  void ScheduleCommit();

  // Return the next request handle.
  int GetNextRequestHandle();

  // Our worker thread. All requests are processed from that thread.
  base::Thread* thread_;

  // Our database.
  WebDatabase* db_;

  // Whether we should commit the database.
  bool should_commit_;

  // A lock to protect pending requests and next request handle.
  Lock pending_lock_;

  // Next handle to be used for requests. Incremented for each use.
  Handle next_request_handle_;

  typedef std::map<Handle, WebDataRequest*> RequestMap;
  RequestMap pending_requests_;

  DISALLOW_EVIL_CONSTRUCTORS(WebDataService);
};

////////////////////////////////////////////////////////////////////////////////
//
// WebDataServiceConsumer.
//
// All requests to the web data service are asynchronous. When the request has
// been performed, the data consumer is notified using the following interface.
//
////////////////////////////////////////////////////////////////////////////////

class WebDataServiceConsumer {
 public:

  // Called when a request is done. h uniquely identifies the request.
  // result can be NULL, if no result is expected or if the database could
  // not be opened. The result object is destroyed after this call.
  virtual void OnWebDataServiceRequestDone(WebDataService::Handle h,
                                           const WDTypedResult* result) = 0;
};

#endif  // CHROME_BROWSER_WEBDATA_WEB_DATA_SERVICE_H__
