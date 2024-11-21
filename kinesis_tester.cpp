
#include <aws/core/Aws.h>
#include <aws/core/utils/json/JsonSerializer.h>
#include <aws/kinesis/KinesisClient.h>
#include <aws/kinesis/model/PutRecordRequest.h>
#include <aws/core/auth/AWSCredentials.h>

const Aws::Kinesis::KinesisClient getClient(const std::string &id, const std::string &key, const std::string &region) {
  Aws::Client::ClientConfiguration config;
  config.scheme = Aws::Http::Scheme::HTTPS;
  config.connectTimeoutMs = 30000;
  config.requestTimeoutMs = 10000;
  config.verifySSL = true;
  config.region = region;
  // client_config_.endpointOverride = awsParams->GetSctvEndpoint();
  // std::shared_ptr<Aws::Client::RetryStrategy> exponentialBackoff = std::make_shared<ExponentialBackoffRetryStrategy>();
  // client_config_.retryStrategy = exponentialBackoff;

  Aws::Auth::AWSCredentials creds = {Aws::String(id),
                                     Aws::String(key), ""};
  return {creds, config};
}

int main(int argc, char* argv[]) {
    Aws::SDKOptions options;
    Aws::InitAPI(options);

    std::map<std::string, std::string> arguments;
    std::cout.setf( std::ios_base::unitbuf );

    // Parse command-line arguments
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 < argc) { // Ensure there's a value after the key
            arguments[argv[i]] = argv[i + 1];
        }
    }

    const std::string usage = "Usage: --broker_name <broker_name> --topic <topic> --aws_id <id> --aws_key <key> --aws_region <region> --stream_name <stream_name> --partition <partition> --message <message>";

    //Check for required arguments
    if (arguments.find("--id") == arguments.end() ||
        arguments.find("--key") == arguments.end() ||
        arguments.find("--region") == arguments.end() ||
        arguments.find("--stream_name") == arguments.end() ||
        arguments.find("--partition") == arguments.end() ||
        arguments.find("--message") == arguments.end()) {
        std::cerr << usage << std::endl;
        return 1;
    }

    //Retrieve and print the arguments
    const std::string &id = arguments["--id"];
    const std::string &key = arguments["--key"];
    const std::string &region = arguments["--region"];
    const std::string &stream_name = arguments["--stream_name"];
    const std::string &partition = arguments["--partition"];
    const std::string &message = arguments["--message"];

    Aws::Kinesis::KinesisClient kinesisClient = getClient(id, key, region);

    Aws::Kinesis::Model::PutRecordRequest request;
    request.SetStreamName(stream_name);
    request.SetPartitionKey(partition);
    request.SetData(Aws::Utils::ByteBuffer((unsigned char*)message.c_str(), message.length()));

    auto outcome = kinesisClient.PutRecord(request);
    if (!outcome.IsSuccess()) {
        std::cerr << "Failed to send data to Kinesis: " << outcome.GetError().GetMessage() << std::endl;
    } else {
        std::cout << "Data sent to Kinesis successfully." << std::endl;
    }

    Aws::ShutdownAPI(options);
    return 0;
}
