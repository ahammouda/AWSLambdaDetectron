#include <vector>
#include <sstream>

#include <aws/lambda-runtime/runtime.h>
#include "storage.hpp" // "prediction.hpp" is necessarily included in storage.hpp


void FileExists(std::string path){
    using namespace std;

    DIR *dr;
    struct dirent *en;
    dr = opendir(path.c_str()); //open all directory
    if (dr) {
        while ((en = readdir(dr)) != NULL) {
            cout<<" \n"<<en->d_name; //print all directory name
        }
        closedir(dr); //close all directory
        cout << "\n No More Iteration" << endl;
    } else {
        cout<<"Director Not Found"<<endl;
    }
}

// // https://stackoverflow.com/questions/10058606/splitting-a-string-by-a-character
// std::vector<std::string> splitString(std::string textString,  std::string delimeter) {
//     std::stringstream textStream(textString);
//     std::vector<std::string> seglist;

//     std::string segment;
//     while(std::getline(textStream, segment, delimeter))
//     {
//         seglist.push_back(segment);
//     }

//     return seglist;
// }


using namespace aws::lambda_runtime;

char const TAG[] = "LAMBDA_ALLOC";

static invocation_response my_handler(invocation_request const& req,
                                      Aws::S3::S3Client const& s3Client,
                                      Aws::DynamoDB::DynamoDBClient const& dynamoClient) {

    auto const& tableName = Aws::Environment::GetEnv("TABLE_NAME");
    auto const& baseDir = Aws::Environment::GetEnv("MODEL_DIR");

    std::cout << "Table Name:  " << tableName << std::endl;
    std::cout << "MNT Dir:  " << baseDir << std::endl;

    // Log existence/access to mount dir
    FileExists( baseDir );

    using namespace Aws::Utils::Json;
    JsonValue json(req.payload);
    if (!json.WasParseSuccessful()) {
        return invocation_response::failure("Failed to parse input JSON", "InvalidJSON");
    }

    auto v = json.View();
    if (!v.ValueExists("Records") ||
        // v.GetArray("Records").size() == 0 ||
        !v.GetArray("Records")[0].ValueExists("s3") ||
        !v.GetArray("Records")[0].GetObject("s3").ValueExists("bucket")||
        !v.GetArray("Records")[0].GetObject("s3").ValueExists("object")) {
        return invocation_response::failure("Missing input value s3bucket or s3key", "InvalidJSON");
    }

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

    AWS_LOGSTREAM_INFO(TAG, "Attempting to download file from s3://" << fromBucket << "/" << objectKey);

    Aws::S3::Model::GetObjectOutcome get_object_outcome =
        s3Client.GetObject(object_request);

    if (get_object_outcome.IsSuccess())
    {
        // // TODO: remove these lines
        // auto& file_ext = splitString(objectKey,".").back();
        // std::cout << "file extension is: " << file_ext <<std::endl;

        // Write retreived file to the baseDir
        auto& retrieved_file = get_object_outcome.GetResultWithOwnership().
            GetBody();
        std::fstream image_file(baseDir + objectKey, std::ios::out | std::ios::binary);
        image_file << retrieved_file.rdbuf();

        // TODO: remove this line
        FileExists("/mnt/ml/models/");

        // Call prediction code and store results
        /*************************************************************************************/
        std::list<Prediction> predictionsList = makePredictions(
            baseDir + objectKey,
            baseDir + "model.ts"
        );

        // TODO: do you want to do something with this first code?
        int code = storePredictions(dynamoClient, tableName, predictionsList);
        /*************************************************************************************/

        // Remove the retreived file from the baseDir
        code = std::remove((baseDir + objectKey).c_str());
        if (code == 0){
            std::cout << "Success Removing file " << baseDir + objectKey << std::endl;
        } else {
            std::cout << "Something went wrong removing file " << baseDir + objectKey << std::endl;
            return invocation_response::failure(
                "Something went wrong removing file " + baseDir + objectKey,
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

std::function<std::shared_ptr<Aws::Utils::Logging::LogSystemInterface>()> GetConsoleLoggerFactory()
{
    return [] {
        return Aws::MakeShared<Aws::Utils::Logging::ConsoleLogSystem>(
            "console_logger", Aws::Utils::Logging::LogLevel::Trace);
    };
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

        auto handler_fn = [&client,&dynamoClient](aws::lambda_runtime::invocation_request const& req) {
            return my_handler(req, client, dynamoClient);
        };
        run_handler(handler_fn);
    }

    //Before the application terminates, the SDK must be shut down.
    ShutdownAPI(options);
    return 0;
}