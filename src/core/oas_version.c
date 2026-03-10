#include <liboas/oas_version.h>

const char *oas_version(void)
{
    return OAS_VERSION_STRING;
}

int oas_version_number(void)
{
    return OAS_VERSION_MAJOR * 10000 + OAS_VERSION_MINOR * 100 + OAS_VERSION_PATCH;
}
