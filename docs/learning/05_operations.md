# 操作（Operation）与执行器（Evaluator）

## 设计思路

BitShares 把所有能改变链状态的行为抽象为"操作（operation）"，每个操作是一个 C++ 结构体，包含操作所需的所有参数。交易（transaction）是操作的有序列表。

这种设计的优点：

- 每种操作独立定义，便于审计
- 新增功能只需新增操作类型，不影响已有逻辑
- 序列化/反射自动处理，不需要手写协议解析代码
- 操作类型编号（0、1、2...）是协议的一部分，**一旦确定不能改变**

---

## 操作的完整列表（共 77 个）

```
# 转账与资产
 0  transfer_operation              转账
 8  account_upgrade_operation       升级为终身会员
14  asset_issue_operation           发行资产
15  asset_reserve_operation         销毁资产

# 限价订单（DEX 核心）
 1  limit_order_create_operation    创建限价单
 2  limit_order_cancel_operation    取消限价单
 3  call_order_update_operation     更新保证金（更新抵押）
 4  fill_order_operation            [VIRTUAL] 订单成交（由引擎生成，不能广播）

# 账户管理
 5  account_create_operation        创建账户
 6  account_update_operation        更新账户信息
 7  account_whitelist_operation     白名单管理
 9  account_transfer_operation      账户所有权转让

# 资产管理
10  asset_create_operation          创建资产
11  asset_update_operation          更新资产参数
12  asset_update_bitasset_operation 更新智能资产参数
13  asset_update_feed_producers_operation 更新喂价人
16  asset_fund_fee_pool_operation   向手续费池注资
17  asset_settle_operation          资产结算
18  asset_global_settle_operation   全局结算（紧急）
19  asset_publish_feed_operation    发布价格喂价
43  asset_claim_fees_operation      提取资产手续费
47  asset_claim_pool_operation      提取资产池资金
48  asset_update_issuer_operation   更换资产发行人
42  asset_settle_cancel_operation   [VIRTUAL] 取消结算

# 见证人
20  witness_create_operation        注册见证人
21  witness_update_operation        更新见证人信息

# 提案（链上治理）
22  proposal_create_operation       创建提案
23  proposal_update_operation       更新提案（投票）
24  proposal_delete_operation       删除提案

# 自动提款授权
25  withdraw_permission_create_operation
26  withdraw_permission_update_operation
27  withdraw_permission_claim_operation
28  withdraw_permission_delete_operation

# 委员会
29  committee_member_create_operation
30  committee_member_update_operation
31  committee_member_update_global_parameters_operation

# 归属期余额
32  vesting_balance_create_operation
33  vesting_balance_withdraw_operation

# 工作提案
34  worker_create_operation

# 扩展
35  custom_operation                自定义数据（插件用）
36  assert_operation                断言检查
37  balance_claim_operation         领取创世余额
38  override_transfer_operation     强制转账（仅发行人）

# 机密转账（Stealth）
39  transfer_to_blind_operation     转入隐私账户
40  blind_transfer_operation        隐私内部转账
41  transfer_from_blind_operation   从隐私账户转出

# FBA（Fee Backed Asset）
44  fba_distribute_operation        [VIRTUAL]

# 抵押竞拍
45  bid_collateral_operation        抵押物出价
46  execute_bid_operation           [VIRTUAL]

# HTLC（哈希时间锁）
49  htlc_create_operation           创建 HTLC
50  htlc_redeem_operation           赎回 HTLC
51  htlc_redeemed_operation         [VIRTUAL]
52  htlc_extend_operation           延长 HTLC
53  htlc_refund_operation           [VIRTUAL]

# 自定义权限（BSIP-40）
54  custom_authority_create_operation
55  custom_authority_update_operation
56  custom_authority_delete_operation

# 权益代币（Staking）
57  ticket_create_operation
58  ticket_update_operation

# 流动性池（AMM，BSIP-77）
59  liquidity_pool_create_operation
60  liquidity_pool_delete_operation
61  liquidity_pool_deposit_operation
62  liquidity_pool_withdraw_operation
63  liquidity_pool_exchange_operation
75  liquidity_pool_update_operation

# SameT Fund（闪电贷）
64  samet_fund_create_operation
65  samet_fund_delete_operation
66  samet_fund_update_operation
67  samet_fund_borrow_operation
68  samet_fund_repay_operation

# Credit Offer（信贷）
69  credit_offer_create_operation
70  credit_offer_delete_operation
71  credit_offer_update_operation
72  credit_offer_accept_operation
73  credit_deal_repay_operation
74  credit_deal_expired_operation   [VIRTUAL]
76  credit_deal_update_operation
```

标注 `[VIRTUAL]` 的操作由区块链引擎内部生成，**不能由用户广播**，只出现在操作历史中以记录链上事件（如订单自动成交）。

---

## 操作结构定义示例

```cpp
// libraries/protocol/include/graphene/protocol/transfer.hpp

struct transfer_operation : public base_operation {
    struct fee_parameters_type {
        uint64_t fee       = 20 * GRAPHENE_BLOCKCHAIN_PRECISION;  // 默认手续费
        uint32_t price_per_kbyte = 10 * GRAPHENE_BLOCKCHAIN_PRECISION;  // 每KB额外费用
    };

    asset             fee;         // 手续费（必须 >= calculate_fee()）
    account_id_type   from;        // 转出账户
    account_id_type   to;          // 转入账户
    asset             amount;      // 转账金额和资产类型
    fc::optional<memo_data> memo;  // 可选的加密备注
    extensions_type   extensions;

    // 必须实现的接口
    account_id_type fee_payer()const { return from; }
    void            validate()const;                  // 基本参数验证（不访问数据库）
    share_type      calculate_fee(const fee_parameters_type& k)const;
};

FC_REFLECT( graphene::protocol::transfer_operation,
            (fee)(from)(to)(amount)(memo)(extensions) )
```

所有操作继承 `base_operation`，必须提供：

- `fee_payer()`：谁付手续费
- `validate()`：不访问数据库的基本校验（范围检查等）
- `calculate_fee()`：手续费计算

---

## 执行器（Evaluator）模式

每个操作对应一个 evaluator，负责**访问数据库、验证业务逻辑、修改状态**。

```cpp
// 头文件声明
// libraries/chain/include/graphene/chain/transfer_evaluator.hpp
class transfer_evaluator : public evaluator<transfer_evaluator> {
public:
    typedef transfer_operation operation_type;

    void_result do_evaluate( const transfer_operation& op );
    void_result do_apply   ( const transfer_operation& op );
};
```

```cpp
// 实现
// libraries/chain/transfer_evaluator.cpp

void_result transfer_evaluator::do_evaluate( const transfer_operation& op )
{
    const auto& d = db();
    
    // 1. 验证账户存在
    const account_object& from_account = op.from(d);
    const account_object& to_account   = op.to(d);
    
    // 2. 验证资产存在且可转账
    const asset_object& asset_type = op.amount.asset_id(d);
    FC_ASSERT( asset_type.can_be_transferred() );
    
    // 3. 验证余额足够
    FC_ASSERT( d.get_balance(from_account, asset_type) >= op.amount + op.fee );
    
    return void_result();
}

void_result transfer_evaluator::do_apply( const transfer_operation& op )
{
    const auto& d = db();
    
    // 扣款
    d.adjust_balance( op.from, -op.amount );
    // 收款
    d.adjust_balance( op.to, op.amount );
    
    return void_result();
}
```

### 执行分两步的原因

`do_evaluate`（评估）和 `do_apply`（应用）分开的好处：

1. **一个交易里的多个操作**：先对所有操作 `evaluate`，全部通过才 `apply`，否则整个交易回滚
2. **提案预验证**：提案中的操作可以在提案创建时 `evaluate`，提前发现问题
3. **清晰的职责分离**：evaluate 只读，apply 才写

### 执行流程

```cpp
// database::apply_transaction 中的简化逻辑
void database::apply_transaction( const signed_transaction& tx )
{
    auto session = _undo_db.start_undo_session();
    
    for( const operation& op : tx.operations ) {
        auto eval = get_evaluator( op );     // 按类型找到对应 evaluator
        eval->evaluate( op );                // do_evaluate
    }
    for( const operation& op : tx.operations ) {
        auto eval = get_evaluator( op );
        eval->apply( op );                   // do_apply
    }
    
    session.merge();  // 成功则提交
}
```

### 注册 Evaluator

在 `db_init.cpp` 中注册，操作类型编号与 evaluator 一一对应：

```cpp
// libraries/chain/db_init.cpp
void database::initialize_evaluators()
{
    _operation_evaluators.resize(255);
    register_evaluator<transfer_evaluator>();
    register_evaluator<limit_order_create_evaluator>();
    register_evaluator<limit_order_cancel_evaluator>();
    // ...
}
```

---

## 新增操作的步骤

1. 在 `libraries/protocol/include/graphene/protocol/` 中定义操作结构体 + `FC_REFLECT`
2. 在 `operations.hpp` 的 `fc::static_variant<...>` 末尾追加新操作类型
3. 在 `libraries/chain/include/graphene/chain/` 中声明 evaluator 类
4. 在 `libraries/chain/` 中实现 evaluator 的 `do_evaluate` 和 `do_apply`
5. 在 `db_init.cpp` 中注册 evaluator
6. 在 `hardfork.d/` 中定义激活时间
7. 在 `do_evaluate/do_apply` 中用 `HARDFORK_X_PASSED(d.head_block_time())` 控制启用

---

## 流动性池（AMM）的工作原理

`liquidity_pool_exchange_operation` 实现了类似 Uniswap v1 的恒积 AMM：

```
x * y = k  （恒积公式）
```

其中 `x` 和 `y` 是池中两种资产的数量。用户换入 `Δx`，换出 `Δy`：

```
(x + Δx) * (y - Δy) = k
Δy = y - k / (x + Δx) = y * Δx / (x + Δx)
```

实际还要扣除手续费（万分之几，由池创建者设定）。

代码：`libraries/chain/liquidity_pool_evaluator.cpp`
