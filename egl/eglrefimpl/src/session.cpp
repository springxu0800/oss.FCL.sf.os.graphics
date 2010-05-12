// Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
// All rights reserved.
// This component and the accompanying materials are made available
// under the terms of "Eclipse Public License v1.0"
// which accompanies this distribution, and is available
// at the URL "http://www.eclipse.org/legal/epl-v10.html".
//
// Initial Contributors:
// Nokia Corporation - initial contribution.
//
// Contributors:
//
// Description:
// Reference EGL implementation to support EGL sync objects and OpenWF extensions
#include <e32debug.h>
#include "eglprivate.h"

const TInt KEglMajorVersion = 1;
const TInt KEglMinorVersion = 4;

#define KEglClientApis  ""
#define KEglVendor      "Nokia"
#define KEglVersion     "1.4 Reference EGL"
#define KEglExtensions  "EGL_KHR_reusable_sync" /* additonal extensions should be added beginning with a space */ \
                        " EGL_NOK__private__signal_sync"

// Helper macros for repetitive task
//
#define CHECK_FOR_NULL_DISPLAY(handle, retval) \
    if (handle == EGL_NO_DISPLAY) \
        { \
        SetError(EGL_BAD_DISPLAY); \
        return retval; \
        }

#define CHECK_FOR_NULL_SYNCOBJ(handle, retval) \
    if (handle == EGL_NO_SYNC_KHR) \
        { \
        SetError(EGL_BAD_PARAMETER); \
        return retval; \
        }

#define GET_DISPLAY_RETURN_IF_ERR(drv, display, handle, retval) \
    CEglDisplay* display = iDriver.FindDisplay(handle); \
    if (!display) \
        { \
        drv.Unlock(); \
        SetError(EGL_BAD_DISPLAY); \
        return retval; \
        } \
    if (!display->IsInitialized()) \
        { \
        drv.Unlock(); \
        SetError(EGL_NOT_INITIALIZED); \
        return retval; \
        }

#define GET_SYNCOBJ_RETURN_IF_ERR(drv, display, sync, handle, retval) \
    CEglSync* sync = display->FindSyncObj(handle); \
    if (!sync) \
        { \
        drv.Unlock(); \
        SetError(EGL_BAD_PARAMETER); \
        return retval; \
        } \
    if (sync->IsDestroyed()) /* sync obj has been marked as destroyed */ \
        { \
        drv.Unlock(); \
        SetError(EGL_BAD_PARAMETER); \
        return retval; \
        }
                                                                
CEglThreadSession::CEglThreadSession(CEglDriver& aDriver):
	iDriver(aDriver),
	iError(EGL_SUCCESS)
	{
	Dll::SetTls( NULL ); //set TLS to NULL Jose
	}

CEglThreadSession::~CEglThreadSession()
	{
	delete iEgl;
	CEglDriver::Close();
	}

CEglThreadSession* CEglThreadSession::Static()
	{
	CEglThreadSession* es = reinterpret_cast<CEglThreadSession*>(Dll::Tls());
	
	if(es)
		{
		RDebug::Printf(" &&&&&&&&&&&&&&&&& if CEglThreadSession not null,CEglThreadSession Addr of CEglThreadSession &&&&&&&&&&&= %x\n", es);
		}
	if (es)
		{
		return es;
		}

	const TInt err = CEglDriver::Open();
	if (err != KErrNone)
		{
		return NULL;
		}

	// CEglDriver is reference counted. As we successfuly open the driver, pls.iDriver will be non-null
	// and it should be safe to cache the pointer inside CEglThreadSession
	CEglDriver* drv = CEglDriver::GetDriver();
	__ASSERT_DEBUG(drv, User::Panic(KEglPanicCategory, EEglPanicDriverNull));

	// create session object on default thread's heap
	es = new CEglThreadSession(*drv);
	RDebug::Printf("In CEglThreadSession::Static(),CEglThreadSession Addr of CEglThreadSession Ptr = %x\n", &es);
	RDebug::Printf("In CEglThreadSession::Static(),CEglThreadSession Addr of CEglThreadSession= %x\n", es);
	//RDebug::Printf("In CEglThreadSession::Static(),Thread Id %Lu",(RThread().Id().Id()));
	RDebug::Printf("In CEglThreadSession::Static(),Thread Id %Lu",(RThread().Id()));
	if (!es || Dll::SetTls(es)!= KErrNone)
		{
		delete es;
		return NULL;
		}
	
	return es;
	}

void CEglThreadSession::SetError(EGLint aError)
	{
	// EGL spec section 3.1, GetError will return the status of the most recent EGL function call
	// so we will always override the error status
	iError = aError;
	}

EGLint CEglThreadSession::EglGetError()
	{
	// eglGetError always succeed so it will set error state to EGL_SUCCESS
	const EGLint lastError = iError;
	iError = EGL_SUCCESS;
	return lastError;
	}

EGLDisplay CEglThreadSession::EglGetDisplay(NativeDisplayType aDisplayId)
    {
    // EGL spec section 3.2: we do not need to raise EGL error when GetDisplay fails
    SetError(EGL_SUCCESS);

    if (aDisplayId != EGL_DEFAULT_DISPLAY)
        {
        return EGL_NO_DISPLAY;
        }

    // default display is created when driver is initialised the first time and will
    // be destroyed when all threads within process have called eglReleaseThread

    return KEglDefaultDisplayHandle;
    }
void CEglThreadSession::SetEgl(EGL* aEgl)
	{
	iEgl = aEgl;
	}
EGL* CEglThreadSession::getEgl()
	{
	return iEgl;
	}
EGLBoolean CEglThreadSession::EglInitialize(EGLDisplay aDisplay, EGLint* aMajor, EGLint* aMinor)
    {
    SetError(EGL_SUCCESS);
    
    CHECK_FOR_NULL_DISPLAY(aDisplay, EGL_FALSE)

    iDriver.Lock();
    CEglDisplay* display = iDriver.FindDisplay(aDisplay);
    if (!display)
        {
        iDriver.Unlock();
        SetError(EGL_BAD_DISPLAY);
        return EGL_FALSE;
        }

    const TInt err = display->Initialize();
    iDriver.Unlock();
    
    if (err != KErrNone)
        {
        SetError(EGL_NOT_INITIALIZED);
        return EGL_FALSE;
        }
    EGL* pEgl = NULL;
    pEgl = new EGL();
    
    iEgl = pEgl;
    
    RIEGLDisplay* Egldisplay = iEgl->getDisplay(aDisplay);
    //create the current display
    //if a context and a surface are bound by the time of eglTerminate, they remain bound until eglMakeCurrent is called
	RIEGLDisplay* newDisplay = NULL;
	try
	{
		newDisplay = new RIEGLDisplay(aDisplay);	//throws bad_alloc
		iEgl->addDisplay(newDisplay);	//throws bad_alloc
		Egldisplay = newDisplay;
		RI_ASSERT(Egldisplay);
	}
	catch(std::bad_alloc)
	{
		RI_DELETE(newDisplay);
	//	EGL_RETURN(EGL_BAD_ALLOC, EGL_FALSE); //TODO Need to enable this later. Jose
	}
    
    //need to think of deleting egl if anything goes wrong.
    if (aMajor)	
        {
        *aMajor = KEglMajorVersion;
        }
    if (aMinor)
        {
        *aMinor = KEglMinorVersion;
        }

    return EGL_TRUE;
    }

EGLBoolean CEglThreadSession::EglTerminate(EGLDisplay aDisplay)
    {
    SetError(EGL_SUCCESS);
    
    CHECK_FOR_NULL_DISPLAY(aDisplay, EGL_FALSE)

    iDriver.Lock();
    CEglDisplay* display = iDriver.FindDisplay(aDisplay);
    if (!display)
        {
        iDriver.Unlock();
        SetError(EGL_BAD_DISPLAY);
        return EGL_FALSE;
        }

    display->Terminate();
    iDriver.Unlock();
    
    return EGL_TRUE;
    }

TFuncPtrEglProc CEglThreadSession::EglGetProcAddress(const char* aName)
    {
    SetError(EGL_SUCCESS);
        
    if(!aName)
        {
        return NULL;
        }
    
    // EGL spec does not mention about raising error if requested function not found
    // This implementation does not set error and leave thread state unmodified
    return iDriver.GetProcAddress(aName);
    }

const char* CEglThreadSession::EglQueryString(EGLDisplay aDisplay, EGLint aName)
    {
    SetError(EGL_SUCCESS);
    
    const char* str = NULL;

    CHECK_FOR_NULL_DISPLAY(aDisplay, str)

    iDriver.Lock();
    GET_DISPLAY_RETURN_IF_ERR(iDriver, display, aDisplay, str)
    iDriver.Unlock();

    switch (aName)
        {
        case EGL_CLIENT_APIS:
            str = KEglClientApis;
            break;
        case EGL_EXTENSIONS:
            str = KEglExtensions;
            break;
        case EGL_VENDOR:
            str = KEglVendor;
            break;
        case EGL_VERSION:
            str = KEglVersion;
            break;
        default:
            SetError(EGL_BAD_PARAMETER);
            break;
        }

    return str;
    }
EGLBoolean CEglThreadSession::EglGetConfigs(EGLDisplay dpy, EGLConfig *configs,
		                                    EGLint config_size, EGLint *num_config)
	{
	return do_eglGetConfigs(dpy, configs,config_size, num_config);
	}
EGLBoolean CEglThreadSession::EglChooseConfig(EGLDisplay dpy,const EGLint *attrib_list, EGLConfig *configs,
											  EGLint config_size,EGLint *num_config)
	{
	return do_eglChooseConfig(dpy, attrib_list,configs, config_size,num_config);
	}
EGLBoolean CEglThreadSession::EglGetConfigAttrib(EGLDisplay dpy, EGLConfig config,EGLint attribute, 
		                                         EGLint *value)
	{
	return do_eglGetConfigAttrib(dpy, config,attribute, value);
	} 
EGLSurface CEglThreadSession::EglCreateWindowSurface(EGLDisplay dpy,EGLConfig config, 
		                                             EGLNativeWindowType win, const EGLint *attrib_list)
	{
	return do_eglCreateWindowSurface(dpy, config,win,attrib_list);
	}
EGLSurface CEglThreadSession::EglCreatePbufferSurface(EGLDisplay dpy,EGLConfig config, 
		                                              const EGLint *attrib_list)
	{
	return do_eglCreatePbufferSurface(dpy, config,attrib_list);
	}
EGLSurface CEglThreadSession::EglCreatePixmapSurface(EGLDisplay dpy,EGLConfig config, 
		                                             EGLNativePixmapType pixmap, const EGLint *attrib_list)
	{
	return do_eglCreatePixmapSurface(dpy, config,pixmap,attrib_list);
	}
EGLBoolean CEglThreadSession::EglDestroySurface(EGLDisplay dpy, EGLSurface surface)
	{
	return do_eglDestroySurface(dpy, surface);
	}
EGLBoolean CEglThreadSession::EglQuerySurface(EGLDisplay dpy, EGLSurface surface,EGLint attribute,
		                                      EGLint *value)
	{
	return do_eglQuerySurface(dpy, surface,attribute, value);
	}
EGLBoolean CEglThreadSession::EglBindAPI(EGLenum api)
	{
	return do_eglBindAPI(api);
	}
EGLenum CEglThreadSession::EglQueryAPI(void)
	{
	return do_eglQueryAPI();
	}
EGLBoolean CEglThreadSession::EglWaitClient()
	{
	return do_eglWaitClient();
	}
EGLSurface CEglThreadSession::EglCreatePbufferFromClientBuffer(EGLDisplay dpy,EGLenum buftype,EGLClientBuffer buffer, 
		                                                       EGLConfig config,const EGLint *attrib_list)
	{
	return do_eglCreatePbufferFromClientBuffer(dpy, buftype, buffer,config, attrib_list);
	}
EGLBoolean CEglThreadSession::EglSurfaceAttrib(EGLDisplay dpy, EGLSurface surface,
		                                       EGLint attribute, EGLint value)
	{
	return do_eglSurfaceAttrib(dpy, surface,attribute, value);
	}
EGLBoolean CEglThreadSession::EglBindTexImage(EGLDisplay dpy,EGLSurface surface, EGLint buffer)
	{
	return do_eglBindTexImage(dpy, surface, buffer);
	}
EGLBoolean CEglThreadSession::EglReleaseTexImage(EGLDisplay dpy,EGLSurface surface, EGLint buffer)
	{
	return do_eglReleaseTexImage(dpy, surface, buffer);
	}
EGLBoolean CEglThreadSession::EglSwapInterval(EGLDisplay dpy, EGLint interval)
	{
	return do_eglSwapInterval(dpy, interval);
	}
EGLContext CEglThreadSession::EglCreateContext(EGLDisplay dpy, EGLConfig config,EGLContext share_context,
		                                       const EGLint *attrib_list)
	{
	return do_eglCreateContext(dpy, config,share_context,attrib_list);
	}
EGLBoolean CEglThreadSession::EglDestroyContext(EGLDisplay dpy, EGLContext ctx)
	{
	return do_eglDestroyContext(dpy, ctx);
	}
EGLBoolean CEglThreadSession::EglMakeCurrent(EGLDisplay dpy, EGLSurface draw,EGLSurface read, EGLContext ctx)
	{
	return do_eglMakeCurrent(dpy, draw,read, ctx);
	}
EGLContext CEglThreadSession::EglGetCurrentContext()
	{
	return do_eglGetCurrentContext();
	}
EGLSurface CEglThreadSession::EglGetCurrentSurface(EGLint readdraw)
	{
	return do_eglGetCurrentSurface(readdraw);
	}
EGLDisplay CEglThreadSession::EglGetCurrentDisplay()
	{
	return do_eglGetCurrentDisplay();
	}
EGLBoolean CEglThreadSession::EglQueryContext(EGLDisplay dpy, EGLContext ctx,
											  EGLint attribute, EGLint* value)
	{
	return do_eglQueryContext(dpy, ctx,attribute, value);
	}
EGLBoolean CEglThreadSession::EglWaitGL()
	{
	return do_eglWaitGL();
	}
EGLBoolean CEglThreadSession::EglWaitNative(EGLint engine)
	{
	return do_eglWaitNative(engine);
	}
EGLBoolean CEglThreadSession::EglSwapBuffers(EGLDisplay dpy, EGLSurface surface)
	{
	return do_eglSwapBuffers(dpy, surface);
	}
EGLBoolean CEglThreadSession::EglCopyBuffers(EGLDisplay dpy, EGLSurface surface,EGLNativePixmapType target)
	{
	return do_eglCopyBuffers(dpy, surface,target);
	}

EGLSyncKHR CEglThreadSession::EglCreateSyncKhr(EGLDisplay aDisplay, EGLenum aType, const EGLint *aAttribList)
    {
    SetError(EGL_SUCCESS);
    
    CHECK_FOR_NULL_DISPLAY(aDisplay, EGL_NO_SYNC_KHR)

    if (aType != EGL_SYNC_REUSABLE_KHR || (aAttribList && *aAttribList != EGL_NONE))
        {
        SetError(EGL_BAD_ATTRIBUTE);
        return EGL_NO_SYNC_KHR;
        }
    
    iDriver.Lock();
    GET_DISPLAY_RETURN_IF_ERR(iDriver, display, aDisplay, EGL_NO_SYNC_KHR)
    
    CEglSync* syncObj = display->CreateSyncObj();
    iDriver.Unlock();
    
    if (!syncObj)
        {
        SetError(EGL_BAD_ALLOC);
        return EGL_NO_SYNC_KHR;
        }

    return reinterpret_cast<EGLSyncKHR>(syncObj);
    }

EGLBoolean CEglThreadSession::EglDestroySyncKhr(EGLDisplay aDisplay, EGLSyncKHR aSync)
    {
    SetError(EGL_SUCCESS);
    
    CHECK_FOR_NULL_DISPLAY(aDisplay, EGL_FALSE)
    CHECK_FOR_NULL_SYNCOBJ(aSync, EGL_FALSE)

    iDriver.Lock();
    GET_DISPLAY_RETURN_IF_ERR(iDriver, display, aDisplay, EGL_FALSE)
    
    const TInt err = display->DestroySyncObj(aSync);
    iDriver.Unlock();

    if (err != KErrNone)
        {
        SetError(EGL_BAD_PARAMETER);
        return EGL_FALSE;
        }

    return EGL_TRUE;
    }

EGLint CEglThreadSession::EglClientWaitSyncKhr(EGLDisplay aDisplay, EGLSyncKHR aSync, EGLint aFlags, EGLTimeKHR aTimeout)
    {
    SetError(EGL_SUCCESS);
    
    CHECK_FOR_NULL_DISPLAY(aDisplay, EGL_FALSE)
    CHECK_FOR_NULL_SYNCOBJ(aSync, EGL_FALSE)
    
    const EGLint supportedFlags = EGL_SYNC_FLUSH_COMMANDS_BIT_KHR;
    if (aFlags & ~supportedFlags)
        {
        SetError(EGL_BAD_PARAMETER);
        return EGL_FALSE;
        }
    
    iDriver.Lock();
    GET_DISPLAY_RETURN_IF_ERR(iDriver, display, aDisplay, EGL_FALSE)
    GET_SYNCOBJ_RETURN_IF_ERR(iDriver, display, syncObj, aSync, EGL_FALSE)

    // increment refcount to mark this sync obj in use and prevent it from being destroyed when other thread calls eglDestroySyncKHR or eglTerminate
    syncObj->Open();
    
    // release display lock as we're going to wait on sync object after this point, not releasing display lock at this 
    // point will cause deadlock
    iDriver.Unlock();

    // sync obj refcount has been incremented so it won't get destroyed even if other thread call eglDestroySyncKHR or eglTerminate
    // at this point

    // we do not support client apis, so flushing flags will be ignored in this implementation
    EGLint err = syncObj->Wait(aTimeout);

    // decrement refcount
    // sync obj will be destroyted if refcount is 0 e.g. other thread issues eglDestroySyncKHR or eglTerminate while this thread
    // is waiting on it
    iDriver.Lock();
    syncObj->Close();
    iDriver.Unlock();

    // we do not check error here as it is passed back to caller

    return err;
    }

EGLBoolean CEglThreadSession::EglSignalSyncKhr(EGLDisplay aDisplay, EGLSyncKHR aSync, EGLenum aMode)
    {
    const EGLint err = EglSignalSyncInternal(aDisplay, aSync, aMode);
    SetError(err);
    return err == EGL_SUCCESS;
    }

EGLint CEglThreadSession::EglSignalSyncInternal(EGLDisplay aDisplay, EGLSyncKHR aSync, EGLenum aMode)
    {
    if (aDisplay == EGL_NO_DISPLAY)
        {
        return EGL_BAD_DISPLAY;
        }
    if (aSync == EGL_NO_SYNC_KHR)
        {
        return EGL_BAD_PARAMETER;
        }
    if (aMode != EGL_SIGNALED_KHR && aMode != EGL_UNSIGNALED_KHR)
        {
        return EGL_BAD_PARAMETER;
        }

    iDriver.Lock();

    CEglDisplay* display = iDriver.FindDisplay(aDisplay);
    if (!display)
        {
        iDriver.Unlock();
        return EGL_BAD_DISPLAY;
        }
    if (!display->IsInitialized())
        {
        iDriver.Unlock();
        return EGL_NOT_INITIALIZED;
        }

    CEglSync* syncObj = display->FindSyncObj(aSync);
    if (!syncObj)
        {
        iDriver.Unlock();
        return EGL_BAD_PARAMETER;
        }
    if (syncObj->Type() != EGL_SYNC_REUSABLE_KHR)
        {
        iDriver.Unlock();
        return EGL_BAD_MATCH;
        }
    if (syncObj->IsDestroyed()) /* sync obj has been marked as destroyed */
        {
        iDriver.Unlock();
        return EGL_BAD_PARAMETER;
        }
    
    syncObj->Signal(aMode);
    iDriver.Unlock();
    
    return EGL_SUCCESS;
    }

EGLBoolean CEglThreadSession::EglGetSyncAttribKhr(EGLDisplay aDisplay, EGLSyncKHR aSync, EGLint aAttribute, EGLint *aValue)
    {
    SetError(EGL_SUCCESS);
    
    CHECK_FOR_NULL_DISPLAY(aDisplay, EGL_FALSE)
    CHECK_FOR_NULL_SYNCOBJ(aSync, EGL_FALSE)
    
    if (aAttribute != EGL_SYNC_TYPE_KHR && aAttribute != EGL_SYNC_STATUS_KHR)
        {
        SetError(EGL_BAD_ATTRIBUTE);
        return EGL_FALSE;
        }

    if (!aValue)
        {
        SetError(EGL_BAD_PARAMETER);
        return EGL_FALSE;
        }

    iDriver.Lock();

    GET_DISPLAY_RETURN_IF_ERR(iDriver, display, aDisplay, EGL_FALSE)
    GET_SYNCOBJ_RETURN_IF_ERR(iDriver, display, syncObj, aSync, EGL_FALSE)

    if (syncObj->Type() != EGL_SYNC_REUSABLE_KHR)
        {
        iDriver.Unlock();
        SetError(EGL_BAD_MATCH);
        return EGL_FALSE;
        }
    
    switch (aAttribute)
        {
        case EGL_SYNC_TYPE_KHR:
            *aValue = syncObj->Type();
            break;
            
        case EGL_SYNC_STATUS_KHR:
            *aValue = syncObj->Status();
            break;
        }

    iDriver.Unlock();
    
    return EGL_TRUE;
    }

#ifdef _DEBUG
void CEglThreadSession::EglHeapMarkStart()
    {
    iDriver.Lock();
    iDriver.Heap().__DbgMarkStart();
    iDriver.Unlock();
    }

EGLint CEglThreadSession::EglHeapMarkEnd(EGLint aCount)
    {
    iDriver.Lock();
    const TInt cell = iDriver.Heap().__DbgMarkEnd(aCount);
    iDriver.Unlock();
    
    return cell;
    }

void CEglThreadSession::EglHeapSetBurstAllocFail(EGLenum aType, EGLint aRate, EGLint aBurst)
    {
    iDriver.Lock();
    iDriver.Heap().__DbgSetBurstAllocFail(static_cast<RAllocator::TAllocFail>(aType), aRate, aBurst);
    iDriver.Unlock();
    }

#endif //_DEBUG
