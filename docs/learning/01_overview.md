# BitShares Core 学习指南：总览

## 项目是什么

BitShares Core是一个**去中心化金融区块链节点**，用C++14实现，由 Cryptonomex（Daniel Larimer创立）开发，后由社区维护。它不只是一个简单的转账链，而是一个**链上交易所**（DEX），原生支持：

- 用户发行资产（UIA）
- 锚定资产（SmartCoin / BitAsset，如bitUSD，由抵押担保）
- 链上限价订单撮合
- 流动性池（AMM）
- 信贷/借贷（SameT Fund、Credit Offer）
- 机密转账（Confidential Transfers，基于 Pedersen Commitment）
- HTLC（哈希时间锁合约，跨链原子互换）

代币符号`BTS`，精度`100000`（5位小数），最大供应量10^15。

---

## 技术栈一览

| 层次          | 技术                                                         |
|-------------|------------------------------------------------------------|
| **语言**      | C++14（少量C++17特性）                                           |
| **构建**      | CMake + Make，可选 ccache                                     |
| **并发**      | Boost.Asio + fc协程（fiber/stackful coroutine），基于ucontext     |
| **序列化**     | 自研fc::raw（二进制，Little-Endian）+ fc::variant（JSON兼容）          |
| **反射**      | 自研FC_REFLECT 宏（编译期静态反射）                                    |
| **加密**      | OpenSSL（AES/SHA/RIPEMD160/DH/ECDH）+ secp256k1-zkp（签名/范围证明） |
| **P2P网络**   | 自研graphene::net，底层Boost.Asio TCP                           |
| **RPC/API** | WebSocket（websocketpp）+ JSON-RPC                           |
| **数据库**     | 自研内存对象数据库（graphene::db），带undo历史，持久化到文件                     |
| **测试**      | Boost.Test，分组为fixture-based test suite                     |
| **文档**      | Doxygen                                                    |

---

## 两个可执行程序

```
programs/
├── witness_node/    # 区块链全节点：P2P同步 + 出块 + RPC服务
└── cli_wallet/      # 命令行钱包：连接节点的WebSocket客户端
```

`witness_node`是核心，承担：

- 从P2P网络同步区块
- 验证并应用交易
- 如果配置了见证人密钥，参与出块
- 通过WebSocket暴露JSON-RPC API

`cli_wallet`是轻客户端，持有私钥，通过WebSocket连到节点，用节点API广播签名交易。

---

## 库依赖层次（由底向上）

```
fc                        ← 最底层：工具库（异步/加密/序列化/日志）
├── protocol              ← 协议层：操作/交易/类型定义（不访问数据库）
├── db                    ← 通用对象数据库框架
│   └── chain             ← 区块链业务逻辑（继承db，实现所有操作的执行器）
│       ├── net           ← P2P网络层
│       ├── app           ← 组合 chain+net+plugins，暴露RPC API
│       │   ├── wallet    ← 钱包逻辑
│       │   └── plugins/  ← 可选插件（历史/市场/ES等）
│       └── utilities     ← 版本信息等杂项
```

每一层只向上依赖，不向下引用。`protocol` 层最干净，可以单独用于构造/解析交易而无需启动节点。

---

## 学习路线建议

1. **区块链基础** → `03_blockchain.md`：DPoS 共识、区块结构、硬分叉机制
2. **对象数据库** → `04_database.md`：对象 ID 系统、索引、undo 历史
3. **操作与执行器** → `05_operations.md`：全部 77 个操作、evaluator 模式
4. **密码学** → `06_cryptography.md`：secp256k1 签名、哈希函数、AES、Pedersen Commitment
5. **C++ 技法** → `07_cpp_patterns.md`：fc::fwd pimpl、FC_REFLECT、static_variant、协程
6. **P2P 网络** → `08_network.md`：节点发现、消息传播、fork 处理
7. **第三方库** → `09_third_party.md`：Boost/OpenSSL/secp256k1-zkp/websocketpp 的使用方式
8. **插件系统** → `10_plugins.md`：如何扩展节点功能
