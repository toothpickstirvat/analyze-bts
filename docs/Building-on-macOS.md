BitShares-Core Build Instructions for macOS
===========================================

1. Install XCode and its command line tools by following the instructions here: https://guide.macports.org/#installing.xcode. 
   In OS X 10.11 (El Capitan) and newer, you will be prompted to install developer tools when running a developer command in the terminal. This step may not be needed.

2. Install Homebrew by following the instructions here: http://brew.sh/

3. Initialize Homebrew:
   ```
   brew doctor
   brew update
   ```

4. Install dependencies:
   ```
   brew install cmake git autoconf automake libtool
   ```

   Note:
   As mentioned elsewhere, BitShares-Core depends on the third-party libraries "Boost" and "OpenSSL". These libraries need to be in certain version ranges. At the moment, Boost needs to be between 1.58 and 1.74. OpenSSL needs to be in the 1.0.x or 1.1.x range.

   * Boost:
     It's likely that the default boost version in brew has issues or not yet supported by us. If you just want to build BitShares-Core, use an older version of boost from our own HomeBrew Tap:
     ```
     brew install bitshares/boost/boost@1.69
     ```
     If necessary, link it as the default boost library:
     ```
     brew link --force --overwrite boost@1.69
     ```

     Nevertheless, if you would like to experiment or test newer boost libraries, you can try:
     ```
     brew install boost
     ```


   * OpenSSL:
     As of writing, the latest OpenSSL in brew is OpenSSL v3, unfortunately it is not supported by us. You may install an older version of OpenSSL with brew:
     ```
     brew install openssl@1.1
     ```
     If necessary, link it as the default openssl library:
     ```
     brew link --force --overwrite openssl@1.1
     ```

5. *Optional.* To support importing Bitcoin wallet files:
   ```
   brew install berkeley-db
   ```

6. *Optional.* To use TCMalloc in LevelDB:
   ```
   brew install google-perftools
   ```

7. Clone the BitShares-Core repository:
   ```
   git clone https://github.com/bitshares/bitshares-core.git
   cd bitshares-core
   ```

8. Build BitShares-Core:
   ```
   git submodule update --init --recursive
   cmake .
   make
   ```

   Note:
   To compile with non-default versions of Boost and/or OpenSSL, we need to tell cmake where these libraries are. Instead of the "cmake ." mentioned above, we use (for example):
   ```
   cmake -DBOOST_ROOT=/usr/local/opt/boost@1.69 -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl@1.1 .
   ```
   and then proceed with the normal
   ```
   make
   ```