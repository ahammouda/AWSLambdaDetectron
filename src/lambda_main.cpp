#include "prediction.hpp"
#include "storage.hpp"

#include <aws/lambda-runtime/runtime.h>

//https://stackoverflow.com/questions/612097/how-can-i-get-the-list-of-files-in-a-directory-using-c-or-c
void logContents(std::string path){
    using namespace std;

    DIR *dr;
    struct dirent *en;
    dr = opendir(path.c_str()); //open all directory
    if (dr) {
        while ((en = readdir(dr)) != NULL) {
            cout<<" \n"<<en->d_name; //print all directory name
        }
        closedir(dr); //close all directory
        cout << "\n No more directory contents" << endl;
    } else {
        cout<<"\n Directory Not Found"<<endl;
    }
}

using namespace aws::lambda_runtime;

char const TAG[] = "LAMBDA_ALLOC";

static invocation_response my_handler(invocation_request const& req,
                                      Aws::S3::S3Client const& s3Client,
                                      Aws::DynamoDB::DynamoDBClient const& dynamoClient,
                                      Aws::CognitoIdentityProvider::CognitoIdentityProviderClient cognitoClient) {

    auto const& predTableName = Aws::Environment::GetEnv("PRED_TABLE_NAME");
    auto const& uploadTableName = Aws::Environment::GetEnv("UPLOAD_TABLE_NAME");

    // auto const& tableName = Aws::Environment::GetEnv("TABLE_NAME");
    auto const& baseDir = Aws::Environment::GetEnv("MODEL_DIR");

    std::cout << "pred Table Name:  " << predTableName << std::endl;
    std::cout << "uploads Table Name:  " << uploadTableName << std::endl;
    std::cout << "Model Directory:  " << baseDir << std::endl;

    // Log existence/access to model directory
    logContents( baseDir );

    using namespace Aws::Utils::Json;
    JsonValue json(req.payload);
    if (!json.WasParseSuccessful()) {
        return invocation_response::failure("Failed to parse input JSON", "InvalidJSON");
    }

    auto v = json.View();
    if (!v.ValueExists("Records") ||
        !v.GetArray("Records")[0].ValueExists("s3") ||
        !v.GetArray("Records")[0].GetObject("s3").ValueExists("bucket")||
        !v.GetArray("Records")[0].GetObject("s3").ValueExists("object")) {
        return invocation_response::failure("Missing input value s3bucket or s3key", "InvalidJSON");
    }
    // const Aws::String userId = v.GetArray("Records")[0].GetObject("userIdentity").GetString("principalId");

    // std::cout << "*** User Id: " << userId << std::endl;

    const Aws::String fromBucket =
        v.GetArray("Records")[0].GetObject("s3").GetObject("bucket").GetString("name");

    std::cout << "request payload as json: Bucket Name " << fromBucket << std::endl;

    const Aws::String objectKey =
        v.GetArray("Records")[0].GetObject("s3").GetObject("object").GetString("key");

    std::cout << "request payload as json: Object Key " << objectKey << std::endl;

    // Build s3 request
    Aws::S3::Model::GetObjectRequest object_request;
    object_request.SetBucket(fromBucket);
    object_request.SetKey(objectKey);

    std::cout << "Attempting to download file from s3://" << fromBucket << "/" << objectKey << std::endl;

    Aws::S3::Model::GetObjectOutcome get_object_outcome =
        s3Client.GetObject(object_request);

    if (get_object_outcome.IsSuccess())
    {
        std::vector<std::string> pathComponents = splitString(std::string(objectKey),'/');
        std::string fileName = pathComponents.back();

        // Write retreived file to the baseDir
        auto& retrieved_file = get_object_outcome.GetResultWithOwnership().
            GetBody();
        std::fstream image_file(baseDir + fileName, std::ios::out | std::ios::binary);
        image_file << retrieved_file.rdbuf();

        // Call prediction code and store results
        /*************************************************************************************/
        std::list<Prediction> predictionsList = makePredictions(
            baseDir + fileName,
            baseDir + "model.ts"
        );

        char* uuid = storeImageProcessingMetaData(dynamoClient,cognitoClient,objectKey,uploadTableName);
        // NOTE: this first status code isn't used
        //            --> you want to run the rest of this function call no matter what
        int code = storePredictions(dynamoClient, predTableName, uuid, predictionsList);
        /*************************************************************************************/

        // Remove the retreived file from the baseDir
        code = std::remove((baseDir + fileName).c_str());
        if (code == 0){
            std::cout << "Success Removing file " << baseDir + fileName << std::endl;
        } else {
            std::cout << "Something went wrong removing file " << baseDir + fileName << std::endl;
            return invocation_response::failure(
                "Something went wrong removing file " + baseDir + fileName,
                "EFSIOError"
            );
        }
        std::cout << "Success" << std::endl;
    }
    else
    {
        auto err = get_object_outcome.GetError();
        std::cout << "Error: GetObject: " <<
            err.GetExceptionName() << ": " << err.GetMessage() << std::endl;

        std::cout << "Failure" << std::endl;
        return invocation_response::failure(
            err.GetExceptionName() + ": " + err.GetMessage(),
            "DownloadFailure"
        );
    }

    // Not super important what gets returned here - it will only be used by the http api gateway internally
    return invocation_response::success("success", "application/text");
}

int main()
{
    using namespace Aws;
    //The Aws::SDKOptions struct contains SDK configuration options.
    //An instance of Aws::SDKOptions is passed to the Aws::InitAPI and
    //Aws::ShutdownAPI methods.  The same instance should be sent to both methods.
    SDKOptions options;
    options.loggingOptions.logLevel = Utils::Logging::LogLevel::Debug;

    //The AWS SDK for C++ must be initialized by calling Aws::InitAPI.
    InitAPI(options);
    {
        Client::ClientConfiguration config;
        config.region = Aws::Environment::GetEnv("AWS_REGION");
        // config.caFile = "/etc/pki/tls/certs/ca-bundle.crt";

        auto credentialsProvider = Aws::MakeShared<Aws::Auth::EnvironmentAWSCredentialsProvider>(TAG);
        S3::S3Client client(credentialsProvider, config);
        Aws::DynamoDB::DynamoDBClient dynamoClient(config);
        Aws::CognitoIdentityProvider::CognitoIdentityProviderClient cognitoClient(config);


        auto handler_fn = [&client,&dynamoClient,&cognitoClient](aws::lambda_runtime::invocation_request const& req) {
            return my_handler(req, client, dynamoClient, cognitoClient);
        };
        run_handler(handler_fn);
    }

    //Before the application terminates, the SDK must be shut down.
    ShutdownAPI(options);
    return 0;
}