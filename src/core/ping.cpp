#include "ping.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

namespace ok::ping {

Result measure(const wstring& host) {
    Result r;
    char hostA[256] = {};
    WideCharToMultiByte(CP_ACP, 0, host.c_str(), -1, hostA, 256, nullptr, nullptr);

    // winsock must be initialised for the resolver
    static bool wsaInit = false;
    if (!wsaInit) {
        WSADATA wd;
        if (WSAStartup(MAKEWORD(2, 2), &wd) == 0) wsaInit = true;
    }

    IPAddr dest = INADDR_NONE;
    {
        IN_ADDR a{};
        if (InetPtonA(AF_INET, hostA, &a) == 1) {
            dest = a.S_un.S_addr;
        } else {
            ADDRINFOA hints{};
            hints.ai_family = AF_INET;
            ADDRINFOA* res = nullptr;
            if (getaddrinfo(hostA, nullptr, &hints, &res) == 0 && res) {
                dest = ((sockaddr_in*)res->ai_addr)->sin_addr.S_un.S_addr;
                freeaddrinfo(res);
            } else {
                r.status = L"could not resolve host";
                return r;
            }
        }
    }

    HANDLE icmp = IcmpCreateFile();
    if (icmp == INVALID_HANDLE_VALUE) { r.status = L"IcmpCreateFile failed"; return r; }

    char payload[32];
    memset(payload, 'A', sizeof(payload));
    const WORD size = sizeof(payload);
    const WORD timeout = 1200; // ms per echo
    const int tries = 4;

    unsigned char replyBuf[sizeof(ICMP_ECHO_REPLY) + 40] = {};

    int hits = 0;
    DWORD totalRtt = 0;
    for (int i = 0; i < tries; ++i) {
        DWORD ret = IcmpSendEcho(icmp, dest, payload, size, nullptr, replyBuf, sizeof(replyBuf), timeout);
        if (ret) {
            PICMP_ECHO_REPLY rep = (PICMP_ECHO_REPLY)replyBuf;
            if (rep->Status == IP_SUCCESS) {
                hits++;
                totalRtt += rep->RoundTripTime;
            }
        }
        Sleep(120);
    }
    IcmpCloseHandle(icmp);

    if (hits == 0) { r.status = L"request timed out"; return r; }
    r.ok = true;
    r.ms = totalRtt / hits;
    r.status = L"avg of " + std::to_wstring(hits) + L" replies";
    return r;
}

} // namespace ok::ping
