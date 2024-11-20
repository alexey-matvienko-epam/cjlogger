#include "cjlogger.h"

#include <aws/core/Aws.h>
#include <aws/core/utils/json/JsonSerializer.h>
#include <aws/kinesis/KinesisClient.h>
#include <aws/kinesis/model/PutRecordRequest.h>
#include <aws/core/auth/AWSCredentials.h>

#include <iostream>

#include <pthread.h>

#include "sysreg_wrap.h"

const std::string deviceId = "/current/diid";
const std::string serialNumberTopic = "/hw/info/serial_number";
const std::string fwVersionTopic = "/os/version/firmware";
const std::string modelGroupTopic = "/hw/info/model_group";

using namespace Aws;
using namespace Aws::Kinesis;
using namespace Aws::Utils::Json;

CJLogger::CJLogger()
    : kinesis_stream_name_("name"), stop_flag_(false) {
}

CJLogger::~CJLogger() {
  stopAllConnections();
}

void CJLogger::initAws() {
  client_config_.scheme = Aws::Http::Scheme::HTTPS;
  client_config_.connectTimeoutMs = 30000;
  client_config_.requestTimeoutMs = 10000;
  client_config_.verifySSL = true;
  client_config_.region = "us-east-1";
  // client_config_.endpointOverride = awsParams->GetSctvEndpoint();
  // std::shared_ptr<Aws::Client::RetryStrategy> exponentialBackoff = std::make_shared<ExponentialBackoffRetryStrategy>();
  // client_config_.retryStrategy = exponentialBackoff;
}

std::string CJLogger::getSysRegVariable(const std::string &key) const {
  static vizio::systemregistry::SystemRegistry registry(vizio::ipc::Broker::getDefaultBroker());

  auto result = registry.get(key);
  if (result.second) {
    return result.first;
  } else {
    std::cerr << fmt::format("Get sysreg value for key {} failed with status {}", key, int(registry.getStatus())) << std::endl;
  }
  return std::string{};
}

void CJLogger::initVariables() {
  var_device_id_ = getSysRegVariable(deviceId);
  // var_fw_version_ = getSysRegVariable(fwVersionTopic);
  // var_serial_number_ = getSysRegVariable(serialNumberTopic);
  // var_model_ = getSysRegVariable(modelNameTopic);

  // auto time = std::chrono::system_clock::now().time_since_epoch();
  // int64_t ts = duration_cast<milliseconds>(time).count();

  // formedJson = formedJson.WithString("partitionKey", var_device_id_.c_str());
  // formedJson = formedJson.WithString("shardId", shardId);
  // formedJson = formedJson.WithString("streamName", kinesis_stream_name_);
  // formedJson = formedJson.WithString("sequenceNumber", sequenceNumber);
  // formedJson = formedJson.WithString("firmwareVersion", var_fw_version_.c_str());
  // formedJson = formedJson.WithString("esn", var_serial_number_.c_str());
  // formedJson = formedJson.WithString("timestamp", std::to_string(ts).c_str());
}

const Aws::Kinesis::KinesisClient CJLogger::getClient(const Aws::Client::ClientConfiguration &config) {
  Aws::Auth::AWSCredentials creds = {Aws::String("Id"),
                                     Aws::String("Key"), ""};
  return {creds, config};
}

void CJLogger::startIPCListener() {
  initAws();
  initVariables();

  if (!broker_.initialize("KinesisAnaliticsBroker", getpid())) {
    std::cerr << "Broker failed to initialize. KinesisAnaliticsBroker won't work" << std::endl;
    return;
  }
  accept_thread_ = std::thread(&CJLogger::acceptConnections, this);
  listener_thread_ = std::thread(&CJLogger::listenToIPC, this);
}

void CJLogger::acceptConnections() {
  pthread_setname_np(pthread_self(), "accept_connections");

  std::cout << "Start accepting connections " << std::endl;
  while (!stop_flag_.load()) {
    auto connection = broker_.acceptClientConnection();
    if (connection) {
      std::lock_guard<std::mutex> lock(connections_mutex_);
      std::thread connection_thread(&CJLogger::process, this, std::move(connection), "accepted_topic");
      connection_threads_[connection_thread.get_id()] = std::move(connection_thread);
    }
  }
  std::cout << "Stop accepting connections " << std::endl;
}

void CJLogger::listenToIPC() {
  pthread_setname_np(pthread_self(), "listen_data");

  std::cout << "Start listening ipc" << std::endl;
  while (!stop_flag_.load()) {
    auto [connection, topic] = broker_.waitForData(event_topics_);
    if (!connection || topic.empty()) {
      std::cout << "S3Logger:: topic: " << topic << " or connection is nullptr" << std::endl;
      continue;
    }
    std::cout << "Topic received is " << topic << std::endl;
    std::lock_guard<std::mutex> lock(connections_mutex_);
    std::thread connection_thread(&CJLogger::process, this, std::move(connection), topic);
    connection_threads_[connection_thread.get_id()] = std::move(connection_thread);
  }
  std::cout << "Stop listening ipc" << std::endl;
}

void CJLogger::process(std::unique_ptr<vizio::ipc::Connection> connection, const std::string& topic) {
  pthread_setname_np(pthread_self(), fmt::format("connection_{}", topic).c_str());

  std::cout << "Start receiving data: " << topic << std::endl;
  while (!stop_flag_.load()) {
    if (connection) {
      const auto& [request, success] = connection->receive<std::string>(topic);
      if (!success) {
        std::cout << "Failed to receive data from topic: " << topic << std::endl;
        return;
      }
      sendToKinesis(*request);
    }
  }
  std::cout << "Stop receiving data: " << topic << std::endl;
}

void CJLogger::sendToKinesis(const std::string& data) {
  Aws::Kinesis::KinesisClient kinesisClient = getClient(client_config_);

  JsonValue payload;
  try {
    JsonValue received_data(data);
    payload.WithObject("message", received_data);
  } catch (const std::exception& e) {
    payload.WithString("message", data);
  }
  std::string json_data = payload.View().WriteCompact();

  Aws::Kinesis::Model::PutRecordRequest request;
  request.SetStreamName(kinesis_stream_name_);
  request.SetPartitionKey(var_device_id_);
  request.SetData(Aws::Utils::ByteBuffer((unsigned char*)json_data.c_str(), json_data.length()));

  auto outcome = kinesisClient.PutRecord(request);
  if (!outcome.IsSuccess()) {
    std::cerr << "Failed to send data to Kinesis: " << outcome.GetError().GetMessage() << std::endl;
  } else {
    std::cout << "Data sent to Kinesis successfully." << std::endl;
  }
}

void CJLogger::stop() {
  broker_.stop();
  stop_flag_.store(true);
  if (listener_thread_.joinable()) {
    listener_thread_.join();
  }
  if (accept_thread_.joinable()) {
    accept_thread_.join();
  }
  stopAllConnections();
  std::cout << "Stopped IPC listener." << std::endl;
}

void CJLogger::stopAllConnections() {
  std::lock_guard<std::mutex> lock(connections_mutex_);
  for (auto& [id, thread] : connection_threads_) {
    if (thread.joinable()) {
      thread.join();
    }
  }
  connection_threads_.clear();
}