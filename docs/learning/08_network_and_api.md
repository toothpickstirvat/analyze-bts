# 网络层与 API

## 1. P2P 网络架构（graphene::net）

```
libraries/net/
├── include/graphene/net/
│   ├── node.hpp          ← 节点接口（node_delegate 抽象类）
│   ├── peer_database.hpp ← 已知节点的持久化存储
│   ├── core_messages.hpp ← P2P 消息类型定义
│   └── message.hpp       ← 消息封装
└── node.cpp              ← 实现（node_impl，隐藏细节）
```

### node_delegate：节点与应用的解耦

P2P 层（`graphene::net::node`）不直接依赖区块链层，而是通过**抽象接口** `node_delegate` 与应用通信：

```cpp
// libraries/net/include/graphene/net/node.hpp
class node_delegate {
public:
    // 是否已有某个区块/交易
    virtual bool has_item( const net::item_id& id ) = 0;

    // 收到新区块时调用
    virtual bool handle_block( const graphene::net::block_message& blk_msg,
                               bool sync_mode,
                               std::vector<message_hash_type>& tx_ids ) = 0;

    // 收到新交易时调用
    virtual void handle_transaction( const graphene::net::trx_message& trx_msg ) = 0;

    // 同步时请求特定 item
    virtual message get_item( const item_id& id ) = 0;

    // 获取 chain id（用于对等方握手校验）
    virtual graphene::protocol::chain_id_type get_chain_id() const = 0;
};
```

`graphene::app::application` 实现了 `node_delegate`，将 P2P 层收到的消息路由到 `chain::database`。

---

## 2. P2P 消息类型

```cpp
// libraries/net/include/graphene/net/core_messages.hpp

// 区块消息
struct block_message {
    graphene::protocol::signed_block  block;
    block_id_type                     block_id;
};

// 交易消息
struct trx_message {
    graphene::protocol::signed_transaction trx;
};

// 握手消息（连接建立时交换）
struct hello_message {
    std::string        user_agent;
    uint32_t           core_protocol_version;
    fc::ip::address    inbound_address;
    uint16_t           inbound_port;
    chain_id_type      chain_id;  // 不同链会拒绝连接
    fc::sha256         node_id;   // 节点唯一标识
};
```

所有消息类型都注册了 FC_REFLECT，通过 fc::raw 序列化后在 TCP 连接上传输。消息格式：`[4字节长度][4字节类型ID][消息体]`。

---

## 3. 节点发现（Peer Discovery）

```
1. 从 seed_nodes 列表连接已知节点（config.ini 中配置）
2. 连接后交换 peer_list（各自已知的其他节点地址）
3. 尝试连接新发现的节点
4. 已知节点持久化到 peer_database（peer.json）
```

节点 ID = SHA256(公钥) 或随机生成，用于 Kademlia 风格的 DHT（尽管 BitShares 的 P2P 不是完整 DHT）。

---

## 4. 区块同步流程

```
新节点连接到网络：

1. 握手（hello_message）：交换 chain_id、当前块高、最新块 ID
2. 发现自己落后，发送 fetch_blockchain_item_ids_message 请求缺失的区块列表
3. 对等方返回 blockchain_item_ids_inventory_message（区块 ID 列表）
4. 节点按顺序发送 fetch_items_message 请求每个区块
5. 对等方返回 item_not_available_message 或实际 block_message
6. 节点验证并应用每个区块，更新本地链
7. 追上最新块后切换到正常模式（转发新块/交易）
```

同步时调用 `handle_block(..., sync_mode=true)`，跳过部分验证以加速。

---

## 5. 交易广播

```cpp
// cli_wallet 发送一笔转账的路径

// 1. 钱包构建并签名交易
signed_transaction tx = wallet.transfer("alice", "bob", "100 BTS", ...);

// 2. 通过 WebSocket 发送给节点
// network_broadcast_api::broadcast_transaction(tx)

// 3. 节点验证后加入内存池（mempool），广播给其他节点
// node.broadcast_message( trx_message{tx} )

// 4. 对等节点收到后验证，转发给各自连接的节点（Gossip 协议）

// 5. 下一个出块见证人打包进区块
```

BitShares 的 P2P 广播是**Gossip 协议**：每个节点收到新交易/区块后，转发给所有已连接的对等节点（去重）。

---

## 6. WebSocket JSON-RPC API

节点通过 WebSocket 暴露多组 API，客户端用 JSON-RPC 2.0 协议调用：

```json
// 请求
{
  "id": 1,
  "method": "call",
  "params": [
    0,
    "get_account",
    [
      "alice"
    ]
  ]
}
//                                       ^ API集合编号

// 响应
{
  "id": 1,
  "result": {
    "id": "1.2.100",
    "name": "alice",
    ...
  }
}
```

**API 集合编号**：

- `0` — `database_api`（只读，无需登录）
- `1` — `login_api`（用于获取其他 API 的访问权）

通过 `login_api::login(user, password)` 后可以获取：

- `network_broadcast_api`（广播交易）
- `network_node_api`（P2P 管理，需要 admin 权限）
- `history_api`（账户历史查询）
- `block_api`（区块数据查询）
- `asset_api`（资产查询）
- 各插件暴露的 API（市场历史、Elasticsearch 等）

---

## 7. database_api：主要方法

```cpp
// libraries/app/include/graphene/app/database_api.hpp

class database_api {
public:
    // 链信息
    chain_id_type             get_chain_id() const;
    global_property_object    get_global_properties() const;
    dynamic_global_property_object get_dynamic_global_properties() const;

    // 账户
    vector<optional<account_object>> get_accounts(const vector<account_id_type>&) const;
    vector<account_object>           lookup_accounts(const string& start, uint32_t limit) const;

    // 资产
    vector<optional<asset_object>> get_assets(const vector<asset_id_type>&) const;

    // 订单簿
    order_book                get_order_book(const string& base, const string& quote, unsigned limit = 50) const;
    vector<limit_order_object> get_limit_orders(asset_id_type a, asset_id_type b, uint32_t limit) const;

    // 余额
    vector<asset>             get_account_balances(account_id_type id, const flat_set<asset_id_type>& assets) const;

    // 区块
    optional<signed_block>    get_block(uint32_t block_num) const;
    optional<signed_transaction> get_transaction(uint32_t block_num, uint32_t trx_in_block) const;
};
```

---

## 8. WebSocket 通知（订阅）

database_api 支持回调通知，无需轮询：

```cpp
// 订阅账户更新
db_api.set_subscribe_callback(callback_fn, clear_filter);
db_api.subscribe_to_objects(callback_fn, {"1.2.100"});  // 订阅 alice 的账户对象

// 每当该账户有任何变化，节点主动推送新状态给客户端
```

实现上，database 在 `apply_block` / `apply_transaction` 后检查哪些被修改的对象有订阅者，通过 WebSocket 异步推送。

---

## 9. websocketpp — 第三方 WebSocket 库

```
libraries/fc/vendor/websocketpp/
```

websocketpp 是纯头文件 C++ WebSocket 实现，基于 Boost.Asio，支持：

- RFC 6455 WebSocket 协议
- TLS（通过 Boost.Asio SSL）
- 服务端和客户端模式

fc 封装了 websocketpp，暴露了 `fc::rpc::websocket_connection` 和 JSON-RPC 调度层 (`fc::api<T>`)。`fc::api<T>` 通过
FC_REFLECT 自动把 T 的所有成员函数暴露为 JSON-RPC 方法，调用方看到的就是普通 C++ 函数调用。

---

## 10. 插件 API

每个插件可以向节点注册额外的 API 对象：

```cpp
// 市场历史插件（market_history_plugin）
class market_history_api {
public:
    // 获取 K 线数据
    vector<bucket_object> get_market_history(
        asset_id_type a, asset_id_type b,
        uint32_t bucket_seconds,
        fc::time_point_sec start, fc::time_point_sec end
    ) const;

    // 获取支持的时间粒度
    flat_set<uint32_t> get_market_history_buckets() const;
};
```

客户端通过 `login_api::get_market_history()` 获取这个 API 的句柄（编号），之后用该编号调用方法。
