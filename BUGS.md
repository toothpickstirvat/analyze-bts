# 错误与 Bug 记录

记录开发过程中遇到的编译错误、运行时 bug 及其解决方案。

---

## 编译错误

### [2026-06-19] fc/string.hpp 缺少 `<cstdint>`

**环境：** Ubuntu 24.04 / GCC 13.3

**错误信息：**
```
libraries/fc/include/fc/string.hpp:11:3: error: 'uint64_t' does not name a type
libraries/fc/include/fc/string.hpp:14:35: error: 'std::string fc::to_string' redeclared as different kind of entity
libraries/fc/include/fc/reflect/typename.hpp:124:57: error: call of overloaded 'to_string(long unsigned int)' is ambiguous
```

**根因：**
`fc/string.hpp` 使用了 `uint64_t`、`uint32_t` 等整数类型，但未包含 `<cstdint>`。旧版 GCC 会通过 `<string>` 等头文件间接引入这些类型，GCC 13 收紧了头文件依赖，不再间接引入，导致类型未定义，进而引发重载集破坏和 `typename.hpp` 中的歧义错误。

**修复：** 在 `libraries/fc/include/fc/string.hpp` 中添加：
```cpp
#include <cstdint>
```

**修改位置：** `libraries/fc/include/fc/string.hpp` 第 4 行（`#include <string>` 之前）

---

## [2026-06-19] OpenSSL 3.0 兼容问题（fc/src/crypto/openssl.cpp）

**环境：** Ubuntu 24.04 / OpenSSL 3.0.13

### 错误：`FIPS_mode_set` 未声明（编译错误）

**错误信息：**
```
openssl.cpp:49:11: error: 'FIPS_mode_set' was not declared in this scope
```

**根因：** `FIPS_mode_set` 在 OpenSSL 3.0 中被彻底移除，FIPS 改用 provider 机制。原有条件编译只排除了 LibreSSL，未排除 OpenSSL 3.0+。

**修复：** 在 `libraries/fc/src/crypto/openssl.cpp` 第 46 行，将条件改为同时排除 LibreSSL 和 OpenSSL 3.0+：
```cpp
// 改前
#if not defined(LIBRESSL_VERSION_NUMBER)
// 改后
#if !defined(LIBRESSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER < 0x30000000L
```

### `DH_free` 已废弃 → 迁移到 EVP_PKEY API（已修复）

**警告信息：**
```
openssl.cpp:78: warning: 'void DH_free(DH*)' is deprecated: Since OpenSSL 3.0
```

**根因：** OpenSSL 3.0 将整个 `DH` 系列 API 标记为废弃，推荐改用 `EVP_PKEY` / `EVP_PKEY_CTX` API。

**修复（三处改动）：**

1. `libraries/fc/include/fc/crypto/openssl.hpp`：用 `#if OPENSSL_VERSION_NUMBER < 0x30000000L` 包裹 `SSL_TYPE_DECL(ssl_dh, DH)`
2. `libraries/fc/src/crypto/openssl.cpp`：用同样条件包裹 `SSL_TYPE_IMPL(ssl_dh, DH, DH_free)`
3. `libraries/fc/src/crypto/dh.cpp`：用 `#if OPENSSL_VERSION_NUMBER >= 0x30000000L` 分支，将四个函数（`generate_params`、`validate`、`generate_pub_key`、`compute_shared_key`）改用 `EVP_PKEY_CTX` / `OSSL_PARAM_BLD` / `EVP_PKEY_derive` 等新 API 实现；`#else` 分支保留原有 OpenSSL 1.x 代码

---

## [2026-06-19] fc git_revision.cpp 编译错误：`HEAD-HASH-NOTFOUND` 被当作 C++ 标识符

**错误信息：**
```
build/libraries/fc/git_revision.cpp:4: error: 'HEAD' was not declared in this scope
#define FC_GIT_REVISION_UNIX_TIMESTAMP HEAD-HASH-NOTFOUND
```

**根因：**
`libraries/fc/.git` 是子模块指针文件，指向 `../../.git/modules/libraries/fc`，但该路径不存在（子模块 git 数据目录未初始化）。cmake 脚本 `GetGitRevisionDescription.cmake` 中 `get_git_unix_timestamp` 找不到 git hash 时，将回退值字符串 `"HEAD-HASH-NOTFOUND"` 直接替换进 `#define`，C++ 编译器将其解析为三个未定义标识符相减。

**修复：**
1. `libraries/fc/GitVersionGen/GetGitRevisionDescription.cmake`：将 `get_git_unix_timestamp` 中无法获取 hash 时的回退值从 `"HEAD-HASH-NOTFOUND"` 改为 `"0"`（合法的 `uint32_t` 字面量）
2. `build/libraries/fc/git_revision.cpp`（构建目录生成文件）：直接将 `HEAD-HASH-NOTFOUND` 改为 `0` 以立即解除编译阻塞

---

## [2026-06-19] cmake 子模块版本检查 FATAL_ERROR

**错误信息：**
```
fatal: not a git repository: .../.git/modules/docs
CMake Error: Submodule 'docs' is not up-to-date.
```

**根因：**
`docs` 和 `libraries/fc` 都是 git 子模块，但 `.git/modules/docs` 和 `.git/modules/libraries/fc` 不存在（子模块 git 数据目录未初始化）。`CMakeLists.txt` 的 `check_submodule` 函数对 git 命令失败的情况直接报 `FATAL_ERROR`。

**修复：** `CMakeLists.txt` 的 `check_submodule` 函数增加对 `git rev-parse HEAD` 返回值的检查；当子模块 git 目录不存在时（`RESULT_VARIABLE != 0`）改为 `WARNING` 并跳过，不再阻止 cmake 继续执行。

---

## [2026-06-19] GCC 13 `-Wmissing-template-keyword` 误报（fc/include/fc/safe.hpp）

**环境：** Ubuntu 24.04 / GCC 13.3

**警告信息：**
```
safe.hpp:114:12: warning: expected 'template' keyword before dependent template name [-Wmissing-template-keyword]
```

**根因：** GCC 13 在模板类内部的 `friend` 非模板函数中，对 `std::numeric_limits<T>::max()` / `::min()` 产生误报，认为需要 `template` 关键字，实际上是编译器 bug（`numeric_limits` 的成员函数不是模板）。

**修复：** 在 `libraries/fc/include/fc/safe.hpp` 中，用 `#pragma GCC diagnostic push/pop` 局部压制触发误报的三个 `friend operator+/-/*` 函数（第 112–160 行）：
```cpp
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-template-keyword"
    friend safe operator + ( ... ) { ... }
    friend safe operator - ( ... ) { ... }
    friend safe operator * ( ... ) { ... }
#pragma GCC diagnostic pop
```

---

## [2026-06-19] Boost >= 1.73 全局 bind 占位符废弃（`_1`/`_2`/`_3`）

**环境：** Ubuntu 24.04 / Boost 1.83

**提示信息：**
```
/usr/include/boost/bind.hpp:36:1: note: #pragma message: The practice of declaring the Bind
placeholders (_1, _2, ...) in the global namespace is deprecated...
```

**根因：** Boost 1.73 起，将 `_1/_2/_3` 注入全局命名空间的做法被标记为废弃，推荐使用 `boost::placeholders::_1` 等限定名。以下三处代码使用了全局占位符：
- `libraries/fc/include/fc/asio.hpp`：两处 `boost::bind(..., _1)`
- `libraries/fc/src/asio.cpp`：两处 `boost::bind(..., _1, _2)`
- `libraries/fc/vendor/websocketpp/websocketpp/common/functional.hpp`：`using ::_1; using ::_2; using ::_3;`

**修复（四处改动）：**
1. `asio.hpp`：`#include <boost/bind.hpp>` → `#include <boost/bind/bind.hpp>`；两处 `_1` → `boost::placeholders::_1`
2. `asio.cpp`：两处 `_1/_2` → `boost::placeholders::_1/::_2`
3. `websocketpp/common/functional.hpp`：`#include <boost/bind.hpp>` → `#include <boost/bind/bind.hpp>`；`using ::_1/2/3` → `using boost::placeholders::_1/2/3`
4. `CMakeLists.txt` 保留 `-DBOOST_BIND_GLOBAL_PLACEHOLDERS` 作为兜底，压制其余未改到的第三方代码警告

---

## [2026-06-19] secp256k1-zkp 测试文件使用 OpenSSL 3.0 废弃 EC_KEY API

**环境：** Ubuntu 24.04 / OpenSSL 3.0.13

**警告信息：**
```
tests.c:2261: warning: 'EC_KEY_new_by_curve_name' is deprecated: Since OpenSSL 3.0
tests.c:2263: warning: 'd2i_ECPrivateKey' is deprecated: Since OpenSSL 3.0
tests.c:2287: warning: 'ECDSA_sign' is deprecated: Since OpenSSL 3.0
...
```

**根因：** `libraries/fc/vendor/secp256k1-zkp/src/tests.c` 是 secp256k1 库自带的测试套件，使用了 OpenSSL 3.0 废弃的 `EC_KEY` / `ECDSA_*` 系列 API。主项目构建通过 `ExternalProject_Add` 调 autoconf `make`，默认会编译测试。

**修复：** `libraries/fc/CMakeLists.txt` 中的 `ExternalProject_Add` configure 命令加 `--disable-tests`，跳过 `tests.c` 的编译：
```cmake
CONFIGURE_COMMAND ... configure --prefix=... --with-bignum=no --disable-tests
```
重新构建时需先清除旧 secp256k1 构建目录：`rm -rf build/libraries/fc/vendor/secp256k1-zkp`

---

## [2026-06-19] `recursive_directory_iterator::level()` 已废弃（fc/src/filesystem.cpp）

**环境：** Ubuntu 24.04 / Boost 1.83

**警告信息：**
```
filesystem.cpp:199: warning: 'int boost::filesystem::recursive_directory_iterator::level() const'
is deprecated: Use recursive_directory_iterator::depth() instead [-Wdeprecated-declarations]
```

**根因：** Boost.Filesystem 新版将 `level()` 重命名为 `depth()`，两者语义完全相同（返回当前递归深度），旧名已废弃。

**修复：** `libraries/fc/src/filesystem.cpp` 第 199 行：
```cpp
// 改前
int recursive_directory_iterator::level() { return _p->level(); }
// 改后
int recursive_directory_iterator::level() { return _p->depth(); }
```

---
