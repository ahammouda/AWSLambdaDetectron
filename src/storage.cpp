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

Aws::DynamoDB::Model::BatchWriteItemRequest CreateBatchWrite(std::list<Prediction> predictions,
                                                             std::string s3Key,
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
        document[Aws::String("s3Key")] = GetDynamoSafe(const_cast<char*>(s3Key.c_str()));
        document[Aws::String("boundingBox")] = GetDynamoSafe(pred.boundingBox,4);
        document[Aws::String("classification")] = GetDynamoSafe(
            const_cast<char*>(
                pred.classification.c_str()
            )
        );
        document[Aws::String("score")] = GetDynamoSafe(pred.score);
        putReq.SetItem(document);
        writeReq.SetPutRequest(putReq);
        writeRequests.push_back(writeReq);
    }
    // Add WriteRequests to BatchWriteReq
    Aws::String dynamoTableNameAStr(dynamoTableName.c_str(),dynamoTableName.size());
    batchWriteRequest.AddRequestItems(dynamoTableNameAStr, writeRequests);
    return batchWriteRequest;
}

//const Aws::DynamoDB::DynamoDBClient& dynamoClient,
int storePredictions(
    const Aws::DynamoDB::DynamoDBClient& dynamoClient,
    std::string dynamoTableName,
    std::string s3Key,
    std::list<Prediction> predictions){
    std::cout << "running storePredictions code.."  << std::endl;
    // Aws::DynamoDB::Model::BatchWriteItemRequest batchWriteRequest = *(new Aws::DynamoDB::Model::BatchWriteItemRequest());

    // // main data structure to pass to client
    // std::vector<Aws::DynamoDB::Model::WriteRequest> writeRequests;

    // // Build up request ONE *********************************
    // Aws::DynamoDB::Model::WriteRequest writeReqOne;

    // // Which is built up of PutRequest
    // Aws::DynamoDB::Model::PutRequest putReqOne;

    // //which is built of a map:
    // Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue> documentOne;
    // std::string keyO{"id"};
    // Aws::String a_keyO(keyO.c_str(),keyO.size());
    // documentOne[a_keyO] = GetDynamoSafe(UUID());
    // std::string valO{"foo"};
    // Aws::String a_valO(valO.c_str(),valO.size());
    // char valOne[] = "bar";
    // documentOne[a_valO] = GetDynamoSafe(valOne);

    // putReqOne.SetItem(documentOne);
    // writeReqOne.SetPutRequest(putReqOne);

    // // Build up request TWO *********************************
    // Aws::DynamoDB::Model::WriteRequest writeReqTwo;

    // // Which is built up of PutRequest
    // Aws::DynamoDB::Model::PutRequest putReqTwo;

    // // https://stackoverflow.com/questions/52452144/how-to-convert-between-a-stdstring-and-an-awsstring
    // //which is built of a map:
    // Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue> documentTwo;
    // std::string key{"id"};
    // Aws::String a_key(key.c_str(),key.size());
    // documentTwo[key] = GetDynamoSafe(UUID());
    // std::string val{"foo"};
    // Aws::String a_val(val.c_str(),val.size());
    // char valTwo[] = "bar x 2";
    // documentTwo[a_val] = GetDynamoSafe(valTwo);

    // putReqTwo.SetItem(documentTwo);
    // writeReqTwo.SetPutRequest(putReqTwo);

    // // *******************************************************
    // writeRequests.push_back(writeReqOne);
    // writeRequests.push_back(writeReqTwo);

    // std::string TableName{"TestTable"};
    // Aws::String a_TableName(TableName.c_str(),TableName.size());
    // batchWriteRequest.AddRequestItems(a_TableName, writeRequests);
    // NOTE: Can only create these requests in chunks of 25;  Therefore, we need to create
    //       some kind of slicing operatation to pop from the front
    int count = 0;
    while (!predictions.empty()){
        std::list<Prediction> newPredictions = {};
        while (newPredictions.size()<25 && !predictions.empty()) {
            newPredictions.push_back(predictions.front());
            predictions.pop_front();
            count++;
        }
        Aws::DynamoDB::Model::BatchWriteItemRequest batchWriteRequest = CreateBatchWrite(
            newPredictions,s3Key,dynamoTableName);

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