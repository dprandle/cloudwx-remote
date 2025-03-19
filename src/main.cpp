#include "logging.h"
#include "cloudwx.h"

int main(int argc, char **argv)
{
    cloudwx_ctxt ctxt{};
    if (!init_cloudwx(&ctxt)) {
        ilog("Failed to initialize cloudwx");
        return -1;
    }
    ilog("Successfully initialized cloudwx");
    terminate_cloudwx(&ctxt);
}
