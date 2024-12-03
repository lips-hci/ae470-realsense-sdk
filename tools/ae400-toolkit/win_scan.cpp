#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <iphlpapi.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>
#include <thread>

#include "win_scan.h"
#include "write_html.hpp"

const int g_timeout_ms = 2000;

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

typedef int socklen_t;
typedef int ssize_t;
const char string_IP_ZERO[16] = "0.0.0.0";
const char string_IP_LOOPBACK[16] = "127.0.0.1";

std::vector<std::string> AE400_IP_Client;
std::vector<std::string> AE400_SN_Client;

bool g_stop_listening = false;

char* format_devinfo(const int i)
{
    char devinfo[256];
    size_t index = AE400_SN_Client[i].rfind( "/" );
    int len = 0;
    len += sprintf( devinfo + len, "IP address  = %s\n", AE400_IP_Client[i].c_str() );
    len += sprintf( devinfo + len, "Version     = %s\n", AE400_SN_Client[i].substr( index + 1 ).c_str() );
    len += sprintf( devinfo + len, "Description = %s\n", AE400_SN_Client[i].substr( 0, index ).c_str() );
    return _strdup( devinfo );
}

void get_ifaddrs_name(const char *ifaddr[], int *ifnum)
{
    /* Declare and initialize variables */

    // It is possible for an adapter to have multiple
    // IPv4 addresses, gateways, and secondary WINS servers
    // assigned to the adapter.
    //
    // Note that this sample code only prints out the
    // first entry for the IP address/mask, and gateway, and
    // the primary and secondary WINS server for each adapter.

    PIP_ADAPTER_INFO pAdapterInfo;
    PIP_ADAPTER_INFO pAdapter = NULL;
    DWORD dwRetVal = 0;
    UINT n = 0;

    ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);
    pAdapterInfo = (IP_ADAPTER_INFO *)malloc(sizeof(IP_ADAPTER_INFO));
    if (pAdapterInfo == NULL) {
        printf("Error allocating memory needed to call GetAdaptersinfo\n");
        return;
    }
    // Make an initial call to GetAdaptersInfo to get
    // the necessary size into the ulOutBufLen variable
    if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW) {
        free(pAdapterInfo);
        pAdapterInfo = (IP_ADAPTER_INFO *)malloc(ulOutBufLen);
        if (pAdapterInfo == NULL) {
            printf("Error allocating memory needed to call GetAdaptersinfo\n");
            return;
        }
    }

    if ((dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen)) == NO_ERROR) {
        pAdapter = pAdapterInfo;

        while (pAdapter) {
        //Bypass adaper type which is NOT "Ethernet"
        if (pAdapter->Type == MIB_IF_TYPE_ETHERNET) {
            //Bypass IP address which is NOT valid (0.0.0.0 or 127.0.0.1)
            if (strcmp(pAdapter->IpAddressList.IpAddress.String, string_IP_ZERO)
                && strcmp(pAdapter->IpAddressList.IpAddress.String, string_IP_LOOPBACK)) {

                /************ DEBUG print Adapter info
                printf("\tComboIndex: \t%d\n", pAdapter->ComboIndex);
                printf("\tAdapter Name: \t%s\n", pAdapter->AdapterName);
                printf("\tAdapter Desc: \t%s\n", pAdapter->Description);
                printf("\tAdapter Addr: \t");
                for (i = 0; i < pAdapter->AddressLength; i++) {
                    if (i == (pAdapter->AddressLength - 1))
                    printf("%.2X\n", (int)pAdapter->Address[i]);
                    else
                    printf("%.2X-", (int)pAdapter->Address[i]);
                }
                printf("\tIndex: \t%d\n", pAdapter->Index);
                printf("\tType: \t");
                printf("\tIP Address: \t%s\n", pAdapter->IpAddressList.IpAddress.String);
                printf("\tIP Mask: \t%s\n", pAdapter->IpAddressList.IpMask.String);
                printf("\tGateway: \t%s\n", pAdapter->GatewayList.IpAddress.String);
                printf("\t***\n");
                **************/
                ifaddr[n++] = _strdup(pAdapter->IpAddressList.IpAddress.String);
            }
        }

        pAdapter = pAdapter->Next;
        //printf("\n"); //for DEBUG print
        }
    }
    else {
        printf("GetAdaptersInfo failed with error: %d\n", dwRetVal);
    }

    if (pAdapterInfo)
        free(pAdapterInfo);

    *ifnum = n;
    return;
}

void udp_broadcast_client(const int timeout_ms = g_timeout_ms)
{
    int yes = 1;
    struct sockaddr_in serv_addr, client_addr;
    socklen_t addrlen = sizeof( client_addr );
    ssize_t size = 0;
    char ip4[INET_ADDR_STR_LEN] = {0}, buf[BUF_STR_LEN] = {0};

    // Initialize Winsock
    WSADATA wsaData;
    int iResult = WSAStartup( MAKEWORD( 2, 2 ), &wsaData );
    if ( iResult != NO_ERROR )
    {
        printf( "WSAStartup failed with error: %d\n", iResult );
        return;
    }

    // Create an IPv4 and UDP socket
    SOCKET sockfd = socket( PF_INET, SOCK_DGRAM, 0 );
    if ( sockfd == INVALID_SOCKET )
    {
        printf( "socket function failed with error: %d\n", WSAGetLastError() );
        WSACleanup();
        return;
    }

    // Set the struct of sockaddr_in
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons( RECV_PORT );
    serv_addr.sin_addr.s_addr = htonl( INADDR_ANY ); // All the host

    if ( setsockopt( sockfd, SOL_SOCKET, SO_REUSEADDR, ( char * )&yes, sizeof( yes ) ) == SOCKET_ERROR )
    {
        printf( "setsockopt for SO_REUSEADDR failed with error: %u\n", WSAGetLastError() );
    }

    if ( setsockopt( sockfd, SOL_SOCKET, SO_RCVTIMEO, ( char * )&timeout_ms, sizeof( int ) ) == SOCKET_ERROR )
    {
        printf( "setsockopt for SO_RCVTIMEO failed with error: %u\n", WSAGetLastError() );
    }

    if ( bind( sockfd, ( struct sockaddr * )&serv_addr, sizeof( struct sockaddr ) ) == SOCKET_ERROR )
    {
        printf( "bind failed with error: %u\n", WSAGetLastError() );
        closesocket( sockfd );
        WSACleanup();
        return;
    }

    while (1)
    {
        if (g_stop_listening)
            break;

        memset(buf, 0, sizeof(buf));
        size = recvfrom(sockfd, buf, BUF_STR_LEN, 0, (struct sockaddr *)&client_addr, &addrlen);
        if (size == SOCKET_ERROR)
        {
            if (WSAGetLastError() == WSAETIMEDOUT)
            {
                //printf( "recvfrom failed with error: Connection timed out.\n" );
            }
            else
            {
                printf("recvfrom failed with error: %d\n", WSAGetLastError());
                continue;
            }
            break;
        }

        if (inet_ntop(AF_INET, &client_addr.sin_addr, ip4, INET_ADDRSTRLEN) == NULL)
        {
            printf("inet_ntop failed with error: %d\n", WSAGetLastError());
            break;
        }

        auto dev = std::find(AE400_IP_Client.begin(), AE400_IP_Client.end(), ip4);
        if (dev == AE400_IP_Client.end())
        {
            AE400_IP_Client.push_back(ip4);
            AE400_SN_Client.push_back(buf);
            int new_found = (int)AE400_IP_Client.size() - 1;
            printf("%s\n", format_devinfo(new_found));
            if (g_output_html)
            {
                write_devinfo_html("device", new_found, g_filehtml);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    closesocket( sockfd );
    WSACleanup();
    return;
}

void _udp_broadcast(const char* ifaddr)
{
    if (ifaddr == NULL)
    {
        return;
    }

    // Initialize Winsock
    WSADATA wsaData;
    int iResult = WSAStartup( MAKEWORD( 2, 2 ), &wsaData );
    if ( iResult != NO_ERROR )
    {
        printf( "WSAStartup failed with error: %d\n", iResult );
        return;
    }

    // Create a socket for sending data
    SOCKET sockfd = socket( PF_INET, SOCK_DGRAM, IPPROTO_UDP );
    if ( sockfd == INVALID_SOCKET )
    {
        printf( "socket failed with error: %d\n", WSAGetLastError() );
        WSACleanup();
        return;
    }

    BOOL bBroadcast = TRUE;
    if ( setsockopt( sockfd, SOL_SOCKET, SO_BROADCAST, ( CHAR * )&bBroadcast, sizeof( BOOL ) ) == SOCKET_ERROR )
    {
        printf( "setsockopt for SO_BROADCAST failed with error: %u\n", WSAGetLastError() );
        closesocket( sockfd );
        WSACleanup();
        return;
    }

    struct sockaddr_in server = {0};
    server.sin_family = AF_INET;
    server.sin_port = htons( SENDER_PORT );
    server.sin_addr.s_addr = inet_addr( ifaddr );

    if ( bind( sockfd, (SOCKADDR *)&server, sizeof(sockaddr_in) ) == SOCKET_ERROR )
    {
        printf( "bind failed with error: %u\n", WSAGetLastError() );
        closesocket( sockfd );
        WSACleanup();
        return;
    }

    struct sockaddr_in client = {0};
    client.sin_family = AF_INET;
    client.sin_port = htons(SENDER_PORT);
    client.sin_addr.s_addr = htonl(INADDR_BROADCAST); // All the host

    if ( sendto( sockfd, "AE400-echo", sizeof("AE400-echo"), 0, (SOCKADDR *)&client, sizeof(sockaddr_in) )  == SOCKET_ERROR )
    {
        printf( "sendto failed with error: %d\n", WSAGetLastError() );
    }

    closesocket( sockfd );
    WSACleanup();
    return;
}

void udp_broadcast()
{
    static const char *ifaddrs[32] = {""};
    static int ifnum = 32;

    if (!strcmp("", ifaddrs[0])) {
        get_ifaddrs_name(ifaddrs, &ifnum);
    }

    //ifnum will be updated to how many of network interface/adapter on this host
    for (int i = 0; i < ifnum; i++) {
        //printf("DEBUG: broadcast from host addr [%s]\n", ifaddrs[i]);
        _udp_broadcast(ifaddrs[i]);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return;
}

void broadcasting(const int run_seconds)
{
    static std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    while(1)
    {
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - begin).count() < run_seconds)
            udp_broadcast();
        else
            break;
    }
}

void listening(const int run_seconds)
{
    static std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    g_stop_listening = false;
    while(1)
    {
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - begin).count() < run_seconds)
        {
            if (!g_stop_listening)
                udp_broadcast_client();
            else
                break;
        }
        else
            break;
    }
    g_stop_listening = true;
}

void show_AE400_info()
{
    AE400_IP_Client.clear();
    AE400_SN_Client.clear();

    printf( "====== Scan Result ======\n\n" );
    std::thread t2(listening, 15);
    std::thread t1(broadcasting, 12);
    t1.join();
    g_stop_listening = true;
    t2.join();
    printf( "====== Scan End    ======\n\n" );
    if (AE400_IP_Client.empty())
    {
        printf( "no device found          \n" );
    }
}

void show_AE400_html()
{
    //HACK: fixup and update final html save path to avoid permission problem
    //      this MUST be done before cmd 'browser'
    const char* temp = getenv("TEMP"); //TEMP is 'C:\Users\xxx\AppData\Local\Temp
    if (temp != NULL) {
        snprintf(g_filehtml, MAX_PATH - 1, "%s\\%s", temp, os_htmlfilepath);
    }

    g_output_html = true;
    write_devinfo_html("header", 0, g_filehtml);

    show_AE400_info();

    write_devinfo_html("ending", 0, g_filehtml);

    //Notice user that no device was found...
    if (!AE400_IP_Client.empty())
    {
        write_devinfo_html("browser", 0, g_filehtml);
    }
    else
    {
        system( "pause" ); //use can take a look at device info on console
    }
}