#include "storage.hpp"

// If we wanted to store all the segmentations for each image in a single document, this would
// be the approach to serialization that would get us there.  Note near identical method signature
// to storage.storePredictions

int storePredictions(
    const Aws::DynamoDB::DynamoDBClient& dynamoClient,
    std::string dynamoTableName,
    std::list<Prediction> predictions){
    // TODO: Still need to form individual PutRequests in order to
    //          run the batchItem  --
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
}