#pragma once

#include <windows.h>
#include <string>

#include <boost/thread/mutex.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>

struct AsyncRequestHandle
{
    DWORD handle;
};

enum AsyncRequestStatus
{
    // < 0 if thread is busy
    // >= 0 if thread is ready
    ASYNC_REQUEST_STATUS_UNINIT       = -2,   // handle is valid but asynchronous request thread is not yet initialized
    ASYNC_REQUEST_STATUS_PENDING      = -1,   // handle is valid and asynchronous request thread is initialized but is still pending
    ASYNC_REQUEST_STATUS_ACCOMPLISH   = 0,    // asynchronous request is finished w/o errors
    ASYNC_REQUEST_STATUS_ABORTED      = 1,    // asynchronous request is aborted in alive thread
    ASYNC_REQUEST_STATUS_CANCELLED    = 254,  // asynchronous request is cancelled, thread associated with the request is terminated
    ASYNC_REQUEST_STATUS_NOT_FOUND    = 255,  // handle is not associated to anyone asynchronous request
};
