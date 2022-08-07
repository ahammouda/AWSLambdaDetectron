#include "prediction.hpp"
#include "storage.hpp"

int main()
{
    // Inputs: dynamo-table-name, model-file, image-file
    std::cout << "calling makePredictions"  << std::endl;

    auto const& baseDirectory = Aws::Environment::GetEnv("MODEL_DIR");
    std::cout << "base directory: " << baseDirectory << std::endl;
    std::list<Prediction> ls = makePredictions(
        "../b60cfeea70c3c7045141eadce552135921b95679.png","../model.ts");
    std::cout << "makePredictions returns: " << ls.size() << std::endl;

    Aws::SDKOptions options;
    options.cryptoOptions.initAndCleanupOpenSSL=false;


    Aws::InitAPI(options);
    {
        Aws::Client::ClientConfiguration clientConfig;
        // clientConfig.region = Aws::Environment::GetEnv("AWS_REGION");

        // std::cout << "inititializing lambda config in " << clientConfig.region << std::endl;

        Aws::DynamoDB::DynamoDBClient dynamoClient(clientConfig);

        std::cout << "inititialized lambda runtime" << std::endl;

        // Table Name
        auto const& tableName = Aws::Environment::GetEnv("TABLE_NAME");
        std::cout << "calling storePredictions for table " << tableName << "..." << std::endl;
        int code = storePredictions(dynamoClient,tableName,ls); //dynamoClient,
        std::cout << "storePredictions status code: " << code << std::endl;

    }
    ShutdownAPI(options);
    return 0;
}
