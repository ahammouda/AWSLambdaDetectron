#include "storage.hpp"
#include "utils.hpp"

// https://github.com/aws/aws-sdk-cpp/issues/1526
// https://stackoverflow.com/questions/65039858/aws-cpp-sdk-jsonvalue-add-array-of-double-or-integer-to-json
// template <typename T>
// std::shared_ptr<>
Aws::DynamoDB::Model::AttributeValue GetDynamoSafe(float value) {
    Aws::DynamoDB::Model::AttributeValue valueDynamo;
    // if constexpr (std::is_same_v<T, animal>)
    valueDynamo.SetN(value);
    return valueDynamo;
    // std::make_shared<Aws::DynamoDB::Model::AttributeValue>()
}

// std::shared_ptr<>
Aws::DynamoDB::Model::AttributeValue GetDynamoSafe(char* value) {
    Aws::DynamoDB::Model::AttributeValue valueDynamo;
    // if constexpr (std::is_same_v<T, animal>)
    valueDynamo.SetS(value);
    return valueDynamo;
    // std::make_shared<Aws::DynamoDB::Model::AttributeValue>()
}

// std::shared_ptr<>
Aws::DynamoDB::Model::AttributeValue GetDynamoSafe(float value[], int size) {
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
    return dynamoList;
    //std::make_shared<Aws::DynamoDB::Model::AttributeValue>()
}

// TODO: Get rid of this or add to utils as a documenations source
Aws::String awsString(char str[]){
    std::string s(str);
    Aws::String a_str(s.c_str(),s.size());
    return a_str;
}

// Follow types for BatchWriteItem to build this up
// https://sdk.amazonaws.com/cpp/api/LATEST/class_aws_1_1_dynamo_d_b_1_1_dynamo_d_b_client.html
Aws::DynamoDB::Model::BatchWriteItemRequest CreateBatchWrite(std::list<Prediction> predictions,
                                                             char* imageUploadId,
                                                             std::string dynamoTableName){
    // core data structures
    Aws::DynamoDB::Model::BatchWriteItemRequest batchWriteRequest =
        *(new Aws::DynamoDB::Model::BatchWriteItemRequest());
    std::vector<Aws::DynamoDB::Model::WriteRequest> writeRequests;
    for (Prediction pred: predictions){
        // Package writeReqs
        Aws::DynamoDB::Model::WriteRequest writeReq;
        // Which is built up of PutRequest
        Aws::DynamoDB::Model::PutRequest putReq;
        // Which requires a document
        Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue> document;

        document[Aws::String("id")] = GetDynamoSafe(UUID());
        // std::cout << "attempting to add " << imageUploadId << " field..." << std::endl;
        document[Aws::String("imageUploadId")] = GetDynamoSafe( imageUploadId );
        document[Aws::String("boundingBox")] = GetDynamoSafe(pred.boundingBox,4);
        document[Aws::String("classification")] = GetDynamoSafe(
            const_cast<char*>(
                pred.classification.c_str()
            )
        );
        document[Aws::String("score")] = GetDynamoSafe(pred.score);

        putReq.SetItem(document);
        // std::cout << "json putReq: " << putReq.Jsonize().View().WriteCompact(true) << std::endl;
        writeReq.SetPutRequest(putReq);
        // std::cout << "json writeReq: " << writeReq.Jsonize().View().WriteCompact(true) << std::endl;
        writeRequests.push_back(writeReq);
    }
    // Add WriteRequests to BatchWriteReq
    Aws::String dynamoTableNameAStr(dynamoTableName.c_str(),dynamoTableName.size());
    batchWriteRequest.AddRequestItems(dynamoTableNameAStr, writeRequests);
    std::cout << "prepped " << predictions.size() << " write requests..." << std::endl;
    return batchWriteRequest;
}

//const Aws::DynamoDB::DynamoDBClient& dynamoClient,
int storePredictions(
    const Aws::DynamoDB::DynamoDBClient& dynamoClient,
    std::string dynamoTableName,
    char* imageUploadId,
    std::list<Prediction> predictions){
    std::cout << "running storePredictions code.."  << std::endl;



    int count = 0;
    while (!predictions.empty()){
        std::list<Prediction> newPredictions = {};
        while (newPredictions.size()<25 && !predictions.empty()) {
            newPredictions.push_back(predictions.front());
            predictions.pop_front();
            count++;
        }
        Aws::DynamoDB::Model::BatchWriteItemRequest batchWriteRequest = CreateBatchWrite(
            newPredictions,imageUploadId,dynamoTableName);

        const Aws::DynamoDB::Model::BatchWriteItemOutcome result = dynamoClient.BatchWriteItem(batchWriteRequest);
        if (!result.IsSuccess())
        {
            std::cout << result.GetError().GetMessage() << std::endl;
            return 1;
        }
        std::cout << "Successfully batch wrote items through " << count << std::endl; // << generated_pk << " !" <<
        newPredictions.clear();
    }
    return 0;
}

char* storeImageProcessingMetaData(
                const Aws::DynamoDB::DynamoDBClient& dynamoClient,
                const Aws::CognitoIdentityProvider::CognitoIdentityProviderClient& cognitoClient,
                Aws::String s3Key, Aws::String tableName){
    char* storageKey = UUID();
    // From s3Key, compute userName
    std::vector<std::string> pathComponents = splitString(std::string(s3Key),'/');
    std::string userName = pathComponents.front();
    // https://stackoverflow.com/questions/5878775/how-to-find-and-replace-string
    std:: string tok("-at-");
    size_t pos = userName.find(tok);
    // TODO: don't run this function if pos returns error code of some kind
    userName.replace(pos,tok.length(), "@");

    // With username, fetch userId;
    auto const& userPoolId = Aws::Environment::GetEnv("USER_POOL_ID");
    Aws::CognitoIdentityProvider::Model::AdminGetUserRequest adminReq;
    adminReq.SetUserPoolId(userPoolId);
    adminReq.SetUsername(Aws::String(userName));
    const Aws::CognitoIdentityProvider::Model::AdminGetUserOutcome adminOutcome =
            cognitoClient.AdminGetUser(adminReq);
    if (!adminOutcome.IsSuccess()){
        std::cout << adminOutcome.GetError().GetMessage() << std::endl;
        char* error = const_cast<char*>("ERROR");
        return error;
    }
    std::string userId;
    std::string idToken("sub");
    auto& adminResult = adminOutcome.GetResult();
    std::cout << "Attributes size:  " << adminResult.GetUserAttributes().size() << std::endl;
    for (Aws::CognitoIdentityProvider::Model::AttributeType atype:
                adminResult.GetUserAttributes()){
        //
        std::cout << "key: value = " << atype.GetName() <<
            ":" << atype.GetValue() << std::endl;

        if ( idToken.compare(atype.GetName()) == 0 ){
            userId = atype.GetValue();
            std::cout << "userID found: " << userId << std::endl;
            break;
        }
    }

    /* Build document:
     * { id: '', createdBy: '', createdAt:  '', imageKey: '' }
     */
    std::cout << "initializing req... " << std::endl;
    Aws::DynamoDB::Model::PutItemRequest putItemRequest;
    putItemRequest.SetTableName( tableName );

    // Aws::DynamoDB::Model::AttributeValue pk;
    // pk.SetS(storageKey);
    std::cout << "Adding id... " << std::endl;
    putItemRequest.AddItem("id", GetDynamoSafe(storageKey));
    std::cout << "Adding createdBy... " << std::endl;
    putItemRequest.AddItem("createdBy", GetDynamoSafe(
            const_cast<char*>( userId.c_str() )
        )
    );
    // Not sure this matters - you'll have it on the s3 key if truly needed
    // putItemRequest.AddItem("createdAt", GetDynamoSafe( TODO ));
    std::cout << "Adding imageKey... " << std::endl;
    putItemRequest.AddItem("imageKey",
        GetDynamoSafe(
            const_cast<char*>( s3Key.c_str() )
        )
    );

    std::cout << "Adding sending request... " << std::endl;
    const Aws::DynamoDB::Model::PutItemOutcome result = dynamoClient.PutItem(putItemRequest);
    if (!result.IsSuccess())
    {
        std::cout << result.GetError().GetMessage() << std::endl;
        char* error = const_cast<char*>("ERROR");
        return error;
    }
    std::cout << "Successfully created ImageUploads: " << storageKey << " !" << std::endl;
    return storageKey;
}