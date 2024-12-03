#pragma once

#include <assert.h>
#include <stdio.h>
#include <limits.h>

static bool g_output_html = false;
#ifdef WIN32
#include <process.h>
#define execvp _execvp
#define strdup _strdup
char g_filehtml[MAX_PATH];
const char* os_htmlfilepath = { ".\\lipsedge_devinfo.html" };
const char* g_browser_cmd = { "cmd.exe" };
#else
char g_filehtml[PATH_MAX];
const char* os_htmlfilepath = { "./lipsedge_devinfo.html" };
const char* g_browser_cmd = { "xdg-open" };
#endif

extern std::vector<std::string> AE400_IP_Client;
extern std::vector<std::string> AE400_SN_Client;

char* mystrsep(char** stringp, const char* delim)
{
    char* start = *stringp;
    char* p;

    p = (start != NULL) ? strpbrk(start, delim) : NULL;

    if (p == NULL)
    {
        *stringp = NULL;
    }
    else
    {
        *p = '\0';
        *stringp = p + 1;
    }

    return start;
}

bool guessIsMacAddress(const char* input)
{
    unsigned int m[6];
    int a = sscanf(input, "%x:%x:%x:%x:%x:%x", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]);
    return (a == 6);
}

//input
// cmd = tell function what to do?
//  - "header": write html header part and start table to newly created file specified by filehtml
//  - "device": write the device info at index i to the filehtml
//  - "ending": write html ending part and finish table to the file specified by filehtml
//  - "browser" launch default browser to open filehtml
// i = index to read the vector AE400_SN_Client and AE400_IP_Client
// filehtml = the html filename
//return
// 0 = writing contents to html successful
// 1 = failed to write html file or to open browser
int write_devinfo_html( const char *cmd, const int i, const char *filehtml )
{
    static FILE *fhtml = NULL;
    if (!strcmp(cmd, "header"))
    {
        if (fhtml != NULL)
            fclose(fhtml);

        fhtml = fopen(filehtml, "w");
        if (!fhtml)
        {
            perror("fail to write html");
            return 1;
        }

        fprintf(fhtml, "<!DOCTYPE html><html lang=\"en\">\n");
        fprintf(fhtml, "<head>\n");
        fprintf(fhtml, "<meta charset=\"utf-8\">\n");
        fprintf(fhtml, "<meta http-equiv=\"Content-Type\" content=\"text/xhtml\"/>\n");
        fprintf(fhtml, "<title>LIPSedge Device Management</title>\n");
        fprintf(fhtml, "</head>\n");
        fprintf(fhtml, "<body>\n");
        fprintf(fhtml, "<h3>LIPSedge Device Information</h3>\n");
        fprintf(fhtml, "<hr/>\n");
        fprintf(fhtml, "<style> table{border:1px solid #19779e;} td{border:1px solid #59c3e1;padding:12px;} </style>\n");
        fprintf(fhtml, "<table>\n");

        fprintf(fhtml, "<tr>\n");
        fprintf(fhtml, "<td>IP address</td>\n");
        fprintf(fhtml, "<td>Firmware version</td>\n");
        fprintf(fhtml, "<td>Model name</td>\n");
        fprintf(fhtml, "<td>Device serial number</td>\n");
        fprintf(fhtml, "<td>Camera serial number</td>\n");
        fprintf(fhtml, "<td>MAC address</td>\n");
        fprintf(fhtml, "</tr>\n");

        fprintf(fhtml, "<tr>\n");
    }
    else if (!strcmp(cmd, "device"))
    {
        int index = (int)AE400_SN_Client[i].rfind( "/" );
        fprintf(fhtml, "<td><a href=\"http://%s\" target=\"_blank\">%s</a></td>\n", AE400_IP_Client[i].c_str(), AE400_IP_Client[i].c_str());
        fprintf(fhtml, "<td>%s</td>\n", AE400_SN_Client[i].substr( index + 1 ).c_str());
        char description[1024] = {'\0'};
        strcpy(description, AE400_SN_Client[i].substr( 0, index ).c_str());
        char tokens[4][128] = {"Not available\0","Not available\0","Not available\0","Not available\0"};
        char *token = NULL;
        char *input = description;
        int t = 0;
        while ((token = mystrsep(&input, "/")) != NULL) {
            if (token && t < 4) {
                strcpy(tokens[t++], token);
                if (!strcmp(tokens[t-1], "Not available"))
                    fprintf(fhtml, "<td>%s</td>\n", "Not available");
                else if (!strcmp(tokens[t-1], ""))
                    fprintf(fhtml, "<td>%s</td>\n", "Not available");
                else if (!strcmp(tokens[t-1], "\0") || !strcmp(tokens[t-1], "\n"))
                    fprintf(fhtml, "<td>%s</td>\n", "Not available");
                else if (guessIsMacAddress(tokens[t-1])) {
                    //token contains ":" could be MAC address and print it at tuple 4
                    int j = t-1;
                    while (j++ < 4) {
                        if (j == 4)
                            fprintf(fhtml, "<td>%s</td>\n", tokens[t-1]);
                        else
                            fprintf(fhtml, "<td>%s</td>\n", "Not available");
                    }
                    break;
                } else {
                    fprintf(fhtml, "<td>%s</td>\n", tokens[t-1]);
                }
            }
            token = NULL;
        }
        fprintf(fhtml, "<tr/>\n");
    }
    else if (!strcmp(cmd, "ending"))
    {
        fprintf(fhtml, "</table>\n");
        fprintf(fhtml, "<hr/>\n");
        fprintf(fhtml, "<p>NOTE: click device IP address to open device configuraiton main page.</p>\n");
        fprintf(fhtml, "<p>NOTE: the Model name 'LIPSedge AE4XX' refers to LIPSedge AE430/AE470.</p>\n");
        fprintf(fhtml, "<h4>Copyright © 2024 <a href=\"https://www.lips-hci.com\" target=\"_blank\">LIPS corporation</a></h4>\n");
        fprintf(fhtml, "</body></html>\n");
        fclose(fhtml);
    }
    else if (!strcmp(cmd, "browser"))
    {
#ifdef WIN32
        static char *g_args[] = { strdup(g_browser_cmd), "/c", "start", "/max", strdup(g_filehtml), NULL };
#else
        static char *g_args[] = { strdup(g_browser_cmd), strdup(g_filehtml), NULL };
#endif
        //strcpy(g_args[1], filehtml);
        //sprintf( tmp, "%s", filehtml ); //replace input arg as our filehtml
        if (execvp( g_args[0], g_args ))
        {
            perror("fail to launch default browser");
            return 1;
        }
    }
    else
    {
        perror("write_devinfo_html: invalid args");
        return 1;
    }

    return 0; //function successfuly completed
}