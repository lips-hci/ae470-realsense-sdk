/* License: Apache 2.0. See LICENSE file in root directory.
 * Copyright(c) 2024 LIPS Corporation. All Rights Reserved.
 */

#include <cstdlib>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <signal.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
//#include <thread>
#include <pthread.h>

#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>

using namespace rapidjson;

extern "C" int ntpserver(int argc, const char *argv[]);

//std::atomic<bool> running(true);
const int argsize = 5;
const char *_args[argsize] = {"sntpd", "-d", "-l", "debug", "-n"};

void* runserver(void* data)
{
    printf("running ntp server...\n");
    ntpserver(argsize, _args);
    printf("shutdown ntp server...\n");
    pthread_exit(NULL);
    return NULL;
}

struct MemoryStruct {
  char *memory;
  size_t size;
  MemoryStruct() : memory(nullptr), size(0) {}
  ~MemoryStruct() { if (memory) free(memory); }
};

static size_t
ReadResponse(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = (char*)realloc(mem->memory, mem->size + realsize + 1);
    if(!ptr) {
        /* out of memory! */
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}
static void
CleanResponse(void *userp)
{
    struct MemoryStruct *mem = (struct MemoryStruct*)userp;
    mem->memory = (char*)malloc(1);  /* will be grown as needed by realloc above */
    mem->size = 0;    /* no data at this point */
}

/* Never writes anything, just returns the size presented */
static size_t
DummyWrite(char *ptr, size_t size, size_t nmemb, void *userdata)
{
   return size * nmemb;
}

//return:
// 0 : correct format
// 1 : invalid format
static bool check_ip_format(const char* input_ip)
{
	char buf[20] = {'\0'};
	char nl = 0;		/* charater to validate newline following IP */
	const int QUAD = 4;
	int ip[QUAD] = {0};	/* array to store each quad for testing */

	/* parse with sscanf */
	if (sscanf(input_ip, "%d.%d.%d.%d%c", &ip[0], &ip[1], &ip[2], &ip[3], &nl) == 5)
	{
		/* validate following char is '\n' or '\0' */
		if (nl != '\n' || nl != '\0') {
			printf ("additional characters or trailing whitespaces following IP.\n");
			return 1;
		}
	}
	if (sscanf(input_ip, "%d.%d.%d.%d", &ip[0], &ip[1], &ip[2], &ip[3]) != QUAD)
	{
		printf("invalid IPv4 format.\n");
		return 1;
	}

	for (int i = 0; i < QUAD; i++) {
		/* validate 0-255 */
		if (ip[i] < 0 || 255 < ip[i]) {
			printf("invalid quad '%d'.\n", ip[i]);
			return 1;
		}
	}

	return 0;
}

int main(int argc, char *argv[])
{
    CURL *curl;
    CURLcode res;

    struct MemoryStruct response;
    response.memory = (char*)malloc(1);  /* will be grown as needed by realloc above */
    response.size = 0;    /* no data at this point */

    const char program[] = {"lips-ae4xx-ntpsync"};

    char token[256] = {'0'};
    char deviceip[40] = {'0'};
    char serverip[40] = {'0'}; //ntp server IP
    char *ip;

    int devindex = 0;
    const int MAXDEV = 2;
    const char *devnames[MAXDEV] = { "ae430", "ae400" };

    //LIPS HACK: check if I am running in root priviledge
    if (geteuid() != 0)
    {
        fprintf(stderr, "Need to bind UDP port 123 for NTP server running.\nPlease run as root.\n");
        printf("Usage: %s [DEVICE IP]\n", program);
        exit(1);
    }

    //Read IP address from 1st arg
    if (argc == 2 && check_ip_format(argv[1]) == 0)
    {
        snprintf(deviceip, sizeof(deviceip), "%s", argv[1]);
        printf("Device ip is %s\n", deviceip);
    }
    else
    {
        printf("Usage: %s [DEVICE IP]\n", program);
        exit(0);
    }

    pthread_t t1;
    pthread_create(&t1, NULL, runserver, NULL);

    //FIX: give more time (at least 4s) for thread t1 to test connecting NTP server
    sleep(4);

    /* In windows, this will init the winsock stuff */
    curl_global_init(CURL_GLOBAL_ALL);

    printf("TEST -------- ping -----------\n");
    fflush(stdout);

    bool bfound = false;
    char connectURL[128] = {'0'};
    for (int i = 0; i < MAXDEV; i++)
    {
        curl = curl_easy_init();
        if(curl) {
            //format URL string like "http://192.168.0.100/ae400/dm/ping"
            snprintf(connectURL, sizeof(connectURL), "http://%s/%s/dm/ping", deviceip, devnames[i]);

            curl_easy_setopt(curl, CURLOPT_URL, connectURL);

            /* send all data to this function  */
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ReadResponse);

            CleanResponse((void *)&response);
            /* we pass our 'chunk' struct to the callback function */
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);

            /* complete within 5 seconds */
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

            /* Perform the request, res will get the return code */
            res = curl_easy_perform(curl);

            /* Check for errors */
            if(res != CURLE_OK)
                fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            else
            {
                //printf("PARSER:[%s]\n", response.memory);
                //Example: "code":200,"message":"connect success"}
                if (response.size)
                {
                    int code = 404; //NOT FOUND
                    bool connected = false;

                    Document document;
                    document.Parse((char*)response.memory);

                    if(document.IsObject())
                    {
                        if (document.HasMember("code") && document["code"].IsInt())
                        {
                            code = document["code"].GetInt();
                            //printf("> code = %d\n", code);
                        }
                        if (document.HasMember("message") && document["message"].IsString())
                        {
                            if (!strcmp(document["message"].GetString(), "connect success"))
                                connected = true;
                            //printf("> message = %s\n", document["message"].GetString());
                        }
                    }
                    else
                    {
                        printf("DEBUG: document is not JSON type.\n");
                        //assert(document.IsObject());
                        //it can be HTML document type, try searching:
                        //Not Found, or 404, or NotFoundError: Not Found
                        if (std::string::npos != std::string((char*)response.memory).find("Not Found"))
                            code = 404;
                        if (std::string::npos != std::string((char*)response.memory).find("404"))
                            code = 404;
                        if (std::string::npos != std::string((char*)response.memory).find("NotFoundError"))
                            code = 404;
                    }
                    if( code == 200 && connected )
                    {
                        devindex = i;
                        bfound = true;
                        if (!strcmp(devnames[i], "ae430"))
                            printf("device found : ae470/430\n");
                        else
                            printf("device found : ae400/450\n");
                    }
                    else if ( code == 404 )
                        fprintf(stderr, "device %s is not found, try different model...\n", devnames[i]);
                    else
                        fprintf(stderr, "Cannot parse response:[%s]\n", response.memory);
                }
            }

            if (bfound)
                break; //we have connected the device, exit loop

            /* always cleanup */
            curl_easy_cleanup(curl);
        }
    }

    printf("\n\nTEST -------- binding -----------\n");
    fflush(stdout);

    curl = curl_easy_init();
    if(curl) {
        //format URL like "http://192.168.0.100/ae400/dm/binding"
        snprintf(connectURL, sizeof(connectURL), "http://%s/%s/dm/binding", deviceip, devnames[devindex]);
        curl_easy_setopt(curl, CURLOPT_URL, connectURL);

        struct curl_slist *chunk = NULL;
        /* Remove a header curl would otherwise add by itself */
        chunk = curl_slist_append(chunk, "accept: application/json");
        chunk = curl_slist_append(chunk, "Content-Type: application/json");

        /* set our custom set of headers */
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

        /* Now specify the POST data */
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{ \"user\": \"admin\", \"password\": \"LIPS\" }");

        /* send all data to this function  */
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ReadResponse);

        CleanResponse((void *)&response);
        /* we pass our 'chunk' struct to the callback function */
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);

        /* complete within 10 seconds */
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);

        /* Check for errors */
        if(res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        else
        {
            //printf("PARSER:[%s]\n", response.memory);

            // CURLINFO_LOCAL_IP - get local IP address of last connection
            if (res == CURLE_OK && !curl_easy_getinfo(curl, CURLINFO_LOCAL_IP, &ip) && ip && *ip)
            {
                strncpy(serverip, ip, 16);
                printf("Get Local IP: %s\n", serverip);
            }
            else
            {
                curl_easy_cleanup(curl);

                // Workaround: make a new connection to 'pool.ntp.org' to get server's IP but device needs to access internet
                curl = curl_easy_init();
                if(curl) {
                    curl_easy_setopt(curl, CURLOPT_URL, "pool.ntp.org");

                    res = curl_easy_perform(curl);

                    if(res == CURLE_OK && !curl_easy_getinfo(curl, CURLINFO_PRIMARY_IP, &ip) && ip && *ip)
                    {
                        strncpy(serverip, ip, 16);
                        printf("Get NTP Server (pool.ntp.org) IP: %s\n", serverip);
                    }
                    else
                    {
                        ip = strdup("216.239.35.0"); //try time1.google.com
                        strncpy(serverip, ip, 16);
                        printf("Get NTP Server (time1.google.com) IP: %s\n", serverip);
                    }
                }
            }
        }

        /* always cleanup */
        curl_easy_cleanup(curl);
    }

    printf("\n\nTEST -------- parsing -----------\n");
    fflush(stdout);

    if (response.size)
    {
        int code = 403; //Forbidden
        bool login = false;

        Document document;
        document.Parse((char*)response.memory);

        if(document.IsObject())
        {
            if (document.HasMember("code") && document["code"].IsInt())
            {
                code = document["code"].GetInt();
                //printf("> code = %d\n", code);
            }
            if (document.HasMember("message") && document["message"].IsString())
            {
                if (!strcmp(document["message"].GetString(), "login success"))
                    login = true;
                //printf("> message = %s\n", document["message"].GetString());
            }
        }
        else
        {
            printf("DEBUG: document is not object type.\nfull message text is:[%s]\n", (char*)response.memory);
            //assert(document.IsObject());
        }
        if(code == 200 && login)
        {
            if (document.HasMember("token") && document["token"].IsString())
            {
                strcpy(token, document["token"].GetString());
                //printf("DEBUG: token = [%s](%lu)\n", token, strlen(token));
            }
        }
        else if (code == 403)
            fprintf(stderr, "operation is not permitted.\n");
        else
            fprintf(stderr, "Cannot parse response:[%s]\n", response.memory);
    }

    printf("\n\nTEST -------- ntp sync -----------\n");
    fflush(stdout);

    curl = curl_easy_init();
    if(curl) {
        //format URL like "http://192.168.0.100/ntp"
        snprintf(connectURL, sizeof(connectURL), "http://%s/ntp", deviceip);

        int loop = 3;
        while (loop--) {
            curl_easy_setopt(curl, CURLOPT_URL, connectURL);

            struct curl_slist *chunk = NULL;

            /* Remove a header curl would otherwise add by itself */
            chunk = curl_slist_append(chunk, "Content-Type: application/json");

            //char bearer_token[300] = {"Authorization: Bearer "};
            //strcat(bearer_token, token);
            //chunk = curl_slist_append(chunk, bearer_token);

            /* set our custom set of headers */
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

            //curl_easy_setopt(curl, CURLOPT_XOAUTH2_BEARER, token);

            char post_ntp_address[128] = {};
            snprintf(post_ntp_address, sizeof(post_ntp_address), "{ \"inputNTPAddress\": \"%s\" }", serverip);
            /* Now specify the POST data */
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_ntp_address);

            CleanResponse((void *)&response);
            /* send all data to this function  */
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ReadResponse);

            /* we pass our 'chunk' struct to the callback function */
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);

            /* complete within 10 seconds */
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

            /* Perform the request, res will get the return code */
            res = curl_easy_perform(curl);

            /* Check for errors */
            if(res != CURLE_OK)
                fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            else
            {
                //printf("PARSER:[%s]\n", response.memory);
                //LIPS HACK: examing return message to check ntp sync result
                // PASS example:
                //   Connect NTP server success and date has been adjusted 9/4/2024, 5:17:50 AM
                // NG example:
                //   fail: Error: Command failed: /bin/sh -c sudo ntpdate undefined
                //   Exiting, name server cannot be used: Temporary failure in name resolution (-3)
                //   4 Sep 05:14:54 ntpdate[2378]: name server cannot be used: Temporary failure in name resolution (-3)
                std::string msgbody(response.memory);
                std::size_t found = msgbody.find("Connect NTP server success and date has been adjusted");
                if (found != std::string::npos)
                {
                    printf("ntp sync is successful!\ndevice response: ");
                    printf("[%s]\n", msgbody.substr(found, 80).c_str());
                    break; //leaving ntp sync loop
                }
                found = msgbody.find("fail: Error: Command failed:");
                if (found != std::string::npos)
                {
                    printf("ntp sync failed!\ndevice response: ");
                    printf("[%s]\n", msgbody.substr(found, 180).c_str());
                }
            }
            printf("\nkeep trying (%d more retry left)...\n", loop);
        }

        /* always cleanup */
        curl_easy_cleanup(curl);
    }

    printf("\n\nTEST -------- ending -----------\n");
    fflush(stdout);

    pthread_detach(t1);
    pthread_kill(t1, SIGINT);
    //running.store(false);
    //pthread_join(t1, NULL);

    //windows only
#if(WIN32 || _WIN32)
    system("pause");
#endif
    return 0;
}
