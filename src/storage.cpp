#include "storage.hpp"

// https://github.com/aws/aws-sdk-cpp/issues/1526
// https://stackoverflow.com/questions/65039858/aws-cpp-sdk-jsonvalue-add-array-of-double-or-integer-to-json
// template <typename T>
std::shared_ptr<Aws::DynamoDB::Model::AttributeValue> GetDynamoSafe(float value) {
    Aws::DynamoDB::Model::AttributeValue valueDynamo;
    // if constexpr (std::is_same_v<T, animal>)
    valueDynamo.SetN(value);
    return std::make_shared<Aws::DynamoDB::Model::AttributeValue>(valueDynamo);
}

std::shared_ptr<Aws::DynamoDB::Model::AttributeValue> GetDynamoSafe(char* value) {
    Aws::DynamoDB::Model::AttributeValue valueDynamo;
    // if constexpr (std::is_same_v<T, animal>)
    valueDynamo.SetS(value);
    return std::make_shared<Aws::DynamoDB::Model::AttributeValue>(valueDynamo);
}

std::shared_ptr<Aws::DynamoDB::Model::AttributeValue> GetDynamoSafe(float value[], int size) {
    Aws::DynamoDB::Model::AttributeValue dynamoList = {};
    int n;
    for (n=0 ; n<size ; ++n ) {
        Aws::DynamoDB::Model::AttributeValue itemBoxCoord;
        itemBoxCoord.SetN(value[n]);

        // Don't want to do this inline as compiler looses scope of shared and does a
        // double free by mistake
        auto sharedItem =
            std::make_shared<Aws::DynamoDB::Model::AttributeValue>(itemBoxCoord);

        dynamoList.AddLItem(
            sharedItem
        );
    }
    return std::make_shared<Aws::DynamoDB::Model::AttributeValue>(dynamoList);
}

//const Aws::DynamoDB::DynamoDBClient& dynamoClient,
int storePredictions(
    const Aws::DynamoDB::DynamoDBClient& dynamoClient,
    std::string dynamoTableName,
    std::list<Prediction> predictions)
{
    std::cout << "running storePredictions code.."  << std::endl;

    // Aws::SDKOptions options;
    // options.cryptoOptions.initAndCleanupOpenSSL=false;

    // Aws::InitAPI(options);
    // {
    //     Aws::Client::ClientConfiguration clientConfig;
    //     // clientConfig.region = Aws::Environment::GetEnv("AWS_REGION");

    //     std::cout << "inititializing lambda config in " << clientConfig.region << std::endl;

    //     Aws::DynamoDB::DynamoDBClient dynamoClient(clientConfig);

    //     std::cout << "inititialized lambda runtime" << std::endl;

        // Init Request
        Aws::DynamoDB::Model::PutItemRequest putItemRequest;

        putItemRequest.SetTableName( dynamoTableName );

        char *generated_pk = (char *) malloc(37*sizeof(char));
        uuid_t uuid;
        uuid_generate(uuid);
        uuid_unparse(uuid, generated_pk);
        std::cout << generated_pk << std::endl;

        // // Init top level list object to store in this request
        Aws::DynamoDB::Model::AttributeValue allPredictions = {};

        // // Set Attribute Value for primary key
        Aws::DynamoDB::Model::AttributeValue pk;
        pk.SetS(generated_pk);

        while (!predictions.empty())
        {
            Prediction curPrediction = predictions.front();
            predictions.pop_front();

            // Initialize Prediction Record
            Aws::DynamoDB::Model::AttributeValue predictionSet = {};

            // Set key/values for prediction record: Bounding Box
            predictionSet.AddMEntry(Aws::String("boundingBoxes"),
                GetDynamoSafe(
                    curPrediction.boundingBox,4
                )
            );
            predictionSet.AddMEntry(
                Aws::String("classification"),
                GetDynamoSafe(
                    const_cast<char*>(
                        curPrediction.classification.c_str()
                    )
                )
            );
            predictionSet.AddMEntry(Aws::String("score"),
                GetDynamoSafe(
                    curPrediction.score
                )
            );

            // Make Prediction Record type safe for AWS
            auto sharedPredictionSet =
                std::make_shared<Aws::DynamoDB::Model::AttributeValue>(predictionSet);

            // Add Prediction Record to top level list of all predictions
            allPredictions.AddLItem(sharedPredictionSet);
        }
        // Store field/values for final dynamo document put request
        putItemRequest.AddItem("id", pk);
        putItemRequest.AddItem("predictions", allPredictions);

        std::cout << "Attempting to create Document: " << generated_pk << "..." << std::endl;
        const Aws::DynamoDB::Model::PutItemOutcome result = dynamoClient.PutItem(putItemRequest);
        if (!result.IsSuccess())
        {
            std::cout << result.GetError().GetMessage() << std::endl;
            return 1;
        }
        std::cout << "Successfully created Document" << generated_pk << " !" << std::endl;
    // }
    // ShutdownAPI(options);
    return 0;
}