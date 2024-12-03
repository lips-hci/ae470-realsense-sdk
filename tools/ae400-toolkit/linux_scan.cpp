#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>
#include <thread>

#include "linux_scan.h"
#include "write_html.hpp"

const int g_timeout_ms = 2000;

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
    return strdup( devinfo );
}

void get_ifaddrs_name(const char *ifaddr[], int *ifnum)
{
    struct ifaddrs *_ifaddr, *ifa;
    int family, s, n = 0;
    char host[NI_MAXHOST];

    if (getifaddrs(&_ifaddr) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    /* Walk through linked list, maintaining head pointer so we
       can free list later */

    for (ifa = _ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;

        family = ifa->ifa_addr->sa_family;

        /* Display interface name and family (including symbolic
           form of the latter for the common families) */

        //printf("%-8s %s (%d)\n",
        //       ifa->ifa_name,
        //       (family == AF_PACKET) ? "AF_PACKET" :
        //       (family == AF_INET) ? "AF_INET" :
        //       (family == AF_INET6) ? "AF_INET6" : "???",
        //       family);

        /* For an AF_INET* interface address, display the address */

        if (family == AF_INET) {
            s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                    host, NI_MAXHOST,
                    NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                printf("getnameinfo() failed: %s\n", gai_strerror(s));
                continue;
            }
            if (!strcmp(host, "127.0.0.1") || !strcmp(host, "0.0.0.0")) {
                continue; //Bypass ZERO or loopback interface
            }
            //printf("%d\tdev:<%s>\t\taddress:<%s>\n", n, ifa->ifa_name, host);
            ifaddr[n++] = strdup(host);
        }
    }

    if (_ifaddr)
        freeifaddrs(_ifaddr);

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
    struct timeval tv;
    fd_set readfds;

    //Create an IPv4 and UDP socket
    int sockfd = socket( PF_INET, SOCK_DGRAM, 0 );
    if ( sockfd < 0 )
    {
        perror( "socket" );
        return;
    }

    // Set the struct of sockaddr_in
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons( RECV_PORT );
    serv_addr.sin_addr.s_addr = htonl( INADDR_ANY ); // All the host

    if ( setsockopt( sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof( yes ) ) < 0 )
    {
        perror( "setsockopt(REUSEADDR)" );
    }

    tv.tv_sec = (timeout_ms / 1000);
    tv.tv_usec = (timeout_ms - (tv.tv_sec*1000))*1000;
    if ( setsockopt( sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof( struct timeval ) ) < 0 )
    {
        perror( "setsockopt(RCVTIMEO)" );
    }

    if ( bind( sockfd, ( struct sockaddr * )&serv_addr, sizeof( struct sockaddr_in ) ) != 0 )
    {
        perror( "bind" );
        close( sockfd );
        return;
    }

    //FD_ZERO( &readfds );
    //FD_SET( sockfd, &readfds );
    //tv.tv_sec = (timeout_ms / 1000);
    //tv.tv_usec = (timeout_ms - (tv.tv_sec*1000))*1000;
    //select( sockfd + 1, &readfds, NULL, NULL, &tv );
    //if ( FD_ISSET( sockfd, &readfds ) ) {
        //code here...
    //}

    while (1)
    {
        if (g_stop_listening)
            break;

        memset(buf, 0, sizeof(buf));
        size = recvfrom(sockfd, buf, BUF_STR_LEN, 0, (struct sockaddr *)&client_addr, &addrlen);
        if ( size < 0 )
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                //printf( "recvfrom failed with error: Connection timed out.\n" );
            }
            else
            {
                perror( "recvfrom" );
                continue;
            }
            break;
        }

        if (inet_ntop(AF_INET, &client_addr.sin_addr, ip4, INET_ADDRSTRLEN) == NULL)
        {
            perror( "inet_ntop" );
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

    close( sockfd );
    return;
}

void _udp_broadcast(const char* hostaddr)
{
    if (hostaddr == NULL)
    {
        return;
    }

    /*Create an IPv4 UDP socket*/
    int sockfd = socket( PF_INET, SOCK_DGRAM, 0 );
    if ( sockfd < 0 )
    {
        perror( "socket" );
        return;
    }

    /*SO_BROADCAST: broadcast attribute*/
    int so_broadcast = 1;
    if ( setsockopt( sockfd, SOL_SOCKET, SO_BROADCAST, &so_broadcast, sizeof( so_broadcast ) ) < 0 )
    {
        perror( "setsockopt" );
        close( sockfd );
        return;
    }

    //struct hostent *he;
    //if ( ( he = gethostbyname(hostaddr) ) == NULL ) {
    //    perror( "gethostbyname" );
    //    close( sockfd );
    //    return;
    //}

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET; /*IPv4*/
    server_addr.sin_port = htons( INADDR_ANY ); /*All the port*/
    server_addr.sin_addr.s_addr = inet_addr(hostaddr); /*Broadcast address*/

    if ( bind( sockfd, ( struct sockaddr* )&server_addr, sizeof( struct sockaddr ) ) != 0 )
    {
        perror( "bind" );
        close( sockfd );
        return;
    }

    struct sockaddr_in client_addr = {0};
    client_addr.sin_family = AF_INET; /*IPv4*/
    client_addr.sin_port = htons( SENDER_PORT ); /*Set port number*/
    client_addr.sin_addr.s_addr = htonl( INADDR_BROADCAST ); /*The broadcast address*/

    /*Use sendto() to send messages to client*/
    /*sendto() doesn't need to be connected*/
    if ( sendto( sockfd, "AE400-echo", sizeof( "AE400-echo" ), 0, ( struct sockaddr* )&client_addr, sizeof( struct sockaddr ) ) < 0 )
    {
        perror( "sendto" );
    }

    close( sockfd );
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
    const char* temp = P_tmpdir; //GNU: this macro is the name of the default directory for temporary files.
    if (temp != NULL) {
        snprintf(g_filehtml, PATH_MAX - 1, "%s/%s", temp, os_htmlfilepath);
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
}