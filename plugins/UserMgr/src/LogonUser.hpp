#pragma once

#include "AsyncRequest.hpp"

struct LogonAsyncRequestHandle : AsyncRequestHandle
{
};

class _LogonWNetErrorLocker
{
public:
    DWORD wnet_error_code;
    char wnet_error_str[256];
    DWORD wnet_error_str_buf_size;

    _LogonWNetErrorLocker()
    {
        wnet_error_code = 0;
        wnet_error_str[0] = '\0';
        wnet_error_str_buf_size = sizeof(wnet_error_str)/sizeof(wnet_error_str[0]);
    }

    virtual void operator()(DWORD stage, void (*& unlock)(), DWORD *& wnet_error_code_, char *& wnet_error_str_, DWORD & wnet_error_str_buf_size_) {
        unlock = 0; // no need to lock/unlock

        wnet_error_code_ = &wnet_error_code;
        wnet_error_str_ = wnet_error_str;
        wnet_error_str_buf_size_ = wnet_error_str_buf_size;
    }
};

extern boost::mutex g_wnet_error_mutex;
extern _LogonWNetErrorLocker g_wnet_error_locker;

struct LogonAsyncRequest;
typedef boost::thread LogonAsyncRequestThread;
typedef boost::shared_ptr<LogonAsyncRequestThread> LogonAsyncRequestThreadPtr;

DWORD _LogonUser(const std::string & remote, const std::string & user, const std::string & pass, _LogonWNetErrorLocker & wnet_error_locker, void (* interruption_func)() = 0);
LogonAsyncRequestHandle _LogonNetShareAsync(const std::string & remote, const std::string & user, const std::string & pass); // returns hash
AsyncRequestStatus _GetLogonNetShareAsyncStatus(LogonAsyncRequestHandle req_handle, DWORD * last_error, DWORD * wnet_error_code, char wnet_error_str[256]);
LogonAsyncRequestThreadPtr _GetLogonNetShareAsyncRequestThread(LogonAsyncRequestHandle req_handle);
LogonAsyncRequest * _GetLogonNetShareAsyncRequest(LogonAsyncRequestHandle req_handle);
DWORD _CancelLogonNetShareAsyncRequest(LogonAsyncRequestHandle req_handle);
