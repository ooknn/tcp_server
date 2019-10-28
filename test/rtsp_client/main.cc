#include "rtsp.h"
#include "io_context.h"
#include "log.h"

int main(int argc, char** argv)
{
    std::string url;
    if (argc == 2)
    {
        url = argv[1];
    }
    if (url.empty())
    {
        LOG << "Please enter rtsp url";
        exit(0);
    }

    io_context context;

    RtspTcpClient c(&context, url);

    c.connect();

    context.run();
    return 0;
}
