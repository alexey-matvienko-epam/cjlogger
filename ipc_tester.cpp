
#include <ipc/broker.h>
#include <ipc/connection.h>
#include <atomic>
#include <thread>
#include <memory>
#include <iostream>
#include <csignal>
#include <chrono>
#include <algorithm>
#include <vector>
#include <format>
#include <map>

static std::atomic_bool stop_flag_;
static auto broker_ = std::make_unique<vizio::ipc::Broker>();

void signal_handler(int signum) {
    broker_->stop();
    stop_flag_.store(true);
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    std::map<std::string, std::string> arguments;
    std::cout.setf( std::ios_base::unitbuf );

    // Parse command-line arguments
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 < argc) { // Ensure there's a value after the key
            arguments[argv[i]] = argv[i + 1];
        }
    }

    const std::string usage = "Usage: --type <client/server> --name <name> --topic <topic> [client: --message <message>]";

    // Check for required arguments
    if (arguments.find("--type") == arguments.end() ||
        arguments.find("--name") == arguments.end() ||
        arguments.find("--topic") == arguments.end()) {
        std::cerr << usage << std::endl;
        return 1;
    }

    // Retrieve and print the arguments
    const std::string &type = arguments["--type"];
    const std::string &name = arguments["--name"];
    const std::string &topic = arguments["--topic"];
    std::string message;

    std::vector<std::string> types = {"client", "server"};
    if (!std::any_of(types.begin(), types.end(),
    [&type] (const auto &t) {return (type == t);})) {
        std::cerr << usage << std::endl;

        return 1;
    }
    
    if (type == "client") {
        if (arguments.find("--message") == arguments.end()) {
            std::cerr << usage << std::endl;

            return 1;
        }

        message = arguments["--message"];
    }

    const auto broker_name = (type == "client") ? "test_client_broker" : name;
    if (!broker_->initialize(broker_name, 0)) {
        std::cout << "Couldn't initialize broker: " << broker_name << std::endl;

        return -1;
    }
    std::cout << "Initialize broker: " << broker_name << std::endl;

    if (type == "client") {
        auto connection_ = broker_->connectToServer(name);
        if (!connection_) {
            std::cout << "Failed to connect to broker: " << name << std::endl;

            return -1;
        } else {
            std::cout << "Established connection to " << name << std::endl;
        }

        vizio::ipc::Message<std::string> ipc_message(message);
        auto send_result = connection_->send(ipc_message, topic);
        if (!connection_) {
            std::cout << "Failed to send message to the broker: " << message << std::endl;

            return -1;
        } else {
            std::cout << "Send message to the broker: " << message << std::endl;
        }

        broker_->stop();
    } else if (type == "server") {
        std::vector<std::thread> connections;
        std::thread accept_thread([&topic, &connections] () {
            std::cout << "Start accept thread." << std::endl;
            while (!stop_flag_) {
                auto connection_ = broker_->acceptClientConnection();
                std::cout << "New connection" << std::endl;

                std::thread connection_thread([&topic] (std::unique_ptr<vizio::ipc::Connection> connection) {
                    std::cout << "Start receive thread." << std::endl;
                    while (!stop_flag_) {
                        const auto& [request, success] = connection->receive<std::string>(topic);
                        if (!success) {
                            std::cout << "Failed to receive data from topic metric" << std::endl;
                            return;
                        }
                        
                        std::cout << "Received message: " << *request << std::endl;
                    }
                    std::cout << "Stop receive thread." << std::endl;
                }, std::move(connection_));
                connections.emplace_back(std::move(connection_thread));
            }
            std::cout << "Stop accept thread." << std::endl;
        });

        std::cout.flush();
        while (!stop_flag_) {
            std::cout << "waiting..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        broker_->stop();

        accept_thread.join();
        for (auto &connection_thread : connections) {
            if (connection_thread.joinable()) {
                connection_thread.join();
            }
        }
        std::cout << "Stop server" << std::endl;
    }

    return 0;
}
