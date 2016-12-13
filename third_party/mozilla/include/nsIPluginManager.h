/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM d:/projects/mozilla_1_8_0_branch/mozilla/modules/plugin/base/public/nsIPluginManager.idl
 */

#ifndef __gen_nsIPluginManager_h__
#define __gen_nsIPluginManager_h__


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
#include "nsplugindefs.h"
#define NS_PLUGINMANAGER_CID                         \
{ /* ce768990-5a4e-11d2-8164-006008119d7a */         \
    0xce768990,                                      \
    0x5a4e,                                          \
    0x11d2,                                          \
    {0x81, 0x64, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}
class nsIPluginStreamListener; /* forward declaration */


/* starting interface:    nsIPluginManager */
#define NS_IPLUGINMANAGER_IID_STR "da58ad80-4eb6-11d2-8164-006008119d7a"

#define NS_IPLUGINMANAGER_IID \
  {0xda58ad80, 0x4eb6, 0x11d2, \
    { 0x81, 0x64, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a }}

class nsIPluginManager : public nsISupports {
 public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPLUGINMANAGER_IID)

  /**
     * Returns the value of a variable associated with the plugin manager.
     *
     * (Corresponds to NPN_GetValue.)
     *
     * @param variable - the plugin manager variable to get
     * @param value - the address of where to store the resulting value
     * @result - NS_OK if this operation was successful
     */
  /* [noscript] void GetValue (in nsPluginManagerVariable variable, in nativeVoid value); */
  NS_IMETHOD GetValue(nsPluginManagerVariable variable, void * value) = 0;

  /**
     * Causes the plugins directory to be searched again for new plugin 
     * libraries.
     *
     * (Corresponds to NPN_ReloadPlugins.)
     *
     * @param reloadPages - indicates whether currently visible pages should 
     * also be reloaded
     */
  /* void reloadPlugins (in boolean reloadPages); */
  NS_IMETHOD ReloadPlugins(PRBool reloadPages) = 0;

  /**
     * Returns the user agent string for the browser. 
     *
     * (Corresponds to NPN_UserAgent.)
     *
     * @param resultingAgentString - the resulting user agent string
     */
  /* [noscript] void UserAgent (in nativeChar resultingAgentString); */
  NS_IMETHOD UserAgent(const char * * resultingAgentString) = 0;

    NS_IMETHOD
    GetURL(nsISupports* pluginInst,
           const char* url,
           const char* target = NULL,
           nsIPluginStreamListener* streamListener = NULL,
           const char* altHost = NULL,
           const char* referrer = NULL,
           PRBool forceJSEnabled = PR_FALSE) = 0;
    NS_IMETHOD
    PostURL(nsISupports* pluginInst,
            const char* url,
            PRUint32 postDataLen,
            const char* postData,
            PRBool isFile = PR_FALSE,
            const char* target = NULL,
            nsIPluginStreamListener* streamListener = NULL,
            const char* altHost = NULL,
            const char* referrer = NULL,
            PRBool forceJSEnabled = PR_FALSE,
            PRUint32 postHeadersLength = 0,
            const char* postHeaders = NULL) = 0;
  /**
     * Fetches a URL.
     *
     * (Corresponds to NPN_GetURL and NPN_GetURLNotify.)
     *
     * @param pluginInst - the plugin making the request. If NULL, the URL
     *   is fetched in the background.
     * @param url - the URL to fetch
     * @param target - the target window into which to load the URL, or NULL if
     *   the data should be returned to the plugin via streamListener.
     * @param streamListener - a stream listener to be used to return data to
     *   the plugin. May be NULL if target is not NULL.
     * @param altHost - an IP-address string that will be used instead of the 
     *   host specified in the URL. This is used to prevent DNS-spoofing 
     *   attacks. Can be defaulted to NULL meaning use the host in the URL.
     * @param referrer - the referring URL (may be NULL)
     * @param forceJSEnabled - forces JavaScript to be enabled for 'javascript:'
     *   URLs, even if the user currently has JavaScript disabled (usually 
     *   specify PR_FALSE) 
     * @result - NS_OK if this operation was successful
     */
/**
     * Posts to a URL with post data and/or post headers.
     *
     * (Corresponds to NPN_PostURL and NPN_PostURLNotify.)
     *
     * @param pluginInst - the plugin making the request. If NULL, the URL
     *   is fetched in the background.
     * @param url - the URL to fetch
     * @param postDataLength - the length of postData (if non-NULL)
     * @param postData - the data to POST. NULL specifies that there is not post
     *   data
     * @param isFile - whether the postData specifies the name of a file to 
     *   post instead of data. The file will be deleted afterwards.
     * @param target - the target window into which to load the URL, or NULL if
     *   the data should be returned to the plugin via streamListener.
     * @param streamListener - a stream listener to be used to return data to
     *   the plugin. May be NULL if target is not NULL.
     * @param altHost - an IP-address string that will be used instead of the 
     *   host specified in the URL. This is used to prevent DNS-spoofing 
     *   attacks. Can be defaulted to NULL meaning use the host in the URL.
     * @param referrer - the referring URL (may be NULL)
     * @param forceJSEnabled - forces JavaScript to be enabled for 'javascript:'
     *   URLs, even if the user currently has JavaScript disabled (usually 
     *   specify PR_FALSE) 
     * @param postHeadersLength - the length of postHeaders (if non-NULL)
     * @param postHeaders - the headers to POST. Must be in the form of
     * "HeaderName: HeaderValue\r\n".  Each header, including the last,
     * must be followed by "\r\n".  NULL specifies that there are no
     * post headers
     * @result - NS_OK if this operation was successful
     */
/**
     * Persistently register a plugin with the plugin
     * manager. aMimeTypes, aMimeDescriptions, and aFileExtensions are
     * parallel arrays that contain information about the MIME types
     * that the plugin supports.
     *
     * @param aCID - the plugin's CID
     * @param aPluginName - the plugin's name
     * @param aDescription - a description of the plugin
     * @param aMimeTypes - an array of MIME types that the plugin
     *   is prepared to handle
     * @param aMimeDescriptions - an array of descriptions for the
     *   MIME types that the plugin can handle.
     * @param aFileExtensions - an array of file extensions for
     *   the MIME types that the plugin can handle.
     * @param aCount - the number of elements in the aMimeTypes,
     *   aMimeDescriptions, and aFileExtensions arrays.
     * @result - NS_OK if the operation was successful.
     */
  /* [noscript] void RegisterPlugin (in REFNSIID aCID, in string aPluginName, in string aDescription, in nativeChar aMimeTypes, in nativeChar aMimeDescriptions, in nativeChar aFileExtensions, in long aCount); */
  NS_IMETHOD RegisterPlugin(REFNSIID aCID, const char *aPluginName, const char *aDescription, const char * * aMimeTypes, const char * * aMimeDescriptions, const char * * aFileExtensions, PRInt32 aCount) = 0;

  /**
     * Unregister a plugin from the plugin manager
     *
     * @param aCID the CID of the plugin to unregister.
     * @result - NS_OK if the operation was successful.
     */
  /* [noscript] void UnregisterPlugin (in REFNSIID aCID); */
  NS_IMETHOD UnregisterPlugin(REFNSIID aCID) = 0;

    /**
     * Fetches a URL, with Headers
     * @see GetURL.  Identical except for additional params headers and
     * headersLen
     * @param getHeadersLength - the length of getHeaders (if non-NULL)
     * @param getHeaders - the headers to GET. Must be in the form of
     * "HeaderName: HeaderValue\r\n".  Each header, including the last,
     * must be followed by "\r\n".  NULL specifies that there are no
     * get headers
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    GetURLWithHeaders(nsISupports* pluginInst,
                      const char* url,
                      const char* target = NULL,
                      nsIPluginStreamListener* streamListener = NULL,
                      const char* altHost = NULL,
                      const char* referrer = NULL,
                      PRBool forceJSEnabled = PR_FALSE,
                      PRUint32 getHeadersLength = 0,
                      const char* getHeaders = NULL) = 0;
};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSIPLUGINMANAGER \
  NS_IMETHOD GetValue(nsPluginManagerVariable variable, void * value); \
  NS_IMETHOD ReloadPlugins(PRBool reloadPages); \
  NS_IMETHOD UserAgent(const char * * resultingAgentString); \
  NS_IMETHOD RegisterPlugin(REFNSIID aCID, const char *aPluginName, const char *aDescription, const char * * aMimeTypes, const char * * aMimeDescriptions, const char * * aFileExtensions, PRInt32 aCount); \
  NS_IMETHOD UnregisterPlugin(REFNSIID aCID); \

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_NSIPLUGINMANAGER(_to) \
  NS_IMETHOD GetValue(nsPluginManagerVariable variable, void * value) { return _to GetValue(variable, value); } \
  NS_IMETHOD ReloadPlugins(PRBool reloadPages) { return _to ReloadPlugins(reloadPages); } \
  NS_IMETHOD UserAgent(const char * * resultingAgentString) { return _to UserAgent(resultingAgentString); } \
  NS_IMETHOD RegisterPlugin(REFNSIID aCID, const char *aPluginName, const char *aDescription, const char * * aMimeTypes, const char * * aMimeDescriptions, const char * * aFileExtensions, PRInt32 aCount) { return _to RegisterPlugin(aCID, aPluginName, aDescription, aMimeTypes, aMimeDescriptions, aFileExtensions, aCount); } \
  NS_IMETHOD UnregisterPlugin(REFNSIID aCID) { return _to UnregisterPlugin(aCID); } \

/* Use this macro to declare functions that forward the behavior of this interface to another object in a safe way. */
#define NS_FORWARD_SAFE_NSIPLUGINMANAGER(_to) \
  NS_IMETHOD GetValue(nsPluginManagerVariable variable, void * value) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetValue(variable, value); } \
  NS_IMETHOD ReloadPlugins(PRBool reloadPages) { return !_to ? NS_ERROR_NULL_POINTER : _to->ReloadPlugins(reloadPages); } \
  NS_IMETHOD UserAgent(const char * * resultingAgentString) { return !_to ? NS_ERROR_NULL_POINTER : _to->UserAgent(resultingAgentString); } \
  NS_IMETHOD RegisterPlugin(REFNSIID aCID, const char *aPluginName, const char *aDescription, const char * * aMimeTypes, const char * * aMimeDescriptions, const char * * aFileExtensions, PRInt32 aCount) { return !_to ? NS_ERROR_NULL_POINTER : _to->RegisterPlugin(aCID, aPluginName, aDescription, aMimeTypes, aMimeDescriptions, aFileExtensions, aCount); } \
  NS_IMETHOD UnregisterPlugin(REFNSIID aCID) { return !_to ? NS_ERROR_NULL_POINTER : _to->UnregisterPlugin(aCID); } \

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class nsPluginManager : public nsIPluginManager
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPLUGINMANAGER

  nsPluginManager();

private:
  ~nsPluginManager();

protected:
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsPluginManager, nsIPluginManager)

nsPluginManager::nsPluginManager()
{
  /* member initializers and constructor code */
}

nsPluginManager::~nsPluginManager()
{
  /* destructor code */
}

/* [noscript] void GetValue (in nsPluginManagerVariable variable, in nativeVoid value); */
NS_IMETHODIMP nsPluginManager::GetValue(nsPluginManagerVariable variable, void * value)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void reloadPlugins (in boolean reloadPages); */
NS_IMETHODIMP nsPluginManager::ReloadPlugins(PRBool reloadPages)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] void UserAgent (in nativeChar resultingAgentString); */
NS_IMETHODIMP nsPluginManager::UserAgent(const char * * resultingAgentString)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] void RegisterPlugin (in REFNSIID aCID, in string aPluginName, in string aDescription, in nativeChar aMimeTypes, in nativeChar aMimeDescriptions, in nativeChar aFileExtensions, in long aCount); */
NS_IMETHODIMP nsPluginManager::RegisterPlugin(REFNSIID aCID, const char *aPluginName, const char *aDescription, const char * * aMimeTypes, const char * * aMimeDescriptions, const char * * aFileExtensions, PRInt32 aCount)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] void UnregisterPlugin (in REFNSIID aCID); */
NS_IMETHODIMP nsPluginManager::UnregisterPlugin(REFNSIID aCID)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif


#endif /* __gen_nsIPluginManager_h__ */
