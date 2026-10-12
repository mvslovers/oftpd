#include <buildstamp.h>

#include "oftp.h"

const char *
oftp_version(void)
{
    return MBT_PROJECT " " MBT_VERSION " (" MBT_COMMIT ")";
}
