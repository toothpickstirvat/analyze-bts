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
