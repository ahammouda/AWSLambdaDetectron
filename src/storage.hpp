// aws core
#include <aws/core/Aws.h>
#include <aws/core/utils/logging/LogLevel.h>
#include <aws/core/utils/logging/ConsoleLogSystem.h>
#include <aws/core/utils/logging/LogMacros.h>

// s3 specific
#include <aws/core/utils/json/JsonSerializer.h>
#include <aws/core/utils/HashingUtils.h>

#include <aws/core/platform/Environment.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/auth/AWSCredentialsProvider.h>

#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>

// Dynamo specific
#include <aws/core/utils/Outcome.h>
#include <aws/dynamodb/DynamoDBClient.h>
#include <aws/dynamodb/model/AttributeDefinition.h>
#include <aws/dynamodb/model/AttributeValueValue.h>
#include <aws/dynamodb/model/PutItemRequest.h>
#include <aws/dynamodb/model/PutItemResult.h>

#include "utils.hpp"

std::shared_ptr<Aws::DynamoDB::Model::AttributeValue> GetDynamoSafe(float value);

std::shared_ptr<Aws::DynamoDB::Model::AttributeValue> GetDynamoSafe(char* value);

std::shared_ptr<Aws::DynamoDB::Model::AttributeValue> GetDynamoSafe(float value[], int size);

//const Aws::DynamoDB::DynamoDBClient& dynamoClient, //
int storePredictions(
    const Aws::DynamoDB::DynamoDBClient& dynamoClient,
    std::string dynamoTableName,
    std::list<Prediction> predictions
);