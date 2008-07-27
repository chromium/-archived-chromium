/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM d:/projects/mozilla_1_8_0_branch/mozilla/modules/plugin/base/public/nsICookieStorage.idl
 */

#ifndef __gen_nsICookieStorage_h__
#define __gen_nsICookieStorage_h__


#ifndef __gen_nsISupports_h__
#include "nsISupports.h"
#endif

#ifndef __gen_nspluginroot_h__
#include "nspluginroot.h"
#endif

/* For IDL files that don't want to include root IDL files. */
#ifndef NS_NO_VTABLE
#define NS_NO_VTABLE
#endif
// {c8c05101-cfdb-11d2-bab8-b088e084e5bc}
#define NS_COOKIESTORAGE_CID \
{ 0xc8c05101, 0xcfdb, 0x11d2, { 0xba, 0xb8, 0xb0, 0x88, 0xe0, 0x84, 0xe5, 0xbc } }

/* starting interface:    nsICookieStorage */
#define NS_ICOOKIESTORAGE_IID_STR "c8c05100-cfdb-11d2-bab8-b088e084e5bc"

#define NS_ICOOKIESTORAGE_IID \
  {0xc8c05100, 0xcfdb, 0x11d2, \
    { 0xba, 0xb8, 0xb0, 0x88, 0xe0, 0x84, 0xe5, 0xbc }}

/**
 * nsICookieStorage
 */
class NS_NO_VTABLE nsICookieStorage : public nsISupports {
 public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICOOKIESTORAGE_IID)

  /**
     * Retrieves a cookie from the browser's persistent cookie store.
	 * @param aCookieURL    - URL string to look up cookie with.
	 * @param aCookieBuffer - buffer large enough to accomodate cookie data.
	 * @param aCookieSize   - on input, size of the cookie buffer, on output cookie's size.
     */
  /* void getCookie (in string aCookieURL, in voidPtr aCookieBuffer, in PRUint32Ref aCookieSize); */
  NS_IMETHOD GetCookie(const char *aCookieURL, void * aCookieBuffer, PRUint32 & aCookieSize) = 0;

  /**
     * Stores a cookie in the browser's persistent cookie store.
   * @param aCookieURL    - URL string store cookie with.
   * @param aCookieBuffer - buffer containing cookie data.
   * @param aCookieSize   - specifies  size of cookie data.
     */
  /* void setCookie (in string aCookieURL, in constVoidPtr aCookieBuffer, in unsigned long aCookieSize); */
  NS_IMETHOD SetCookie(const char *aCookieURL, const void * aCookieBuffer, PRUint32 aCookieSize) = 0;

};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSICOOKIESTORAGE \
  NS_IMETHOD GetCookie(const char *aCookieURL, void * aCookieBuffer, PRUint32 & aCookieSize); \
  NS_IMETHOD SetCookie(const char *aCookieURL, const void * aCookieBuffer, PRUint32 aCookieSize); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_NSICOOKIESTORAGE(_to) \
  NS_IMETHOD GetCookie(const char *aCookieURL, void * aCookieBuffer, PRUint32 & aCookieSize) { return _to GetCookie(aCookieURL, aCookieBuffer, aCookieSize); } \
  NS_IMETHOD SetCookie(const char *aCookieURL, const void * aCookieBuffer, PRUint32 aCookieSize) { return _to SetCookie(aCookieURL, aCookieBuffer, aCookieSize); } 

/* Use this macro to declare functions that forward the behavior of this interface to another object in a safe way. */
#define NS_FORWARD_SAFE_NSICOOKIESTORAGE(_to) \
  NS_IMETHOD GetCookie(const char *aCookieURL, void * aCookieBuffer, PRUint32 & aCookieSize) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetCookie(aCookieURL, aCookieBuffer, aCookieSize); } \
  NS_IMETHOD SetCookie(const char *aCookieURL, const void * aCookieBuffer, PRUint32 aCookieSize) { return !_to ? NS_ERROR_NULL_POINTER : _to->SetCookie(aCookieURL, aCookieBuffer, aCookieSize); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class nsCookieStorage : public nsICookieStorage
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICOOKIESTORAGE

  nsCookieStorage();

private:
  ~nsCookieStorage();

protected:
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsCookieStorage, nsICookieStorage)

nsCookieStorage::nsCookieStorage()
{
  /* member initializers and constructor code */
}

nsCookieStorage::~nsCookieStorage()
{
  /* destructor code */
}

/* void getCookie (in string aCookieURL, in voidPtr aCookieBuffer, in PRUint32Ref aCookieSize); */
NS_IMETHODIMP nsCookieStorage::GetCookie(const char *aCookieURL, void * aCookieBuffer, PRUint32 & aCookieSize)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void setCookie (in string aCookieURL, in constVoidPtr aCookieBuffer, in unsigned long aCookieSize); */
NS_IMETHODIMP nsCookieStorage::SetCookie(const char *aCookieURL, const void * aCookieBuffer, PRUint32 aCookieSize)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif


#endif /* __gen_nsICookieStorage_h__ */
