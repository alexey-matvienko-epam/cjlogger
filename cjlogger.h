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

    const Aws::Kinesis::KinesisClient getClient();

    void setAwsConfig(const std::string &id, const std::string &key, const std::string &region);
    void setStreamName(const std::string &stream_name);
    void setIpcConfig(const std::string &broker_name, const std::string &topic);

    void startIPCListener();
    void acceptConnections();

    void process(std::unique_ptr<vizio::ipc::Connection> connection);
    void wait();
    void stop();

private:
    void initVariables();
    void waitAllConnections();
    void sendToKinesis(const std::string& data);
    std::string getSysRegVariable(const std::string &key) const;

    std::string var_device_id_;
    std::string var_fw_version_;
    std::string var_serial_number_;
    std::string var_model_;
    
    std::thread accept_thread_;
    std::atomic<bool> stop_flag_;
    std::unique_ptr<vizio::ipc::Broker> broker_;
    std::vector<std::thread> connection_threads_;
    std::mutex connections_mutex_;

    std::string aws_id_;
    std::string aws_key_;
    std::string aws_region_;
    std::string stream_name_;

    std::string broker_name_;
    std::string topic_;
};