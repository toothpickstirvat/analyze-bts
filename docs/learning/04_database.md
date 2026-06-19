# 对象数据库：graphene::db

## 核心思想

BitShares 的状态（账户余额、订单、资产等）全部保存在一个**内存对象数据库**中，定期 flush 到磁盘。这个数据库不是 SQLite 或
LevelDB，而是完全自研的，特点是：

1. **强类型**：每种对象有固定的 C++ 类型
2. **多索引**：每种对象可以有多个查询维度（类似 SQL 的多列索引）
3. **Undo 历史**：支持多层回滚（用于处理分叉切换）
4. **对象 ID 系统**：所有对象用 `a.b.c` 格式寻址

---

## 对象 ID 系统

所有链上对象用三元组 `(space, type, instance)` 唯一标识，序列化为 `"a.b.c"` 字符串：

```cpp
// libraries/db/include/graphene/db/object_id.hpp
template<uint8_t SpaceID, uint8_t TypeID>
struct object_id {
    uint64_t instance;  // 序号，从 0 开始自增
};
```

| 空间   | 含义             | 例子                     |
|------|----------------|------------------------|
| `1`  | 协议空间（出现在网络传输中） | `1.2.x` 账户, `1.3.x` 资产 |
| `2`  | 实现空间（仅节点内部）    | `2.0.x` 全局属性           |
| `3`+ | 插件专用空间         | `2.7.x` 操作历史           |

### 完整类型表（协议空间 1）

```
1.0.x  null_object
1.1.x  base_object
1.2.x  account_object           ← 账户
1.3.x  asset_object             ← 资产定义
1.4.x  force_settlement_object  ← 强制结算订单
1.5.x  committee_member_object  ← 委员会成员
1.6.x  witness_object           ← 见证人
1.7.x  limit_order_object       ← 限价订单
1.8.x  call_order_object        ← 保证金借贷
1.9.x  custom_authority_object  ← 自定义权限
1.10.x proposal_object          ← 提案
1.11.x operation_history_object ← 操作历史
1.12.x withdraw_permission_object
1.13.x vesting_balance_object   ← 归属期余额
1.14.x worker_object            ← 工作提案
1.15.x balance_object           ← 余额（创世分配）
1.16.x htlc_object              ← 哈希时间锁合约
1.17.x credit_offer_object      ← 信贷报价
1.18.x credit_deal_object       ← 信贷交易
1.19.x liquidity_pool_object    ← 流动性池
1.20.x samet_fund_object        ← SameT 基金
1.21.x ticket_object            ← 投票权证
```

在 C++ 代码中，用 `account_id_type`（typedef 为 `object_id<1, 2>`）表示账户 ID，避免混用不同类型的 ID。

---

## 对象定义示例

```cpp
// libraries/chain/include/graphene/chain/account_object.hpp

class account_object : public graphene::db::abstract_object<account_object,
                                                             protocol_ids,   // space = 1
                                                             account_object_type> // type = 2
{
public:
    static constexpr uint8_t space_id = protocol_ids;
    static constexpr uint8_t type_id  = account_object_type;

    string          name;
    authority       owner;
    authority       active;
    account_options options;
    // ...
};
```

继承 `abstract_object` 自动获得 `id`（类型为 `account_id_type`）字段。

---

## 索引系统（multi_index）

数据库使用 Boost.MultiIndex 为每种对象维护多个查询维度：

```cpp
// 账户索引的定义
struct by_name {};
struct by_id {};

typedef multi_index_container<
    account_object,
    indexed_by<
        ordered_unique<tag<by_id>,   member<object, object_id_type, &object::id>>,
        ordered_unique<tag<by_name>, member<account_object, string, &account_object::name>>
    >
> account_multi_index_type;

typedef generic_index<account_object, account_multi_index_type> account_index;
```

查询示例：

```cpp
const auto& idx = db.get_index_type<account_index>();

// 按 ID 查询（O(log n)）
const account_object& acc = db.get<account_object>( account_id_type(0) );

// 按名字查询
const auto& by_name_idx = idx.indices().get<by_name>();
auto it = by_name_idx.find( "alice" );
```

### Boost.MultiIndex 原理

`multi_index_container` 内部用多棵 B 树（`ordered_*`）或哈希表（`hashed_*`）维护同一份数据的不同视图，插入/删除一次，所有索引自动更新，保证各索引间的一致性。

这是 BitShares 能高效支持"按账户名找账户"、"按资产符号找资产"等多维查询的关键。

---

## Undo 历史：可回滚的数据库

这是最精妙的设计之一。每次修改对象时，数据库记录修改前的状态，允许回滚到任意之前的快照。

```cpp
// libraries/db/include/graphene/db/undo_database.hpp

struct undo_state {
    map<object_id_type, unique_ptr<object>> old_values;  // 修改前的值
    map<object_id_type, unique_ptr<object>> removed;     // 被删除的对象
    vector<object_id_type>                  new_ids;     // 新创建的对象 ID
};
```

### 操作流程

```cpp
// 开始一个可撤销的会话
auto session = db._undo_db.start_undo_session();

// 修改对象（会自动记录 undo 信息）
db.modify( account, [&]( account_object& a ) {
    a.statistics(db).modify( db, [&]( account_statistics_object& s ) {
        s.total_core_in_orders += op.amount.amount;
    });
});

// 成功时提交（清除 undo 信息）
session.merge();

// 失败时回滚（恢复到修改前状态）
session.undo();  // 析构时如果没有 merge() 也会自动调用
```

### 与数据库 ACID 的对应

| ACID             | BitShares db                   |
|------------------|--------------------------------|
| Atomicity（原子性）   | undo session：一个交易要么全部成功，要么完整回滚 |
| Consistency（一致性） | evaluator 中的约束检查               |
| Isolation（隔离性）   | 单线程执行，无并发修改                    |
| Durability（持久性）  | flush() 写磁盘                    |

### 应用于区块链

处理一个区块时：

```
begin_undo_session()
  for each tx in block:
    begin_undo_session()
      apply_transaction(tx)
    merge_session()   ← 交易成功则合并
  合并所有交易的 undo 信息为"区块级 undo"
```

当需要切换分叉时，依次 undo 每个区块，恢复到分叉点之前的状态，再 apply 新分支的区块。

---

## 数据库的持久化

内存状态定期写入文件系统：

```
witness_node_data_dir/
├── blockchain/          ← 区块文件（原始二进制区块数据）
│   └── database/
│       └── block_num_to_block/
├── object_database/     ← 对象数据库快照
└── config.ini
```

对象数据库的序列化：每种对象类型的所有实例序列化为一个文件，重启时全部加载进内存。这也是节点启动时"Replaying blockchain"
需要时间的原因（需要重建内存索引）。

---

## 与节点 API 的关系

`graphene::chain::database` 继承自 `graphene::db::object_database`，并在其上叠加了区块链业务逻辑（验证、执行操作、出块等）：

```
graphene::db::object_database    ← 通用对象存储、undo
         ↑ 继承
graphene::chain::database        ← 区块链业务：apply_block, apply_transaction
         ↑ 聚合
graphene::app::application       ← 网络 + API 的组合体
```

访问对象的路径在代码中随处可见：

```cpp
// evaluator 中通过 db() 访问数据库
const auto& d = db();
const account_object& from = d.get<account_object>( op.from );
```
