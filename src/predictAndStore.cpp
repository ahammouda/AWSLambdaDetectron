#include "prediction.hpp"
#include "storage.hpp"

int main()
{
    // Inputs: dynamo-table-name, model-file, image-file
    std::cout << "starting predictAndStore"  << std::endl;

    Aws::SDKOptions options;
    options.cryptoOptions.initAndCleanupOpenSSL=false;

    Aws::InitAPI(options);
    {
        Aws::Client::ClientConfiguration clientConfig;
        // clientConfig.region = Aws::Environment::GetEnv("AWS_REGION");

        // std::cout << "inititializing lambda config in " << clientConfig.region << std::endl;

        Aws::DynamoDB::DynamoDBClient dynamoClient(clientConfig);
        Aws::S3::S3Client s3Client(clientConfig);
        Aws::CognitoIdentityProvider::CognitoIdentityProviderClient cognitoClient(clientConfig);

        std::cout << "inititialized lambda runtime" << std::endl;

        auto const& baseDir = Aws::Environment::GetEnv("MODEL_DIR");
        std::cout << "base directory: " << baseDir << std::endl;

        Aws::String fromBucket = Aws::String("flowknow-userstorage-dev");
        Aws::String objectKey = Aws::String(
            "adamhammouda3-at-gmail.com/b60cfeea70c3c7045141eadce552135921b95679.png"
        );

        std::vector<std::string> pathComponents = splitString(std::string(objectKey),'/');
        std::string fileName = pathComponents.back();

        Aws::S3::Model::GetObjectRequest object_request;
        object_request.SetBucket(fromBucket);
        object_request.SetKey(objectKey);

        std::cout << "Attempting to download file from s3://" << fromBucket << "/" << objectKey << std::endl;

        Aws::S3::Model::GetObjectOutcome get_object_outcome =
            s3Client.GetObject(object_request);

        if (get_object_outcome.IsSuccess())
        {
            auto& retrieved_file = get_object_outcome.GetResultWithOwnership().
                GetBody();

            std::fstream image_file(baseDir + fileName, std::ios::out | std::ios::binary);
            image_file << retrieved_file.rdbuf();

            std::list<Prediction> ls = makePredictions(
                baseDir + fileName,
                baseDir + "model.ts"
            );
            std::cout << "makePredictions returns: " << ls.size() << std::endl;

            // Table Name
            auto const& predTableName = Aws::Environment::GetEnv("PRED_TABLE_NAME");
            auto const& uploadTableName = Aws::Environment::GetEnv("UPLOAD_TABLE_NAME");
            std::cout << "calling storeImageProcessingMetaData for table " << uploadTableName << "..." << std::endl;
            std::cout << "calling storePredictions for table " << predTableName << "..." << std::endl;
            // TODO: New function
            char* uuid = storeImageProcessingMetaData(dynamoClient,cognitoClient,objectKey,uploadTableName);
            // todo: instead of passing imageId, pass uuid for metaData
            int code = storePredictions(dynamoClient,predTableName,uuid,ls);
            std::cout << "storePredictions status code: " << code << std::endl;
        }

    }
    ShutdownAPI(options);
    return 0;
}
