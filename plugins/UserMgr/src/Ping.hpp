#pragma once

#include "AsyncRequest.hpp"
#include <IPExport.h>
#include <icmpapi.h>
#include <Iphlpapi.h>
#include <IpStatus.h>


struct PingAsyncRequestHandle : AsyncRequestHandle
{
};

extern "C" {

#pragma pack(push, 1)
typedef struct _PING_REPLY {
    DWORD num_responces;
    ICMP_ECHO_REPLY icmp_echo;
    char reply[256]; // reply length is equal to input data string length, big enough to fit long responces (236=256-20 - for user data, where 20 - payload for system data responce)
    // see details: "Programmatically using ping (IcmpSendcho2 and IcmpParseReplies)" https://groups.google.com/forum/#!topic/microsoft.public.win32.programmer.networks/Kyi5lKgGdxM
} PING_REPLY;
#pragma pack(pop)

}


struct PingAsyncRequest;
typedef boost::thread PingAsyncRequestThread;
typedef boost::shared_ptr<PingAsyncRequestThread> PingAsyncRequestThreadPtr;

extern PING_REPLY g_ping_reply;

DWORD _Ping(const std::string & remote, const std::string & data, DWORD dwTimeout, PING_REPLY & ping_reply);
PingAsyncRequestHandle _PingAsync(const std::string & remote, const std::string & data, DWORD dwTimeout); // returns hash
AsyncRequestStatus _GetPingAsyncStatus(PingAsyncRequestHandle req_handle, DWORD * last_error, _PING_REPLY * ping_reply);
PingAsyncRequestThreadPtr _GetPingAsyncRequestThread(PingAsyncRequestHandle req_handle);
PingAsyncRequest * _GetPingAsyncRequest(PingAsyncRequestHandle req_handle);
DWORD _CancelPingAsyncRequest(PingAsyncRequestHandle req_handle);
