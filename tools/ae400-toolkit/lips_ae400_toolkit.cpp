#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include "win_scan.h"
bool option_html = true;
#else
#include "linux_scan.h"
bool option_html = false;
#endif

int main( int argc, char *argv[] )
{
    printf( "Usage: %s [-html: use your browser to open html]\n", argv[0]);
    printf( "Scanning your network to find AE4xx devices...\n(this may take 10~20 secs)\n\n" );
    if (argc == 2 && !strncmp(argv[1], "-html", 5))
    {
        option_html = true;
    }

    if (option_html)
        show_AE400_html();
    else
        show_AE400_info();

    exit(0);
}
