
#include <aws/core/Aws.h>

#include <memory>
#include <unistd.h>
#include <csignal>
#include <iostream>

#include "cjlogger.h"

static std::atomic_bool stop_flag_;

void signal_handler(int signum) {
    stop_flag_.store(true);
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    Aws::SDKOptions options;
    Aws::InitAPI(options);

    std::map<std::string, std::string> arguments;

    for (int i = 1; i < argc; i += 2) {
        if (i + 1 < argc) {
            arguments[argv[i]] = argv[i + 1];
        }
    }

    const std::string usage = "Usage: --broker_name <broker_name> --topic <topic> --aws_id <id> --aws_key <key> --aws_region <region> --stream_name <stream_name> ";

    //Check for required arguments
    if (arguments.find("--broker_name") == arguments.end() ||
        arguments.find("--topic") == arguments.end() ||
        arguments.find("--aws_id") == arguments.end() ||
        arguments.find("--aws_key") == arguments.end() ||
        arguments.find("--aws_region") == arguments.end() ||
        arguments.find("--stream_name") == arguments.end()) {
        std::cerr << usage << std::endl;
        return 1;
    }

    //Retrieve and print the arguments
    const std::string &broker_name = arguments["--broker_name"];
    const std::string &topic = arguments["--topic"];
    const std::string &aws_id = arguments["--aws_id"];
    const std::string &aws_key = arguments["--aws_key"];
    const std::string &aws_region = arguments["--aws_region"];
    const std::string &stream_name = arguments["--stream_name"];

    auto logger = std::make_unique<CJLogger>();
    logger->setIpcConfig(broker_name, topic);
    logger->setAwsConfig(aws_id, aws_key, aws_region);
    logger->setStreamName(stream_name);
    logger->startIPCListener();
    
    while (!stop_flag_) {
        std::cout << "waiting..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    logger->stop();
    logger->wait();

    Aws::ShutdownAPI(options);
    return 0;
}
