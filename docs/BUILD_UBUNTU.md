Note: this documents is a copy of the "Build" section of [README.md](https://github.com/bitshares/bitshares-core/blob/master/README.md#build), it may be stale, please check README.md for latest info.

# Ubuntu (64-bit) Build and Install Instructions
As of writing, we support building on Ubuntu 16.04, 18.04 and 20.04 LTS releases.
The following dependencies are recommended for a clean install on Ubuntu (64-bit):

    sudo apt-get update
    sudo apt-get install autoconf cmake make automake libtool git libboost-all-dev libssl-dev g++ libcurl4-openssl-dev doxygen

## Build BitShares Core

    git clone https://github.com/bitshares/bitshares-core.git
    cd bitshares-core
    git submodule update --init --recursive
    cmake -DCMAKE_BUILD_TYPE=Release .
    make 

## Build Support Boost Version
NOTE: BitShares Core requires a Boost version in the range [1.58 - 1.74]. Newer versions may work, but have not been tested. If your system came pre-installed with a version of Boost that you do not wish to use, you may manually build your preferred version and use it with BitShares by specifying it on the CMake command line. Example:

    cmake -DBOOST_ROOT=/path/to/boost -DCMAKE_BUILD_TYPE=Release .
    make
