# About

C++ source code for predicting classifications, and storing those predictions in dynamodb.  Can modify to just run, or use with lambda.  Adapted from [detectron2](https://github.com/facebookresearch/detectron2/blob/main/tools/deploy/README.md)

# Running The Code
```
    $ docker build -t dlcpp -f Dockerfile .
    $ # NOTE: Need the --env-file flag for aws connections
    $ docker run --env-file ./.env --rm -it dlcpp:latest /home/appuser/lambda_predictor/build/testPredictAndStore
```


# References:
* https://docs.docker.com/engine/reference/commandline/run/#set-environment-variables--e---env---env-file
* https://discourse.cmake.org/t/getting-undefined-reference-error-despite-of-having-the-library-specified-in-cmake/1888/2
* https://github.com/awslabs/aws-lambda-cpp/blob/master/examples/dynamodb/main.cpp
* https://github.com/awslabs/aws-lambda-cpp/tree/master/examples/dynamodb
* https://docs.aws.amazon.com/sdk-for-cpp/v1/developer-guide/cmake-params.html#cmake-custom-memory-management
* https://github.com/aws/aws-sdk-cpp
* https://sdk.amazonaws.com/cpp/api/LATEST/aws-cpp-sdk-dynamodb_2include_2aws_2dynamodb_2model_2_attribute_value_8h_source.html


# Some deploy snippets

RUN aws iam attach-role-policy \
    --role-name lambda-cpp-demo \
    --policy-arn arn:aws:iam::aws:policy/AmazonS3ReadOnlyAccess


RUN aws lambda create-function \
    --function-name encode-file \
    --role <arn returned from above> \
    --runtime provided \
    --timeout 15 \
    --memory-size 128 \
    --handler encoder \
    --zip-file fileb:///encoder.zip
