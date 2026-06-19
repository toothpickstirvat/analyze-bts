# 区块链机制：共识、区块与交易

## 1. 共识机制：DPoS（委托权益证明）

BitShares 使用 **DPoS（Delegated Proof of Stake）**，不是 PoW 挖矿，而是持币人投票选出**见证人（Witness）**轮流出块。

### 出块流程

```
持币人投票
    ↓
选出 N 个活跃见证人（默认 21 个，由链上参数控制）
    ↓
见证人按调度轮流出块（round-robin with randomization）
    ↓
每个 slot 3 秒（区块间隔，可配置 1-30 秒）
    ↓
下一个 slot 的见证人必须在窗口内广播区块
    ↓
如果错过出块，链继续等待，记录 missed block
```

关键代码：`libraries/chain/db_witness_schedule.cpp`

### 不可逆确认

交易在什么时候是"确认"的？

```
GRAPHENE_IRREVERSIBLE_THRESHOLD = 70%（活跃见证人的 70% 出块后则不可逆）
```

也就是说，21 个见证人中至少 15 个出块之后，一个区块才变为不可逆。通常需要等约 45 秒。

### 与 PoW 的根本区别

|       | PoW        | DPoS         |
|-------|------------|--------------|
| 安全假设  | 算力诚实 > 50% | 投票权诚实 > 70%  |
| 出块速度  | 慢（10分钟+）   | 快（3秒）        |
| 能耗    | 极高         | 极低           |
| 中心化风险 | 矿池集中       | 超级节点集中       |
| 攻击成本  | 购买算力       | 购买足够多代币并控制投票 |

---

## 2. 区块结构

```cpp
// libraries/protocol/include/graphene/protocol/block.hpp

struct block_header {
    block_id_type      previous;        // 父区块的 RIPEMD160(block_data)
    fc::time_point_sec timestamp;       // 出块时间（精度 1 秒）
    witness_id_type    witness;         // 出块者的对象 ID（1.6.x）
    checksum_type      transaction_merkle_root;  // 交易默克尔根
    extensions_type    extensions;
};

struct signed_block_header : public block_header {
    signature_type witness_signature;   // 见证人用私钥对 block_header 签名
};

struct signed_block : public signed_block_header {
    vector<processed_transaction> transactions;  // 区块内所有交易
};
```

区块 ID = `RIPEMD160(block_header_data)`，这也是为什么 `block_id_type` 是 `fc::ripemd160`（20 字节）。

### Merkle 树

交易默克尔根用来快速验证区块中某笔交易的存在（SPV 验证的基础）：

```
           Root
          /    \
        H12    H34
       /   \   /  \
      H1   H2 H3  H4
      |    |  |   |
     tx1  tx2 tx3 tx4
```

相关文档：`docs/SPV.md`

---

## 3. 交易结构

```cpp
// libraries/protocol/include/graphene/protocol/transaction.hpp

struct transaction {
    uint16_t           ref_block_num;    // TaPOS：引用的区块号低 16 位
    uint32_t           ref_block_prefix; // TaPOS：引用的区块 ID 前 4 字节
    fc::time_point_sec expiration;       // 过期时间（最多 24 小时后）
    vector<operation>  operations;       // 操作列表（可以多个）
    extensions_type    extensions;
};

struct signed_transaction : public transaction {
    vector<signature_type> signatures;  // 满足所有操作权限需求的签名集合
};
```

### TaPOS（Transaction as Proof of Stake）

`ref_block_num` + `ref_block_prefix` 引用一个最近的区块，这防止了重放攻击：

- 交易必须引用最近 `GRAPHENE_MAX_TIME_UNTIL_EXPIRATION`（24小时）内的区块
- 不同链的区块 ID 不同，交易无法跨链重放
- 配合 `expiration` 字段：过期后交易作废，防止延迟重放

### 手续费（Fee Schedule）

每个操作有手续费，手续费结构定义在 `fee_schedule.hpp`，委员会可以通过投票调整。手续费用 BTS（或指定资产）支付，部分销毁，部分进入储备金。

---

## 4. 权限与签名系统（多重签名）

BitShares 的权限系统比比特币复杂得多，支持**多签**和**权限委托**。

每个账户有两层权限：

```cpp
struct authority {
    uint32_t                              weight_threshold; // 需要达到的总权重
    flat_map<account_id_type, weight_type> account_auths;   // 委托给其他账户
    flat_map<public_key_type, weight_type> key_auths;       // 直接持有公钥
    flat_map<address, weight_type>         address_auths;   // 比特币格式地址（遗留）
};

struct account_object {
    authority owner;   // 最高权限，用于修改账户本身
    authority active;  // 日常操作权限
    // ...
};
```

**例子**：2-of-3 多签

```
active.weight_threshold = 2
active.key_auths = {
    (pubkey_A, 1),
    (pubkey_B, 1),
    (pubkey_C, 1)
}
```

发送交易时，需要携带任意两个私钥的签名，总权重 >= 2 即可。

验证逻辑：`libraries/chain/transaction_evaluation_state.cpp`

### Custom Authority（BSIP-40）

允许账户将特定操作的权限委托给其他密钥，且可以附加条件限制（如只能转账到特定账户、只能转账不超过一定金额）：

```cpp
// libraries/protocol/include/graphene/protocol/custom_authority.hpp
struct custom_authority_create_operation {
    account_id_type    account;
    bool               enabled;
    authority          auth;         // 被委托的权限方
    operation_type     operation_type; // 只允许哪种操作
    vector<restriction> restrictions; // 附加限制
};
```

---

## 5. 硬分叉系统

BitShares 通过**时间戳**控制新功能的启用，而不是区块高度（因为出块速度可变）。

### 定义方式

```bash
# libraries/chain/hardfork.d/BSIP_85.hf
#define HARDFORK_BSIP_85_TIME (fc::time_point_sec( 1596117300 ))
#define HARDFORK_BSIP_85_PASSED(now) (now >= HARDFORK_BSIP_85_TIME)
```

### 使用方式

在 evaluator 代码中：

```cpp
void limit_order_create_evaluator::do_apply(const limit_order_create_operation& op) {
    const auto& d = db();
    
    if( HARDFORK_BSIP_85_PASSED(d.head_block_time()) ) {
        // 新逻辑：允许 fill-or-kill 订单
    } else {
        // 老逻辑
    }
}
```

`hardfork.hpp` 是自动生成的（cmake 将 `hardfork.d/` 下所有 `.hf` 文件拼接），**不要直接修改**。

### 为什么用时间而不是块高？

用区块高度的问题：如果出块速度改变（例如 3 秒改为 2 秒），按高度激活的时间点就变了。用 UTC 时间戳，激活时刻明确，所有节点只需对比
`head_block_time()` 就能同步激活，不需要额外协调。

---

## 6. 创世区块与链 ID

```cpp
// libraries/chain/include/graphene/chain/genesis_state.hpp
struct genesis_state_type {
    vector<initial_account_type>  initial_accounts;   // 初始账户和公钥
    vector<initial_asset_type>    initial_assets;     // 初始资产
    vector<initial_witness_type>  initial_witnesses;  // 初始见证人
    chain_parameters              initial_parameters; // 初始链参数
    // ...
};
```

**Chain ID** = `SHA256(genesis_state_serialized)`

这意味着创世配置不同，链 ID 就不同。测试网和主网有不同的链 ID，TaPOS 引用的区块也不同，交易无法跨网络重放。

---

## 7. Fork 处理

节点可能同时收到不同的区块分支（fork）：

```
主链:  A → B → C → D
分叉:  A → B → C' → D' → E'
```

`fork_database`（`libraries/chain/fork_database.cpp`）维护所有已知分支，选择**累积难度最高**的链（DPoS 中即最长链）作为主链。

切换分支时：

1. **undo**：回滚当前主链上比分叉点新的所有区块（利用 undo 历史）
2. **apply**：应用新分支上的区块

undo 历史的实现见 `04_database.md`。
