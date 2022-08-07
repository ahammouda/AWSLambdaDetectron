# Copyright (c) Facebook, Inc. and its affiliates.
# See https://pytorch.org/tutorials/advanced/cpp_frontend.html
cmake_minimum_required(VERSION 3.5 FATAL_ERROR) # .12
project(lambda_predictor LANGUAGES CXX)

# utils
add_library(utils SHARED
  utils.hpp utils.cpp
)

# ****** Setup predictions code and dependencies ******
find_package(Torch REQUIRED)
find_package(TorchVision REQUIRED)   # needed by export-method=tracing/scripting
find_package(OpenCV REQUIRED)
find_package(Python3 COMPONENTS Development)

add_library(predictions SHARED
  prediction.hpp prediction.cpp
)
target_link_libraries(predictions
  "${TORCH_LIBRARIES}" -W TorchVision::TorchVision -W ${OpenCV_LIBS} utils)


# ****** Setup dynamo storage code and dependencies ******
find_package(AWSSDK REQUIRED COMPONENTS s3 dynamodb)

add_library(dynamoStorage SHARED storage.hpp storage.cpp)
target_link_libraries(dynamoStorage AWS::aws-lambda-runtime ${AWSSDK_LINK_LIBRARIES} utils)

# ****** Build main entrypoints for functionality ******
find_package(aws-lambda-runtime REQUIRED)

# ****** Build harness for local testing ******
add_executable(testPredictAndStore predictAndStore.cpp)
target_link_libraries(
  testPredictAndStore
  predictions
  dynamoStorage
  utils
  AWS::aws-lambda-runtime
)
set_property(TARGET testPredictAndStore PROPERTY CXX_STANDARD 14)

# ****** Build lambda entrypoint ******
add_executable(main lambda_main.cpp)
target_link_libraries(
  main
  predictions
  dynamoStorage
  utils
  AWS::aws-lambda-runtime
)
set_property(TARGET main PROPERTY CXX_STANDARD 14)