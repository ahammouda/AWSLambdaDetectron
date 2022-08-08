FROM ubuntu:bionic-20220531

# ************** Mostly detectron specific setup **************
# NOTE: This actually seems to work, just must have lots of memory allocated for it
# Previously installed before torchvision
ENV DEBIAN_FRONTEND noninteractive
RUN apt-get update && apt-get install -y \
	python3-opencv ca-certificates python3-dev git wget sudo ninja-build \
    libopencv-dev \
    # Next 2 lines AWS Specific
    build-essential manpages-dev unzip \
    uuid-dev libssl-dev libpulse-dev libcurl4-openssl-dev autoconf libtool
RUN ln -sv /usr/bin/python3 /usr/bin/python

# Optionally: remove the current version of cmake
# RUN sudo apt remove cmake

# install non-default version of cmake: https://github.com/Kitware/CMake/releases/tag/v3.12.4
# (required for many of the following builds to work)
RUN wget https://github.com/Kitware/CMake/releases/download/v3.12.4/cmake-3.12.4.tar.gz
RUN tar -zxf cmake-3.12.4.tar.gz
RUN cd cmake-3.12.4 && ./bootstrap && make -j 4 && make install

# setup manual curl:
RUN git clone https://github.com/curl/curl.git
RUN cd curl && autoreconf -fi && ./configure --with-openssl && make && make install

# create a non-root user
ARG USER_ID=1000
RUN useradd -m --no-log-init --system  --uid ${USER_ID} appuser -g sudo
RUN echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers
USER appuser
WORKDIR /home/appuser

ENV PATH="/home/appuser/.local/bin:${PATH}"
RUN wget https://bootstrap.pypa.io/pip/3.6/get-pip.py && \
	python3 get-pip.py --user && \
	rm get-pip.py

# ************** AWS specific **************
RUN pip3 install awscli

USER root
WORKDIR /

# install C++ runtime environment
RUN cd / && git clone https://github.com/awslabs/aws-lambda-cpp.git
RUN mkdir /aws-lambda-cpp/build && cd /aws-lambda-cpp/build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_INSTALL_PREFIX=/out -DCMAKE_CXX_COMPILER=g++ && \
    make && make install

# install sdk
RUN git clone https://github.com/aws/aws-sdk-cpp.git
RUN cd aws-sdk-cpp && git submodule update --init --recursive
RUN cd aws-sdk-cpp && mkdir build && cd build && \
    cmake .. -DBUILD_ONLY="s3;dynamodb;cognito-idp" \
     -DBUILD_SHARED_LIBS=OFF \
     -DENABLE_UNITY_BUILD=ON \
     -DCMAKE_BUILD_TYPE=Release \
     -DCMAKE_INSTALL_PREFIX=/out \
     -DCUSTOM_MEMORY_MANAGEMENT=0 && \
    make -j 4 && make install

# ************** detectron2 specific **************
# install torvhvision dependencies:
# Need to build 1.12 with abi as that is last documented compatibility with torchvision
# Need to build with-abi as you will get linking error when building with aws-cpp-sdk o/w
RUN wget https://download.pytorch.org/libtorch/cpu/libtorch-cxx11-abi-shared-with-deps-1.12.0%2Bcpu.zip
#https://download.pytorch.org/libtorch/cpu/libtorch-cxx11-abi-shared-with-deps-1.12.0.zip
RUN unzip libtorch-cxx11-abi-shared-with-deps-1.12.0+cpu.zip

USER appuser
ENV HOME=/home/appuser
WORKDIR $HOME

RUN git clone --branch v0.13.0 https://github.com/pytorch/vision/
RUN mkdir vision/build && cd vision/build && \
	cmake .. -DCMAKE_INSTALL_PREFIX=$HOME/.local -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_PREFIX_PATH="/libtorch;$HOME/.local/lib/python3.6/site-packages/torch/" && \
	make -j 4 VERBOSE=1 && make install

# make our installation take effect
ENV CPATH=$HOME/.local/include \
	  LIBRARY_PATH=$HOME/.local/lib \
	  LD_LIBRARY_PATH=$HOME/.local/lib

USER root
WORKDIR /

RUN mkdir $HOME/lambda_predictor
ADD src/CMakeLists.txt $HOME/lambda_predictor
ADD src/prediction.cpp $HOME/lambda_predictor
ADD src/prediction.hpp $HOME/lambda_predictor
ADD src/predictAndStore.cpp $HOME/lambda_predictor
ADD src/storage.hpp $HOME/lambda_predictor
ADD src/storage.cpp $HOME/lambda_predictor
ADD src/utils.hpp $HOME/lambda_predictor
ADD src/utils.cpp $HOME/lambda_predictor
ADD src/lambda_main.cpp $HOME/lambda_predictor
# uncomment next 2 lines except for local usage
# ADD egdata/b60cfeea70c3c7045141eadce552135921b95679.png $HOME/lambda_predictor # only uncomment if not testing w/s3
ADD egdata/model.ts $HOME/lambda_predictor

# RelWithDebInfo \
RUN mkdir $HOME/lambda_predictor/build && cd $HOME/lambda_predictor/build && \
    cmake -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH="/out;$HOME/.local/;/libtorch/" \
    -DCMAKE_CXX_STANDARD_LIBRARIES=-luuid .. \
    && make

# Setup requirements for lambda runtime

# # Help lambda runtime find torchlib - very weird that it's only this library that needs an extra nudge,
# # but checks out to some extent with docs:
# # https://stackoverflow.com/questions/66370682/aws-lambda-not-finding-shared-object-file-in-layer
# # https://docs.aws.amazon.com/lambda/latest/dg/configuration-envvars.html
RUN cp $HOME/.local/lib/libtorchvision.so /lib/
# Entrypoint
CMD ["/home/appuser/lambda_predictor/build/main"]