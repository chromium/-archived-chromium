/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM d:/projects/mozilla_1_8_0_branch/mozilla/modules/plugin/base/public/nsIPluginManager2.idl
 */

#ifndef __gen_nsIPluginManager2_h__
#define __gen_nsIPluginManager2_h__


#ifndef __gen_nsIPluginManager_h__
#include "nsIPluginManager.h"
#endif

/* For IDL files that don't want to include root IDL files. */
#ifndef NS_NO_VTABLE
#define NS_NO_VTABLE
#endif
class nsIPlugin; /* forward declaration */

class nsIEventHandler; /* forward declaration */


/* starting interface:    nsIPluginManager2 */
#define NS_IPLUGINMANAGER2_IID_STR "d2962dc0-4eb6-11d2-8164-006008119d7a"

#define NS_IPLUGINMANAGER2_IID \
  {0xd2962dc0, 0x4eb6, 0x11d2, \
    { 0x81, 0x64, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a }}

/**
 * Plugin Manager 2 Interface
 * These extensions to nsIPluginManager are only available in Communicator 5.0.
 */
class NS_NO_VTABLE nsIPluginManager2 : public nsIPluginManager {
 public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPLUGINMANAGER2_IID)

  /**
   * Puts up a wait cursor.
   *
   * @result - NS_OK if this operation was successful
   */
  /* void beginWaitCursor (); */
  NS_IMETHOD BeginWaitCursor(void) = 0;

  /**
   * Restores the previous (non-wait) cursor.
   *
   * @result - NS_OK if this operation was successful
   */
  /* void endWaitCursor (); */
  NS_IMETHOD EndWaitCursor(void) = 0;

  /**
   * Returns true if a URL protocol (e.g. "http") is supported.
   *
   * @param aProtocol - the protocol name
   * @param aResult   - true if the protocol is supported
   * @result          - NS_OK if this operation was successful
   */
  /* void supportsURLProtocol (in string aProtocol, out boolean aResult); */
  NS_IMETHOD SupportsURLProtocol(const char *aProtocol, PRBool *aResult) = 0;

  /**
   * This method may be called by the plugin to indicate that an error 
   * has occurred, e.g. that the plugin has failed or is shutting down 
   * spontaneously. This allows the browser to clean up any plugin-specific 
   * state.
   *
   * @param aPlugin - the plugin whose status is changing
   * @param aStatus - the error status value
   * @result        - NS_OK if this operation was successful
   */
  /* void notifyStatusChange (in nsIPlugin aPlugin, in nsresult aStatus); */
  NS_IMETHOD NotifyStatusChange(nsIPlugin *aPlugin, nsresult aStatus) = 0;

  /**
   * Returns the proxy info for a given URL. The caller is required to
   * free the resulting memory with nsIMalloc::Free. The result will be in the
   * following format
   * 
   *   i)   "DIRECT"  -- no proxy
   *   ii)  "PROXY xxx.xxx.xxx.xxx"   -- use proxy
   *   iii) "SOCKS xxx.xxx.xxx.xxx"  -- use SOCKS
   *   iv)  Mixed. e.g. "PROXY 111.111.111.111;PROXY 112.112.112.112",
   *                    "PROXY 111.111.111.111;SOCKS 112.112.112.112"....
   *
   * Which proxy/SOCKS to use is determined by the plugin.
   */
  /* void findProxyForURL (in string aURL, out string aResult); */
  NS_IMETHOD FindProxyForURL(const char *aURL, char **aResult) = 0;

  /**
   * Registers a top-level window with the browser. Events received by that
   * window will be dispatched to the event handler specified.
   * 
   * @param aHandler - the event handler for the window
   * @param aWindow  - the platform window reference
   * @result         - NS_OK if this operation was successful
   */
  /* void registerWindow (in nsIEventHandler aHandler, in nsPluginPlatformWindowRef aWindow); */
  NS_IMETHOD RegisterWindow(nsIEventHandler *aHandler, nsPluginPlatformWindowRef aWindow) = 0;

  /**
   * Unregisters a top-level window with the browser. The handler and window pair
   * should be the same as that specified to RegisterWindow.
   *
   * @param aHandler - the event handler for the window
   * @param aWindow  - the platform window reference
   * @result         - NS_OK if this operation was successful
   */
  /* void unregisterWindow (in nsIEventHandler aHandler, in nsPluginPlatformWindowRef aWindow); */
  NS_IMETHOD UnregisterWindow(nsIEventHandler *aHandler, nsPluginPlatformWindowRef aWindow) = 0;

  /**
   * Allocates a new menu ID (for the Mac).
   *
   * @param aHandler   - the event handler for the window
   * @param aIsSubmenu - whether this is a sub-menu ID or not
   * @param aResult    - the resulting menu ID
   * @result           - NS_OK if this operation was successful
   */
  /* void allocateMenuID (in nsIEventHandler aHandler, in boolean aIsSubmenu, out short aResult); */
  NS_IMETHOD AllocateMenuID(nsIEventHandler *aHandler, PRBool aIsSubmenu, PRInt16 *aResult) = 0;

  /**
   * Deallocates a menu ID (for the Mac).
   *
   * @param aHandler - the event handler for the window
   * @param aMenuID  - the menu ID
   * @result         - NS_OK if this operation was successful
   */
  /* void deallocateMenuID (in nsIEventHandler aHandler, in short aMenuID); */
  NS_IMETHOD DeallocateMenuID(nsIEventHandler *aHandler, PRInt16 aMenuID) = 0;

  /**
   * Indicates whether this event handler has allocated the given menu ID.
   *
   * @param aHandler - the event handler for the window
   * @param aMenuID  - the menu ID
   * @param aResult  - returns PR_TRUE if the menu ID is allocated
   * @result         - NS_OK if this operation was successful
   */
  /* void hasAllocatedMenuID (in nsIEventHandler aHandler, in short aMenuID, out boolean aResult); */
  NS_IMETHOD HasAllocatedMenuID(nsIEventHandler *aHandler, PRInt16 aMenuID, PRBool *aResult) = 0;

};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSIPLUGINMANAGER2 \
  NS_IMETHOD BeginWaitCursor(void); \
  NS_IMETHOD EndWaitCursor(void); \
  NS_IMETHOD SupportsURLProtocol(const char *aProtocol, PRBool *aResult); \
  NS_IMETHOD NotifyStatusChange(nsIPlugin *aPlugin, nsresult aStatus); \
  NS_IMETHOD FindProxyForURL(const char *aURL, char **aResult); \
  NS_IMETHOD RegisterWindow(nsIEventHandler *aHandler, nsPluginPlatformWindowRef aWindow); \
  NS_IMETHOD UnregisterWindow(nsIEventHandler *aHandler, nsPluginPlatformWindowRef aWindow); \
  NS_IMETHOD AllocateMenuID(nsIEventHandler *aHandler, PRBool aIsSubmenu, PRInt16 *aResult); \
  NS_IMETHOD DeallocateMenuID(nsIEventHandler *aHandler, PRInt16 aMenuID); \
  NS_IMETHOD HasAllocatedMenuID(nsIEventHandler *aHandler, PRInt16 aMenuID, PRBool *aResult); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_NSIPLUGINMANAGER2(_to) \
  NS_IMETHOD BeginWaitCursor(void) { return _to BeginWaitCursor(); } \
  NS_IMETHOD EndWaitCursor(void) { return _to EndWaitCursor(); } \
  NS_IMETHOD SupportsURLProtocol(const char *aProtocol, PRBool *aResult) { return _to SupportsURLProtocol(aProtocol, aResult); } \
  NS_IMETHOD NotifyStatusChange(nsIPlugin *aPlugin, nsresult aStatus) { return _to NotifyStatusChange(aPlugin, aStatus); } \
  NS_IMETHOD FindProxyForURL(const char *aURL, char **aResult) { return _to FindProxyForURL(aURL, aResult); } \
  NS_IMETHOD RegisterWindow(nsIEventHandler *aHandler, nsPluginPlatformWindowRef aWindow) { return _to RegisterWindow(aHandler, aWindow); } \
  NS_IMETHOD UnregisterWindow(nsIEventHandler *aHandler, nsPluginPlatformWindowRef aWindow) { return _to UnregisterWindow(aHandler, aWindow); } \
  NS_IMETHOD AllocateMenuID(nsIEventHandler *aHandler, PRBool aIsSubmenu, PRInt16 *aResult) { return _to AllocateMenuID(aHandler, aIsSubmenu, aResult); } \
  NS_IMETHOD DeallocateMenuID(nsIEventHandler *aHandler, PRInt16 aMenuID) { return _to DeallocateMenuID(aHandler, aMenuID); } \
  NS_IMETHOD HasAllocatedMenuID(nsIEventHandler *aHandler, PRInt16 aMenuID, PRBool *aResult) { return _to HasAllocatedMenuID(aHandler, aMenuID, aResult); } 

/* Use this macro to declare functions that forward the behavior of this interface to another object in a safe way. */
#define NS_FORWARD_SAFE_NSIPLUGINMANAGER2(_to) \
  NS_IMETHOD BeginWaitCursor(void) { return !_to ? NS_ERROR_NULL_POINTER : _to->BeginWaitCursor(); } \
  NS_IMETHOD EndWaitCursor(void) { return !_to ? NS_ERROR_NULL_POINTER : _to->EndWaitCursor(); } \
  NS_IMETHOD SupportsURLProtocol(const char *aProtocol, PRBool *aResult) { return !_to ? NS_ERROR_NULL_POINTER : _to->SupportsURLProtocol(aProtocol, aResult); } \
  NS_IMETHOD NotifyStatusChange(nsIPlugin *aPlugin, nsresult aStatus) { return !_to ? NS_ERROR_NULL_POINTER : _to->NotifyStatusChange(aPlugin, aStatus); } \
  NS_IMETHOD FindProxyForURL(const char *aURL, char **aResult) { return !_to ? NS_ERROR_NULL_POINTER : _to->FindProxyForURL(aURL, aResult); } \
  NS_IMETHOD RegisterWindow(nsIEventHandler *aHandler, nsPluginPlatformWindowRef aWindow) { return !_to ? NS_ERROR_NULL_POINTER : _to->RegisterWindow(aHandler, aWindow); } \
  NS_IMETHOD UnregisterWindow(nsIEventHandler *aHandler, nsPluginPlatformWindowRef aWindow) { return !_to ? NS_ERROR_NULL_POINTER : _to->UnregisterWindow(aHandler, aWindow); } \
  NS_IMETHOD AllocateMenuID(nsIEventHandler *aHandler, PRBool aIsSubmenu, PRInt16 *aResult) { return !_to ? NS_ERROR_NULL_POINTER : _to->AllocateMenuID(aHandler, aIsSubmenu, aResult); } \
  NS_IMETHOD DeallocateMenuID(nsIEventHandler *aHandler, PRInt16 aMenuID) { return !_to ? NS_ERROR_NULL_POINTER : _to->DeallocateMenuID(aHandler, aMenuID); } \
  NS_IMETHOD HasAllocatedMenuID(nsIEventHandler *aHandler, PRInt16 aMenuID, PRBool *aResult) { return !_to ? NS_ERROR_NULL_POINTER : _to->HasAllocatedMenuID(aHandler, aMenuID, aResult); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class nsPluginManager2 : public nsIPluginManager2
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPLUGINMANAGER2

  nsPluginManager2();

private:
  ~nsPluginManager2();

protected:
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsPluginManager2, nsIPluginManager2)

nsPluginManager2::nsPluginManager2()
{
  /* member initializers and constructor code */
}

nsPluginManager2::~nsPluginManager2()
{
  /* destructor code */
}

/* void beginWaitCursor (); */
NS_IMETHODIMP nsPluginManager2::BeginWaitCursor()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void endWaitCursor (); */
NS_IMETHODIMP nsPluginManager2::EndWaitCursor()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void supportsURLProtocol (in string aProtocol, out boolean aResult); */
NS_IMETHODIMP nsPluginManager2::SupportsURLProtocol(const char *aProtocol, PRBool *aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void notifyStatusChange (in nsIPlugin aPlugin, in nsresult aStatus); */
NS_IMETHODIMP nsPluginManager2::NotifyStatusChange(nsIPlugin *aPlugin, nsresult aStatus)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void findProxyForURL (in string aURL, out string aResult); */
NS_IMETHODIMP nsPluginManager2::FindProxyForURL(const char *aURL, char **aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void registerWindow (in nsIEventHandler aHandler, in nsPluginPlatformWindowRef aWindow); */
NS_IMETHODIMP nsPluginManager2::RegisterWindow(nsIEventHandler *aHandler, nsPluginPlatformWindowRef aWindow)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void unregisterWindow (in nsIEventHandler aHandler, in nsPluginPlatformWindowRef aWindow); */
NS_IMETHODIMP nsPluginManager2::UnregisterWindow(nsIEventHandler *aHandler, nsPluginPlatformWindowRef aWindow)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void allocateMenuID (in nsIEventHandler aHandler, in boolean aIsSubmenu, out short aResult); */
NS_IMETHODIMP nsPluginManager2::AllocateMenuID(nsIEventHandler *aHandler, PRBool aIsSubmenu, PRInt16 *aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void deallocateMenuID (in nsIEventHandler aHandler, in short aMenuID); */
NS_IMETHODIMP nsPluginManager2::DeallocateMenuID(nsIEventHandler *aHandler, PRInt16 aMenuID)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void hasAllocatedMenuID (in nsIEventHandler aHandler, in short aMenuID, out boolean aResult); */
NS_IMETHODIMP nsPluginManager2::HasAllocatedMenuID(nsIEventHandler *aHandler, PRInt16 aMenuID, PRBool *aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif


#endif /* __gen_nsIPluginManager2_h__ */
