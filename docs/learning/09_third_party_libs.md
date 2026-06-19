# 第三方库

## 1. Boost（核心依赖）

BitShares深度依赖Boost，以下是实际使用的模块：

### Boost.Asio

**用途**：异步I/O核心，TCP网络、定时器。

```cpp
#include <boost/asio.hpp>

// fc 的协程调度底层
boost::asio::io_service io_service;
boost::asio::ip::tcp::resolver resolver(io_service);
boost::asio::ip::tcp::socket socket(io_service);
```

fc 在 Boost.Asio 上构建了协程调度层（`fc::async`），使异步回调风格的代码能以同步方式书写。

### Boost.MultiIndex

**用途**：对象数据库的多维索引。

```cpp
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>

// 见 04_database.md 中的 account_multi_index_type
```

这是BitShares对象数据库能高效多维查询的核心。`multi_index_container`维护多个视图，是Boost中最精密的容器之一。

### Boost.Filesystem

**用途**：路径操作、目录创建、文件枚举。

```cpp
#include <boost/filesystem.hpp>
fc::path data_dir = "witness_node_data_dir";
boost::filesystem::create_directories(data_dir.generic_string());
```

注意：`recursive_directory_iterator::level()`已废弃，项目中已改为`depth()`（见 BUGS.md）。

### Boost.Program_options

**用途**：命令行参数解析（`witness_node`和`cli_wallet`启动参数）。

```cpp
// programs/witness_node/main.cpp
po::options_description app_options("Application Options");
app_options.add_options()
    ("help,h", "Print this help message and exit.")
    ("rpc-endpoint,r", po::value<string>(), "Endpoint for wallet websocket RPC to listen on")
    ;
```

### Boost.Context（协程）

**用途**：用户态协程（fc::fiber）的底层，提供 `make_fcontext` / `jump_fcontext`。

保存/恢复CPU寄存器（包括栈指针），实现协程的 `yield` / `resume`，是fc协程的最底层机制。

### Boost.Bind

**用途**：函数绑定，包装回调。

```cpp
// 注意：Boost 1.73+ 全局占位符废弃，使用限定名
#include <boost/bind/bind.hpp>
using boost::placeholders::_1;
```

项目已升级到 `boost::placeholders::_1` 形式（见 BUGS.md）。

### Boost.PP（预处理器宏）

**用途**：`FC_REFLECT` 等宏内部大量使用。

```cpp
#include <boost/preprocessor/seq/for_each.hpp>
// BOOST_PP_SEQ_FOR_EACH 遍历宏参数序列
// FC_REFLECT(Type, (field1)(field2)...) 展开时用到
```

### Boost.Endian

**用途**：字节序无关的整数读写。

```cpp
#include <boost/endian/buffers.hpp>
boost::endian::little_uint64_buf_at buf;
// 无论宿主机字节序，buf 始终以 little-endian 存储
```

fc::raw 序列化使用小端序，通过 Boost.Endian 确保跨平台一致。

---

## 2. OpenSSL

**版本支持**：1.0.2 / 1.1.x / 3.0+（通过版本守卫条件编译）

项目使用 OpenSSL 的以下功能：

### 哈希函数（EVP 接口）

```cpp
#include <openssl/evp.h>

EVP_MD_CTX* ctx = EVP_MD_CTX_new();
EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
EVP_DigestUpdate(ctx, data, len);
EVP_DigestFinal_ex(ctx, hash_out, &hash_len);
EVP_MD_CTX_free(ctx);
```

| 函数         | OpenSSL 算法对象                         |
|------------|--------------------------------------|
| SHA-1      | `EVP_sha1()`                         |
| SHA-224    | `EVP_sha224()`                       |
| SHA-256    | `EVP_sha256()`                       |
| SHA-512    | `EVP_sha512()`                       |
| RIPEMD-160 | `EVP_ripemd160()`（需 legacy provider） |

### 椭圆曲线（EC_GROUP / EC_POINT）

```cpp
#include <openssl/ec.h>

// 在 OpenSSL 3.0 中，高层 EVP 接口推荐
EVP_PKEY* pkey = EVP_EC_gen("secp256k1");
BIGNUM* bn = nullptr;
EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_PRIV_KEY, &bn);

// EC_GROUP / EC_POINT（所有版本可用，未废弃）
EC_GROUP* group = EC_GROUP_new_by_curve_name(NID_secp256k1);
EC_POINT* point = EC_POINT_new(group);
EC_POINT_oct2point(group, point, data, len, ctx);
```

### AES（对称加密）

```cpp
#include <openssl/evp.h>

EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv);
EVP_EncryptUpdate(ctx, out, &out_len, in, in_len);
EVP_EncryptFinal_ex(ctx, out + out_len, &final_len);
```

### DH（Diffie-Hellman，OpenSSL 3.0 迁移）

```cpp
// OpenSSL 3.0+ 使用 EVP_PKEY_CTX
EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_DH, nullptr);
EVP_PKEY_keygen_init(ctx);
// ...

// OpenSSL 1.x 使用旧接口
DH* dh = DH_new();
DH_generate_parameters_ex(dh, bits, DH_GENERATOR_2, nullptr);
```

项目已通过版本守卫 `#if OPENSSL_VERSION_NUMBER >= 0x30000000L` 处理 API 差异。

---

## 3. secp256k1-zkp

**路径**：`libraries/fc/vendor/secp256k1-zkp`

比特币官方 `libsecp256k1` 的扩展版，专门为机密交易设计。

### 标准 secp256k1 功能

```c
// 创建签名（比 OpenSSL 的 ECDSA_sign 快 ~10x）
secp256k1_ecdsa_sign_compact(ctx, msg32, sig64, key32, nonce_fn, nonce_data, &recid);

// 验证/恢复公钥
secp256k1_ecdsa_recover_compact(ctx, msg32, sig64, pubkey33, &pubkey_len, 1, recid);

// 公钥运算
secp256k1_ec_pubkey_tweak_add(ctx, pubkey, tweak);  // P = P + tweak*G（HD 派生用）
secp256k1_ec_privkey_tweak_add(ctx, key, tweak);    // k = k + tweak
```

### ZKP 扩展功能（机密转账）

```c
// Pedersen Commitment
secp256k1_pedersen_commit(ctx, commit, blind, value);

// 盲因子求和（验证交易平衡）
secp256k1_pedersen_blind_sum(ctx, blind_out, blinds, n_blinds, n_positive);

// 验证承诺总和为零（验证无中生有）
secp256k1_pedersen_verify_tally(ctx, commits, n, neg_commits, m, excess);

// 范围证明（证明金额在 [0, 2^64) 不泄露值）
secp256k1_rangeproof_sign(ctx, proof, &proof_len, min_value,
                          commit, blind, nonce, exp, bits, value);
secp256k1_rangeproof_verify(ctx, &min_val, &max_val, commit, proof, proof_len);
```

**构建方式**：通过 CMake 的 `ExternalProject_Add` 调用 autoconf/make 构建为静态库：

```cmake
ExternalProject_Add(
        project_secp256k1
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/secp256k1-zkp
        CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/vendor/secp256k1-zkp/configure
        --prefix=${SECP256K1_INSTALL_DIR}
        --with-bignum=no
        --disable-tests        # ← 避免编译 tests.c（含废弃 API）
        BUILD_COMMAND make
        INSTALL_COMMAND make install
)
```

---

## 4. websocketpp

**路径**：`libraries/fc/vendor/websocketpp`

纯头文件 C++ WebSocket 实现，基于 Boost.Asio。

### 关键特性

- 实现 RFC 6455（WebSocket 协议）
- 支持 TLS（通过 `boost::asio::ssl`）
- 支持 WebSocket Server 和 Client 两种角色
- 纯 C++ 模板，无外部依赖（除 Boost.Asio）

### 在 BitShares 中的封装层次

```
websocketpp（底层 WebSocket 帧处理）
    ↓
fc::http::websocket_server（fc 封装）
    ↓
fc::rpc::websocket_api_connection（JSON-RPC 分发）
    ↓
fc::api<database_api>（自动生成 RPC 方法）
    ↓
database_api（实际业务逻辑）
```

`fc::api<T>` 通过 FC_REFLECT 遍历 T 的所有方法，自动为每个方法生成 JSON-RPC handler，实现"定义 C++ 类即拥有 WebSocket API"
的效果。

### 项目对 websocketpp 的修改

1. `#include <boost/bind.hpp>` → `#include <boost/bind/bind.hpp>`（Boost 1.73+ 兼容）
2. `using ::_1/2/3` → `using boost::placeholders::_1/2/3`（废弃全局占位符）
3. `ci_less::nocase_compare` 移除废弃的 `std::binary_function` 继承

---

## 5. miniz

**路径**：`libraries/fc/src/compress/miniz.c`

单文件 zlib 兼容压缩库（C 语言实现），MIT 许可。

**用途**：

- P2P 消息压缩
- 快照文件压缩

项目修改：

- `TDEFL_PUT_BITS` 宏中的 `if` 加花括号，消除 `-Wmisleading-indentation` 警告（见 BUGS.md）

---

## 6. editline

**路径**：`libraries/fc/vendor/editline`

命令行编辑库（readline 的开源替代），用于 `cli_wallet` 的交互式命令行，支持历史记录、行编辑、Tab 补全。

---

## 版本与兼容性矩阵

| 库             | 最低版本  | 当前验证版本               | 说明                 |
|---------------|-------|----------------------|--------------------|
| Boost         | 1.57  | 1.83（Ubuntu 24.04）   | 1.73+ 需更新 bind 占位符 |
| OpenSSL       | 1.0.2 | 3.0.13（Ubuntu 24.04） | 3.0+ 大量 API 废弃，已迁移 |
| GCC           | 4.9   | 13.3（Ubuntu 24.04）   | 13 有新警告，已修复        |
| CMake         | 3.8   | 3.28                 |                    |
| secp256k1-zkp | —     | 0.1（fork）            | 从源码编译              |
