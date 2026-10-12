/* OFTPD - the OFTP monitor (started task). Stub. */
#include <stdio.h>

#include "oftp.h"

int
main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("OFTPD %s\n", oftp_version());
    return 0;
}
