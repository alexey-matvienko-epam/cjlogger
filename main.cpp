
#include <aws/core/Aws.h>

#include <unistd.h>
#include <csignal>
#include <iostream>

#include "cjlogger.h"

int main() {
    Aws::SDKOptions options;
    Aws::InitAPI(options);

    {
        CJLogger logger;
        logger.startIPCListener();

        std::cout << "Press Enter to stop the server...\n";
        std::cin.get();

        logger.stop();
    }

    Aws::ShutdownAPI(options);
    return 0;
}
