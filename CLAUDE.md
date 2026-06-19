# CLAUDE.md

本文件为 Claude Code (claude.ai/code) 在此仓库中工作时提供指导。

## 构建

**依赖安装（Ubuntu）：**
```
sudo apt-get install autoconf cmake make automake libtool git libboost-all-dev libssl-dev g++ libcurl4-openssl-dev doxygen
```

**构建步骤：**
```bash
git submodule update --init --recursive
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

调试构建使用 `cmake -DCMAKE_BUILD_TYPE=Debug ..`。CI 使用 ccache，设置 `CCACHE_DIR` 并在 cmake 中加 `-DCMAKE_CXX_COMPILER_LAUNCHER=ccache`。

**构建单个目标：**
```bash
make -j4 chain_test    # 单元测试
make -j4 cli_test      # CLI 钱包测试
make -j4 app_test      # 应用 API 测试
make -j4 es_test       # Elasticsearch 测试
make -j4 witness_node  # 节点程序
make -j4 cli_wallet    # 钱包程序
```

## 运行测试

所有测试二进制文件位于 `build/tests/`：

```bash
# 并行运行所有链单元测试（需要 GNU parallel）
libraries/fc/tests/run-parallel-tests.sh build/tests/chain_test -l test_suite

# 运行单个测试用例
build/tests/chain_test -t basic_tests/valid_name_test

# 运行整个测试套件
build/tests/chain_test -t basic_tests

# 应用 API 测试
build/tests/app_test -l test_suite

# CLI 钱包测试
build/tests/cli_test -l test_suite

# Elasticsearch 测试（需要 ES 在 9200 端口运行）
build/tests/es_test -l test_suite
# 自定义 ES 地址：
GRAPHENE_TESTING_ES_URL=http://127.0.0.1:9201/ build/tests/es_test -l test_suite
```

测试使用 Boost.Test，组织方式为 `BOOST_FIXTURE_TEST_SUITE(套件名, database_fixture)` + `BOOST_AUTO_TEST_CASE(用例名)`。指定单个测试：`-t 套件名/用例名`。

测试之间清理临时文件：`rm -rf /tmp/graphene*`

## 架构概览

这是 **BitShares 区块链节点**（C++14），由一组库构成，对外提供两个程序：
- `programs/witness_node` — 区块链节点（P2P + RPC 服务器）
- `programs/cli_wallet` — 命令行钱包客户端

### 库层次（由底向上）

| 库 | 用途 |
|---|---|
| `libraries/fc` | 子模块：底层 C++ 工具（异步、序列化、加密、JSON、variant 类型） |
| `libraries/protocol` | 网络传输类型：操作、交易、对象 ID，不访问数据库 |
| `libraries/db` | 通用对象数据库（索引、undo 历史），提供 `graphene::db::database` |
| `libraries/chain` | 区块链逻辑：`database`（继承自 `db`）、所有链对象和 evaluator |
| `libraries/net` | P2P 网络层 |
| `libraries/app` | 将 chain+net+plugins 组合成可运行的应用，暴露节点 API |
| `libraries/wallet` | CLI 钱包逻辑，通过 WebSocket 与节点通信 |
| `libraries/plugins/*` | 节点启动时按需加载的可选模块 |

### 对象 ID 系统

所有对象使用 `a.b.c` 格式，其中 `a` = 空间，`b` = 类型，`c` = 实例序号：
- 空间 `1` = 协议空间（出现在网络传输中）：`1.2.x` 账户、`1.3.x` 资产、`1.7.x` 限价单等
- 空间 `2` = 实现空间（仅内部使用）

完整类型列表见 `libraries/protocol/include/graphene/protocol/types.hpp`（协议空间）和 `libraries/chain/include/graphene/chain/types.hpp`（实现空间）。

### 操作（Operation）与执行器（Evaluator）

操作定义在 `libraries/protocol/include/graphene/protocol/` 下，每个操作族一个文件（如 `transfer.hpp`、`market.hpp`）。所有操作汇总为 `fc::static_variant<...>`，定义在 `operations.hpp` 中。

每个操作对应一个 **evaluator**，位于 `libraries/chain/include/graphene/chain/`，包含实际的数据库业务逻辑。新增操作的步骤：
1. 在 `libraries/protocol/include/graphene/protocol/` 中定义操作结构体
2. 添加 `FC_REFLECT` 序列化宏
3. 将操作加入 `operations.hpp` 中的 `operation` static_variant
4. 在 `libraries/chain/` 中实现 evaluator
5. 在 `libraries/chain/db_init.cpp` 中注册 evaluator

### 硬分叉系统

硬分叉时间定义在 `libraries/chain/hardfork.d/` 下的 `.hf` 文件中。`libraries/chain/include/graphene/chain/hardfork.hpp` 是**自动生成的**，不要直接修改。每个 `.hf` 文件定义一个 `HARDFORK_X_TIME` 宏和 `HARDFORK_X_PASSED(now)` 谓词，用于在 evaluator 和数据库代码中控制新功能的启用时机。

### 插件

插件位于 `libraries/plugins/`，每个插件：
- 在启动时向 `graphene::app::application` 注册自身
- 可定义额外的数据库对象（使用独立的空间 ID）
- 可暴露额外的 API 方法

创建新插件骨架：`libraries/plugins/make_new_plugin.sh <名称>`

### 新开发者必读文件

- `libraries/protocol/include/graphene/protocol/operations.hpp` — 所有操作的主列表
- `libraries/protocol/include/graphene/protocol/types.hpp` — 所有对象类型 ID
- `tests/common/database_fixture.hpp` — 测试基础类，展示如何构建和提交交易
- `libraries/app/include/graphene/app/database_api.hpp` — 节点数据库 API 定义
- `libraries/app/include/graphene/app/api.hpp` — 其他节点 API 定义
- `libraries/wallet/include/graphene/wallet/wallet.hpp` — 所有钱包 API 命令

### 序列化

使用 `fc` 提供的 `FC_REFLECT(类型, (字段1)(字段2)...)` 宏。需要外部模板实例化的类型（加速编译），在头文件中用 `GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION`，在 `.cpp` 文件中用 `GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION`。

### fc 异步 / 线程

异步任务用 `fc::async(callable, "名称")`，延迟任务用 `fc::schedule(callable, "名称", 时间)`。`static_variant<T1,T2>` 是可辨别联合类型，用 `.get<T>()` 向下转型。

## 运行节点

```bash
# 在 build 目录下执行：
./programs/witness_node/witness_node --rpc-endpoint=127.0.0.1:8090

# 首次运行会自动创建 witness_node_data_dir/ 及 config.ini
```

节点 API 通过 RPC 端点的 WebSocket 或 HTTP 访问。API 集 `0` = `database`（只读），API 集 `1` = `login`。
