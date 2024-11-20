#include <aws/kinesis/KinesisClient.h>
#include <thread>

#include <string>
#include <atomic>
#include <mutex>

#include <ipc/broker.h>
#include <ipc/connection.h>

using namespace Aws;
using namespace Aws::Kinesis;
using namespace Aws::Utils::Json;

class CJLogger {
public:
    CJLogger();
    ~CJLogger();

    const Aws::Kinesis::KinesisClient getClient(const Aws::Client::ClientConfiguration &config);

    void startIPCListener();
    void acceptConnections();
    void listenToIPC();
    void process(std::unique_ptr<vizio::ipc::Connection> connection, const std::string& topic);
    void stop();

private:
    void initAws();
    void initVariables();
    void stopAllConnections();
    void sendToKinesis(const std::string& data);
    std::string getSysRegVariable(const std::string &key) const;

    std::string var_device_id_;
    std::string var_fw_version_;
    std::string var_serial_number_;
    std::string var_model_;

    Client::ClientConfiguration client_config_;
    std::string kinesis_stream_name_;
    std::thread listener_thread_;
    std::thread accept_thread_;
    std::atomic<bool> stop_flag_;
    vizio::ipc::Broker broker_;
    const std::set<std::string> event_topics_ = {"perfomance"};
    std::map<std::thread::id, std::thread> connection_threads_;
    std::mutex connections_mutex_;
};