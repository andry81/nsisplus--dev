#include "LogonUser.hpp"
#include "nsis_tchar.h"
#include "UserMgr.h"

#include <stdint.h>
#include <list>
#include <algorithm>

#include <boost/functional/hash.hpp>


class LogonAsyncRequestParams
{
public:
	std::string remote;
	std::string user;
	std::string pass;

	LogonAsyncRequestParams(const std::string & remote_, const std::string & user_, const std::string & pass_) :
		remote(remote_),
		user(user_),
		pass(pass_)
	{}
};

std::size_t hash_value(const LogonAsyncRequestParams & params)
{
	std::size_t seed = 0;
	boost::hash_combine(seed, params.remote);
	boost::hash_combine(seed, params.user);
	boost::hash_combine(seed, params.pass);
	return seed;
}

struct LogonAsyncRequestSync
{
	boost::mutex	req_status_mutex; // locks status change

	LogonAsyncRequestSync()
	{}

private:
	// not copyable
	LogonAsyncRequestSync(const LogonAsyncRequestSync &);
	LogonAsyncRequestSync & operator=(const LogonAsyncRequestSync &);
};

typedef boost::shared_ptr<LogonAsyncRequestSync> LogonAsyncRequestSyncPtr;

struct LogonAsyncRequest
{
	LogonAsyncRequestSyncPtr	sync; // synchronization primitives
	AsyncRequestStatus		  req_status;
	DWORD					   req_last_error;
	std::size_t				 req_hash;
	LogonAsyncRequestParams	 req_params;
	LogonAsyncRequestThreadPtr  thread_ptr;
	DWORD					   wnet_error_code;
	char						wnet_error_str[256]; // last, just in case of buffer overflow

	LogonAsyncRequest(std::size_t req_hash_, const LogonAsyncRequestParams & req_params_) :
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
		sync = LogonAsyncRequestSyncPtr(new LogonAsyncRequestSync());
		req_status = ASYNC_REQUEST_STATUS_UNINIT;
		req_last_error = 0;
		wnet_error_code = 0;
		wnet_error_str[0] = '\0';
	}

	void resetThread(const LogonAsyncRequestThreadPtr & thread_ptr);
};

typedef std::list<LogonAsyncRequest> LogonAsyncQueueList;


class LogonAsyncRequestThreadData
{
public:
	LogonAsyncRequest &	 req;
	LogonAsyncRequestParams req_params;

	LogonAsyncRequestThreadData(LogonAsyncRequest & req_) :
		req(req_),
		req_params(req_.req_params)
	{
	}

	void operator()();
};

class LogonAsyncRequestPred
{
public:
	std::size_t req_hash;

	LogonAsyncRequestPred(const LogonAsyncRequestHandle & req_handle) :
		req_hash(req_handle.handle)
	{}

	bool operator()(const LogonAsyncRequest & ref)
	{
		return req_hash == ref.req_hash;
	}
};


boost::mutex g_logon_net_share_async_queue_mutex;
// CAUTION:
//  The container does not have a cleanup procedure, so it will only increase in size!
LogonAsyncQueueList g_logon_net_share_async_queue_list;

boost::mutex g_wnet_error_mutex;
_LogonWNetErrorLocker g_wnet_error_locker;


// unsafe reset
void LogonAsyncRequest::resetThread(const LogonAsyncRequestThreadPtr & thread_ptr_)
{
	thread_ptr = thread_ptr_;
}

LogonAsyncRequestHandle _LogonNetShareAsync(const std::string & remote, const std::string & user, const std::string & pass)
{
	const LogonAsyncRequestParams & req_params = LogonAsyncRequestParams(remote, user, pass);
	const std::size_t req_hash = hash_value(req_params);

	LogonAsyncRequestHandle req_handle = LogonAsyncRequestHandle();
	req_handle.handle = req_hash;

	{
		LogonAsyncRequest * preq = NULL;

		boost::mutex::scoped_lock lock(g_logon_net_share_async_queue_mutex);
		const LogonAsyncQueueList::iterator foundIt = std::find_if(g_logon_net_share_async_queue_list.begin(), g_logon_net_share_async_queue_list.end(), LogonAsyncRequestPred(req_handle));
		if (foundIt != g_logon_net_share_async_queue_list.end()) {
			LogonAsyncRequest & req = *foundIt;
			{
				boost::mutex::scoped_lock lock(req.sync->req_status_mutex);
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
			g_logon_net_share_async_queue_list.push_back(LogonAsyncRequest(req_hash, req_params));
			preq = &g_logon_net_share_async_queue_list.back();
		}

		preq->resetThread(LogonAsyncRequestThreadPtr(new LogonAsyncRequestThread(LogonAsyncRequestThreadData(*preq))));
	}

	return req_handle;
}

void _LockRequestStatus(LogonAsyncRequest & req)
{
	req.sync->req_status_mutex.lock();
}

void _UnlockRequestStatus(LogonAsyncRequest & req)
{
	req.sync->req_status_mutex.unlock();
}

// CAUTION:
//  1. No object unwinding, otherwise __try/__finally won't compile!
//  2. __try/__finally replaces RAII, otherwise C++ code could not be called from pure C code!
void LogonAsyncRequestThreadData::operator()()
{
	boost::this_thread::interruption_point();

	{
		boost::mutex::scoped_lock lock(req.sync->req_status_mutex);
		req.req_status = ASYNC_REQUEST_STATUS_PENDING;
	}

	NETRESOURCEA resource = NETRESOURCEA();
	HANDLE hLogonToken = NULL;

	_LogonWNetErrorLocker wnet_error_locker;

	const DWORD last_error =
		_LogonUser(req_params.remote, req_params.user, req_params.pass, wnet_error_locker, boost::this_thread::interruption_point);

	{
		boost::mutex::scoped_lock lock(req.sync->req_status_mutex);
		switch (last_error) {
			case ERROR_OPERATION_ABORTED:
				req.req_status = ASYNC_REQUEST_STATUS_ABORTED;
			break;

			case NO_ERROR:
			default: // treat all other as accompished
				req.req_status = ASYNC_REQUEST_STATUS_ACCOMPLISH;
		}

		req.req_last_error = last_error;
		req.wnet_error_code = wnet_error_locker.wnet_error_code;
		_tcsnccpy(req.wnet_error_str, wnet_error_locker.wnet_error_str, wnet_error_locker.wnet_error_str_buf_size);
	}
}

DWORD _LogonUser(const std::string & remote, const std::string & user, const std::string & pass, _LogonWNetErrorLocker & wnet_error_locker, void (* interruption_func)())
{
	DWORD result = 0;
	NETRESOURCEA resource;
	LPUSER_INFO_0 ui = 0;
	HANDLE hLogonToken = NULL;

	// buffers at the end
	WCHAR u_remoteid[256];
	WCHAR u_userid[256];

	// drop last error code
	void (* wnet_error_unlock)() = 0;
	DWORD * wnet_error_code = 0;
	char * wnet_error_str = 0;
	DWORD wnet_error_str_buf_size = 0;

	__try {
		wnet_error_locker(0, wnet_error_unlock, wnet_error_code, wnet_error_str, wnet_error_str_buf_size);
	}
	__finally {
		if (wnet_error_unlock) wnet_error_unlock();
	}

	memset(&resource, 0, sizeof(resource));
	resource.dwScope = RESOURCE_GLOBALNET;
	resource.dwType = RESOURCETYPE_ANY;
	resource.dwDisplayType = RESOURCEDISPLAYTYPE_GENERIC;
	resource.dwUsage = RESOURCEUSAGE_CONNECTABLE;

	memset(u_remoteid, 0, sizeof(u_remoteid));
	memset(u_userid, 0, sizeof(u_userid));

	swprintf(u_remoteid, L"%S", remote.c_str());
	swprintf(u_userid, L"%S", user.c_str());

	__try {
		// WARNING:
		//  1. We have to use the call to NetUserGetInfo because in some systems the WNetAddConnection2
		//	 WOULD NOT return any error on the user what actually DOES NOT EXIST BUT EXISTANCE CAN BE CHECKED BY THE NetUserGetInfo.
		//  2. On other hand some systems reports "Access Denided" from the NetUserGetInfo, but seems the WNetAddConnection2
		//	 reports an error in that case (for example, code 1385: Logon failure: the user has not been granted the requested logon type at this computer).
		//  So we have to merge both exclusively successful methods for 2 systems to gain a common success approach for both.
		result = NetUserGetInfo(u_remoteid, u_userid, 0, (LPBYTE *)&ui);

		// Do exit only on specific success return codes:
		//  2221: "The user name could not be found"
		if (result == 2221) {
			return result;
		}

		if (interruption_func) interruption_func();

		// Otherwise try to call the WNetAddConnection2 function...
		__try {
			SetLastError(0); // just in case

			result = ( // to hold resource.lpRemoteName potential destruction until the WNetAddConnection2 return
				resource.lpRemoteName = const_cast<LPSTR>(remote.c_str()),
				WNetAddConnection2(&resource, pass.c_str(), user.c_str(), CONNECT_TEMPORARY)
			);
			if (result == NO_ERROR) {
				// undocumented: for cases where WNetAddConnection2 does not report an error like:
				// "Overlapped I/O operation is in progress"
				result = GetLastError();
			}

			__try {
				wnet_error_locker(1, wnet_error_unlock, wnet_error_code, wnet_error_str, wnet_error_str_buf_size);
				// save wnet error
				WNetGetLastError(wnet_error_code, wnet_error_str, wnet_error_str_buf_size, 0, 0);
			}
			__finally {
				if (wnet_error_unlock) wnet_error_unlock();
			}

			if (interruption_func) interruption_func();

			// continue on "Overllaped I/O operation is in progress"
			if (result != NO_ERROR && result != 997) {
				return result;
			}

			__try {
				SetLastError(0); // just in case

				// just in case call
				LogonUser(user.c_str(),
						remote.c_str(),
						pass.c_str(),
						LOGON32_LOGON_NEW_CREDENTIALS,
						LOGON32_PROVIDER_DEFAULT,
						&hLogonToken);

				result = GetLastError();
			}
			__finally {
				CloseHandle(hLogonToken);
			}
		}
		__finally {
			WNetCancelConnection2(remote.c_str(), 0, TRUE); // might be a race condition if calls from different thread!
		}
	}
	__finally {
		if (ui != NULL) {
			NetApiBufferFree(ui);
			ui = NULL; // just in case
		}
	}

	return result;
}

AsyncRequestStatus _GetLogonNetShareAsyncStatus(LogonAsyncRequestHandle req_handle, DWORD * last_error, DWORD * wnet_error_code, char wnet_error_str[256])
{
	if (last_error) *last_error = NO_ERROR;
	if (wnet_error_code) *wnet_error_code = 0;
	if (wnet_error_str) wnet_error_str[0] = '\0';

	boost::mutex::scoped_lock lock(g_logon_net_share_async_queue_mutex);
	const LogonAsyncQueueList::iterator foundIt = std::find_if(g_logon_net_share_async_queue_list.begin(), g_logon_net_share_async_queue_list.end(), LogonAsyncRequestPred(req_handle));
	if (foundIt == g_logon_net_share_async_queue_list.end()) {
		return ASYNC_REQUEST_STATUS_NOT_FOUND;
	}

	LogonAsyncRequest & req = *foundIt;
	boost::mutex::scoped_lock lock2(req.sync->req_status_mutex);
	if (last_error) *last_error = req.req_last_error;
	if (wnet_error_code) *wnet_error_code = req.wnet_error_code;
	if (wnet_error_str) _tcsnccpy(wnet_error_str, req.wnet_error_str, sizeof(req.wnet_error_str)/sizeof(req.wnet_error_str[0]));

	return req.req_status;
}

LogonAsyncRequestThreadPtr _GetLogonNetShareAsyncRequestThread(LogonAsyncRequestHandle req_handle)
{
	boost::mutex::scoped_lock lock(g_logon_net_share_async_queue_mutex);
	const LogonAsyncQueueList::iterator foundIt = std::find_if(g_logon_net_share_async_queue_list.begin(), g_logon_net_share_async_queue_list.end(), LogonAsyncRequestPred(req_handle));
	if (foundIt == g_logon_net_share_async_queue_list.end()) {
		return NULL;
	}

	return foundIt->thread_ptr;
}

LogonAsyncRequest * _GetLogonNetShareAsyncRequest(LogonAsyncRequestHandle req_handle)
{
	boost::mutex::scoped_lock lock(g_logon_net_share_async_queue_mutex);
	const LogonAsyncQueueList::iterator foundIt = std::find_if(g_logon_net_share_async_queue_list.begin(), g_logon_net_share_async_queue_list.end(), LogonAsyncRequestPred(req_handle));
	if (foundIt == g_logon_net_share_async_queue_list.end()) {
		return NULL;
	}

	return &*foundIt;
}

DWORD _CancelLogonNetShareAsyncRequest(LogonAsyncRequestHandle req_handle)
{
	DWORD result = 0;

	//LogonAsyncRequestThreadPtr thread_ptr = _GetLogonNetShareAsyncRequestThread(req_handle);
	//if (thread_ptr.get() == NULL) {
	//	result = 0x20000000 | 1;
	//}
	//else if (!CancelSynchronousIo(thread_ptr->native_handle())) {
	//	result = GetLastError();
	//}

	// Nothing above has worked, have no choice to just terminate the request thread
	LogonAsyncRequest * preq = _GetLogonNetShareAsyncRequest(req_handle);
	if (preq == NULL) {
		result = 0x20000000 | 1;
	}
	else {
		boost::mutex::scoped_lock lock2(preq->sync->req_status_mutex);
		// terminate only if status is still in busy state
		if(preq->req_status < 0) {
			// update status at first
			preq->req_status = ASYNC_REQUEST_STATUS_CANCELLED;

			HANDLE thread_handle = preq->thread_ptr->native_handle();

			// at first try cancel through Win32 API
			WNetCancelConnection2(preq->req_params.remote.c_str(), 0, TRUE);

			preq->thread_ptr->interrupt(); // just in case
			preq->thread_ptr->detach();

			SetLastError(0); // just in case

			if (!TerminateThread(thread_handle, 0)) {
				result = GetLastError();
			}
		}
	}

	return result;
}
