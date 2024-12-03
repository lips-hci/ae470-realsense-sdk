/* License: Apache 2.0. See LICENSE file in root directory.
 * Copyright(c) 2024 LIPS Corporation. All Rights Reserved.
 */

#include <cstdlib>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include <fstream>
#include <sstream>

#include <librealsense2/lips_ae4xx_dm.h> // ae4xx_restart()

int main(int argc, char **argv)
{
    char ip[16] = "192.168.0.100";

    if (argc == 2)
    {
        strncpy(ip, argv[1], sizeof(ip));
        ip[15] = '\0';
    }
    else
    {
        printf("Usage: %s [ip address]\n", argv[0]);
        return 1;
    }

    int err = ae4xx_restart("ae430", ip);

    printf("\nRestarting camera (ip = [%s]) ...", ip);
    if (err == 0) {
        printf("okay\n");
        printf("\nPlease monitor LED indicator on the camera and wait device boot up\n\n");
    } else {
        printf("failed\n");
    }

#if defined(WIN32)
    //windows only
    system("pause");
#endif

    return 0;
}