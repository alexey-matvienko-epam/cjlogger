#include "cjlogger.h"

#include <aws/core/Aws.h>
#include <aws/core/utils/json/JsonSerializer.h>
#include <aws/kinesis/KinesisClient.h>
#include <aws/kinesis/model/PutRecordRequest.h>
#include <aws/core/auth/AWSCredentials.h>

#include <iostream>

#include <memory>
#include <pthread.h>

#include "sysreg_wrap.h"

const std::string deviceId = "/current/diid";
const std::string serialNumberTopic = "/hw/info/serial_number";
const std::string fwVersionTopic = "/os/version/firmware";
const std::string modelGroupTopic = "/hw/info/model_group";

const std::string aws_id_default = "";
const std::string aws_key_default = "";
const std::string aws_region_default = "";

const std::string broker_name_default = "analyticsMyBroker";
const std::string topic_default = "metric";

const std::string stream_name_default = "TestDataStream";

using namespace Aws;
using namespace Aws::Kinesis;
using namespace Aws::Utils::Json;

CJLogger::CJLogger()
    : broker_(std::make_unique<vizio::ipc::Broker>()),
    aws_id_(aws_id_default), aws_key_(aws_key_default), aws_region_(aws_region_default),
    stream_name_(stream_name_default),
    broker_name_(broker_name_default), topic_(topic_default) {
}

CJLogger::~CJLogger() {
  stop();
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

const Aws::Kinesis::KinesisClient CJLogger::getClient() {
  Aws::Client::ClientConfiguration config;
  config.scheme = Aws::Http::Scheme::HTTPS;
  config.connectTimeoutMs = 30000;
  config.requestTimeoutMs = 10000;
  config.verifySSL = true;
  config.region = aws_region_;
  // client_config_.endpointOverride = awsParams->GetSctvEndpoint();
  // std::shared_ptr<Aws::Client::RetryStrategy> exponentialBackoff = std::make_shared<ExponentialBackoffRetryStrategy>();
  // client_config_.retryStrategy = exponentialBackoff;

  Aws::Auth::AWSCredentials creds = {Aws::String(aws_id_),
                                     Aws::String(aws_key_), ""};
  return {creds, config};
}

void CJLogger::setAwsConfig(const std::string &id, const std::string &key, const std::string &region) {
  aws_id_ = id;
  aws_key_ = key;
  aws_region_ = region;
}

void CJLogger::setStreamName(const std::string &name) {
  stream_name_ = name;
}

void CJLogger::setIpcConfig(const std::string &broker_name, const std::string &topic) {
  broker_name_ = broker_name;
  topic_ = topic;
}

void CJLogger::startIPCListener() {
  initVariables();

  if (!broker_->initialize(broker_name_, 0)) {
    std::cout << "Broker failed to initialize: " << broker_name_ << " won't work" << std::endl;
    return;
  }
  std::cout << "Broker initialized: " << broker_name_ << std::endl;
  accept_thread_ = std::thread(&CJLogger::acceptConnections, this);
}

void CJLogger::acceptConnections() {
  //pthread_setname_np(pthread_self(), "accept_connections");

  std::cout << "Start accepting connections " << std::endl;
  while (!stop_flag_) {
    auto connection = broker_->acceptClientConnection();
    std::cout << "New connection " << std::endl;
    std::thread connection_thread(&CJLogger::process, this, std::move(connection));

    //std::lock_guard<std::mutex> lock(connections_mutex_);
    connection_threads_.emplace_back(std::move(connection_thread));
  }
  std::cout << "Stop accepting connections " << std::endl;
}

void CJLogger::process(std::unique_ptr<vizio::ipc::Connection> connection) {
  //pthread_setname_np(pthread_self(), fmt::format("connection_{}", std::this_thread::get_id()).c_str());

  std::cout << "Start receiving data: " << topic_ << std::endl;
  while (!stop_flag_) {
    const auto& [request, success] = connection->receive<std::string>(topic_);
    if (!success) {
      std::cout << "Failed to receive data from topic: " << topic_ << std::endl;
      return;
    }
    std::cout << "Data received ok: " << *request << std::endl;
    sendToKinesis(*request);
  }
  std::cout << "Stop receiving data: " << topic_ << std::endl;
}

void CJLogger::sendToKinesis(const std::string& data) {
  Aws::Kinesis::KinesisClient kinesisClient = getClient();

  Aws::Kinesis::Model::PutRecordRequest request;
  request.SetStreamName(stream_name_);
  request.SetPartitionKey(var_device_id_);
  request.SetData(Aws::Utils::ByteBuffer((unsigned char*)data.c_str(), data.length()));

  auto outcome = kinesisClient.PutRecord(request);
  if (!outcome.IsSuccess()) {
    std::cerr << "Failed to send data to Kinesis: " << outcome.GetError().GetMessage() << std::endl;
  } else {
    std::cout << "Data sent to Kinesis successfully: " << data << std::endl;
  }
}

void CJLogger::wait() {
  if (accept_thread_.joinable()) {
    accept_thread_.join();
  }
  waitAllConnections();
}

void CJLogger::stop() {
  broker_->stop();
  stop_flag_.store(true);
}

void CJLogger::waitAllConnections() {
  //std::lock_guard<std::mutex> lock(connections_mutex_);
  for (auto &thread : connection_threads_) {
    if (thread.joinable()) {
      thread.join();
    }
  }
  connection_threads_.clear();
}