#include "Ping.hpp"
//#include "nsis_tchar.h"
#include "UserMgr.h"

#include <stdint.h>
#include <list>
#include <algorithm>

#include <boost/functional/hash.hpp>


class PingAsyncRequestParams
{
public:
    std::string remote;
    std::string data;
    DWORD       dwTimeout;

    PingAsyncRequestParams(const std::string & remote_, const std::string & data_, DWORD dwTimeout_) :
        remote(remote_),
        data(data_),
        dwTimeout(dwTimeout_)
    {}
};

std::size_t hash_value(const PingAsyncRequestParams & params)
{
    std::size_t seed = 0;
    boost::hash_combine(seed, params.remote);
    boost::hash_combine(seed, params.data);
    return seed;
}

struct PingAsyncRequestSync
{
    boost::mutex    req_status_mutex; // locks status change

    PingAsyncRequestSync()
    {}

private:
    // not copyable
    PingAsyncRequestSync(const PingAsyncRequestSync &);
    PingAsyncRequestSync & operator=(const PingAsyncRequestSync &);
};

typedef boost::shared_ptr<PingAsyncRequestSync> PingAsyncRequestSyncPtr;

struct PingAsyncRequest
{
    PingAsyncRequestSyncPtr     sync; // synchronization primitives
    AsyncRequestStatus          req_status;
    DWORD                       req_last_error;
    std::size_t                 req_hash;
    PingAsyncRequestParams      req_params;
    PingAsyncRequestThreadPtr   thread_ptr;
    PING_REPLY                  ping_reply;

    PingAsyncRequest(std::size_t req_hash_, const PingAsyncRequestParams & req_params_) :
        req_hash(req_hash_),
        req_params(req_params_)
    {
        reset();
    }

    void join()
    {
        if (thread_ptr.get()) {
            thread_ptr->join(); // just in case, wait for exit before a new call
        }
    }

    void reset()
    {
        sync = PingAsyncRequestSyncPtr(new PingAsyncRequestSync());
        req_status = ASYNC_REQUEST_STATUS_UNINIT;
        req_last_error = 0;
        memset(&ping_reply, 0, sizeof(ping_reply));
    }

    void resetThread(const PingAsyncRequestThreadPtr & thread_ptr);
};

typedef std::list<PingAsyncRequest> PingAsyncQueueList;


class PingAsyncRequestThreadData
{
public:
    PingAsyncRequest & req;
    PingAsyncRequestParams req_params;

    PingAsyncRequestThreadData(PingAsyncRequest & req_) :
        req(req_),
        req_params(req_.req_params)
    {
    }

    void operator()();
};

class PingAsyncRequestPred
{
public:
    std::size_t req_hash;

    PingAsyncRequestPred(const PingAsyncRequestHandle & req_handle) :
        req_hash(req_handle.handle)
    {}

    bool operator()(const PingAsyncRequest & ref)
    {
        return req_hash == ref.req_hash;
    }
};

// synchronous ping reply
PING_REPLY g_ping_reply = PING_REPLY();

boost::mutex g_ping_async_queue_mutex;
// CAUTION:
//  The container does not have a cleanup procedure, so it will only increase in size!
PingAsyncQueueList g_ping_async_queue_list;


// unsafe reset
void PingAsyncRequest::resetThread(const PingAsyncRequestThreadPtr & thread_ptr_)
{
    thread_ptr = thread_ptr_;
}

PingAsyncRequestHandle _PingAsync(const std::string & remote, const std::string & data, DWORD dwTimeout)
{
    const PingAsyncRequestParams & req_params = PingAsyncRequestParams(remote, data, dwTimeout);
    const std::size_t req_hash = hash_value(req_params);

    PingAsyncRequestHandle req_handle = PingAsyncRequestHandle();
    req_handle.handle = req_hash;

    {
        PingAsyncRequest * preq = NULL;

        boost::mutex::scoped_lock lock(g_ping_async_queue_mutex);
        const PingAsyncQueueList::iterator foundIt = std::find_if(g_ping_async_queue_list.begin(), g_ping_async_queue_list.end(), PingAsyncRequestPred(req_handle));
        if (foundIt != g_ping_async_queue_list.end()) {
            PingAsyncRequest & req = *foundIt;
            {
                boost::mutex::scoped_lock lock2(req.sync->req_status_mutex);
                if (req.req_status < 0) { // is busy
                    return req_handle;
                }

                req.join();
            } // must release lock before it's storage reset

            // reuse old request
            req.reset();
            preq = &req;
        }
        else {
            // create new request
            g_ping_async_queue_list.push_back(PingAsyncRequest(req_hash, req_params));
            preq = &g_ping_async_queue_list.back();
        }

        preq->resetThread(PingAsyncRequestThreadPtr(new PingAsyncRequestThread(PingAsyncRequestThreadData(*preq))));
    }

    return req_handle;
}

void _LockRequestStatus(PingAsyncRequest & req)
{
    req.sync->req_status_mutex.lock();
}

void _UnlockRequestStatus(PingAsyncRequest & req)
{
    req.sync->req_status_mutex.unlock();
}

// CAUTION:
//  1. No object unwinding, otherwise __try/__finally won't compile!
//  2. __try/__finally replaces RAII, otherwise C++ code could not be called from pure C code!
void PingAsyncRequestThreadData::operator()()
{
    boost::this_thread::interruption_point();

    {
        boost::mutex::scoped_lock lock(req.sync->req_status_mutex);
        req.req_status = ASYNC_REQUEST_STATUS_PENDING;
    }

    const DWORD result = _Ping(req_params.remote, req_params.data, req_params.dwTimeout, req.ping_reply);

    boost::this_thread::interruption_point();

    {
        boost::mutex::scoped_lock lock(req.sync->req_status_mutex);
        switch (result) {
            case ERROR_OPERATION_ABORTED:
                req.req_status = ASYNC_REQUEST_STATUS_ABORTED;
            break;

            case NO_ERROR:
            default: // treat all other as accompished
                req.req_status = ASYNC_REQUEST_STATUS_ACCOMPLISH;
        }

        req.req_last_error = result;
    }
}

DWORD _Ping(const std::string & remote, const std::string & data, DWORD dwTimeout, PING_REPLY & ping_reply)
{
    DWORD result = 0;
    HANDLE hIcmpFile = NULL;
    IPAddr ipaddr = INADDR_NONE;

    BOOL error_processed = FALSE;

    size_t dataSize;

    __try {
        ipaddr = inet_addr(remote.c_str());
        if (ipaddr == INADDR_NONE) {
            result = 0x20000000 | 1;
            error_processed = TRUE;
            return result;
        }

        hIcmpFile = IcmpCreateFile();
        if (hIcmpFile == INVALID_HANDLE_VALUE) {
            result = GetLastError();
            error_processed = TRUE;
            return result;
        }

        dataSize = data.size(); // including null character
        if (dataSize > 255-20) dataSize = 255-20; // -20 is undocumented payload!
        if(!(ping_reply.num_responces = IcmpSendEcho(hIcmpFile, ipaddr, const_cast<LPSTR>(data.c_str()), dataSize,
                NULL, &ping_reply.icmp_echo, sizeof(ping_reply.icmp_echo)+dataSize,
                dwTimeout))) {
            result = GetLastError();
            error_processed = TRUE;
            return result;
        }

        //ping_reply.reply[sizeof(ping_reply.reply) - 1] = '\0' ; // safe truncation

        error_processed = TRUE;
    }
    __finally {
        if (!error_processed) {
            result = GetLastError();
        }
        if (hIcmpFile) {
            IcmpCloseHandle(hIcmpFile);
            hIcmpFile = NULL; // just in case
        }
    }

    return result;
}

AsyncRequestStatus _GetPingAsyncStatus(PingAsyncRequestHandle req_handle, DWORD * last_error, _PING_REPLY * ping_reply)
{
    if (last_error) *last_error = NO_ERROR;
    if (ping_reply) *ping_reply = PING_REPLY();

    boost::mutex::scoped_lock lock(g_ping_async_queue_mutex);
    const PingAsyncQueueList::iterator foundIt = std::find_if(g_ping_async_queue_list.begin(), g_ping_async_queue_list.end(), PingAsyncRequestPred(req_handle));
    if (foundIt == g_ping_async_queue_list.end()) {
        return ASYNC_REQUEST_STATUS_NOT_FOUND;
    }

    PingAsyncRequest & req = *foundIt;
    boost::mutex::scoped_lock lock2(req.sync->req_status_mutex);
    if (last_error) *last_error = req.req_last_error;
    if (ping_reply) *ping_reply = req.ping_reply;

    return req.req_status;
}

PingAsyncRequestThreadPtr _GetPingAsyncRequestThread(PingAsyncRequestHandle req_handle)
{
    boost::mutex::scoped_lock lock(g_ping_async_queue_mutex);
    const PingAsyncQueueList::iterator foundIt = std::find_if(g_ping_async_queue_list.begin(), g_ping_async_queue_list.end(), PingAsyncRequestPred(req_handle));
    if (foundIt == g_ping_async_queue_list.end()) {
        return NULL;
    }

    return foundIt->thread_ptr;
}

PingAsyncRequest * _GetPingAsyncRequest(PingAsyncRequestHandle req_handle)
{
    boost::mutex::scoped_lock lock(g_ping_async_queue_mutex);
    const PingAsyncQueueList::iterator foundIt = std::find_if(g_ping_async_queue_list.begin(), g_ping_async_queue_list.end(), PingAsyncRequestPred(req_handle));
    if (foundIt == g_ping_async_queue_list.end()) {
        return NULL;
    }

    return &*foundIt;
}

DWORD _CancelPingAsyncRequest(PingAsyncRequestHandle req_handle)
{
    DWORD result = 0;

    //PingAsyncRequestThreadPtr thread_ptr = _GetPingAsyncRequestThread(req_handle);
    //if (thread_ptr.get() == NULL) {
    //    result = 0x20000000 | 1;
    //}
    //else if (!CancelSynchronousIo(thread_ptr->native_handle())) {
    //    result = GetLastError();
    //}

    // Nothing above has worked, have no choice to just terminate the request thread
    PingAsyncRequest * preq = _GetPingAsyncRequest(req_handle);
    if (preq == NULL) {
        result = 0x20000000 | 1;
    }
    else {
        boost::mutex::scoped_lock lock2(preq->sync->req_status_mutex);
        // terminate only if status is still in busy state
        if(preq->req_status < 0) {
            // update status at first
            preq->req_status = ASYNC_REQUEST_STATUS_CANCELLED;

            preq->thread_ptr->interrupt(); // just in case

            HANDLE thread_handle = preq->thread_ptr->native_handle();
            preq->thread_ptr->detach();
            if (!TerminateThread(thread_handle, 0)) {
                result = GetLastError();
            }
        }
    }

    return result;
}
