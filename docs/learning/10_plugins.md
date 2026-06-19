# 插件系统

## 设计理念

插件让节点可以**按需加载**额外功能，而不影响核心区块链逻辑。不运行某个插件时，它不消耗任何资源。常见场景：

- 交易所节点：需要 `market_history` 和 `account_history`
- 见证人节点：只需要 `witness` 插件
- 数据分析节点：需要 `elasticsearch` 把数据推送到 ES

---

## 现有插件列表

```
libraries/plugins/
├── account_history/    ← 账户操作历史（operation_history_object）
├── api_helper_indexes/ ← 为 database_api 提供额外索引（如 asset_by_maker_fee_percent）
├── custom_operations/  ← 自定义操作索引（custom_operation 的解析和存储）
├── debug_witness/      ← 调试用：修改链参数、手动推进时间
├── delayed_node/       ← 延迟节点：总是跟随实际链的 N 个块之前的状态（用于 API 稳定性）
├── elasticsearch/      ← 把操作历史推送到 Elasticsearch
├── es_objects/         ← 把链对象推送到 Elasticsearch
├── grouped_orders/     ← 订单簿聚合（按价格分组的订单深度）
├── market_history/     ← K 线数据（OHLCV，按时间桶聚合）
├── snapshot/           ← 在指定区块高度生成快照（JSON 导出链状态）
├── template_plugin/    ← 插件骨架模板
└── witness/            ← 见证人出块（生产区块）
```

---

## 插件结构

每个插件遵循固定结构：

```
my_plugin/
├── include/graphene/my_plugin/
│   └── my_plugin.hpp           ← 插件类声明
├── my_plugin.cpp               ← 插件实现
└── CMakeLists.txt
```

### 插件类定义

```cpp
// include/graphene/market_history/market_history_plugin.hpp

namespace graphene { namespace market_history {

namespace detail { class market_history_plugin_impl; }

class market_history_plugin : public graphene::app::plugin {
public:
    // 插件名称（在 config.ini 中使用）
    std::string plugin_name() const override;

    // 插件描述
    std::string plugin_description() const override;

    // 启动时初始化（注册 config 选项）
    void plugin_set_program_options(
        boost::program_options::options_description& cli,
        boost::program_options::options_description& cfg) override;

    // 解析配置参数
    void plugin_initialize(const boost::program_options::variables_map& options) override;

    // 节点启动后调用
    void plugin_startup() override;

    // 节点关闭时调用
    void plugin_shutdown() override;

    // 暴露额外 API 对象（可选）
    flat_map<std::string, graphene::app::abstract_plugin*> get_api_plugins();

private:
    std::shared_ptr<detail::market_history_plugin_impl> my;
};

} } // namespace graphene::market_history
```

### 插件注册到 application

```cpp
// programs/witness_node/main.cpp
app.register_plugin<graphene::market_history::market_history_plugin>(true);
app.register_plugin<graphene::account_history::account_history_plugin>(true);
// ...
```

`true` 表示默认启用。客户端可以通过 `--plugins` 命令行参数覆盖启用哪些插件。

---

## 如何监听链事件

插件通过订阅 `database` 的信号来响应链事件：

```cpp
// 在 plugin_startup() 中
void market_history_plugin_impl::plugin_startup()
{
    database& db = _self.database();

    // 每次成功应用一个区块后触发
    db.applied_block.connect( [this](const signed_block& b) {
        update_market_histories(b);
    });

    // 每次成功应用一个交易后触发
    db.on_applied_transaction.connect( [this](const signed_transaction& tx, ...) {
        // 处理新交易
    });
}
```

`fc::signal<void(const signed_block&)>` 是 fc 的信号槽机制，类似 Qt 的 signal/slot，但不依赖 moc。

---

## account_history 插件详解

最常用的插件之一，记录每个账户的所有历史操作。

### 数据结构

```cpp
// 插件使用实现空间（space=2）的专有对象类型
namespace graphene { namespace chain {
    // 每个账户的操作历史链表节点
    class operation_history_object : public abstract_object<...> {
    public:
        operation_history_id_type next;  // 前一个历史操作 ID（链表）
        uint32_t                  block_num;
        uint16_t                  trx_in_block;
        uint16_t                  op_in_trx;
        operation                 op;    // 操作内容
        operation_result          result;// 操作结果
    };

    class account_transaction_history_object : public abstract_object<...> {
    public:
        account_id_type                account;
        operation_history_id_type      operation_id;
        account_transaction_history_id_type next;  // 该账户的历史链表
    };
}}
```

### 查询方式

```cpp
// database_api 调用 account_history 插件数据
vector<operation_history_object> history_api::get_account_history(
    account_id_type account,
    operation_history_id_type stop,   // 从哪个操作开始（0 表示最新）
    unsigned limit,
    operation_history_id_type start
) const;
```

---

## market_history 插件详解

存储 K 线数据（OHLCV），用于行情图表展示。

### 数据聚合

```cpp
// 按时间桶聚合成交记录
class bucket_object : public abstract_object<...> {
public:
    fc::time_point_sec  key_open;     // 桶的开始时间
    uint32_t            key_seconds;  // 桶的长度（60/300/3600/86400...秒）
    asset_id_type       key_base;     // 交易对基础资产
    asset_id_type       key_quote;    // 交易对计价资产

    // OHLCV
    share_type          open_base;    // 开盘价（用 base 表示）
    share_type          open_quote;
    share_type          high_base;
    share_type          high_quote;
    share_type          low_base;
    share_type          low_quote;
    share_type          close_base;
    share_type          close_quote;
    share_type          base_volume;  // 成交量
    share_type          quote_volume;
};
```

配置支持的时间粒度（`config.ini`）：

```ini
market-history-bucket-size = [60,300,900,1800,3600,14400,86400]
market-history-buckets-per-size = 200
```

---

## elasticsearch 插件

将链上操作历史推送到 Elasticsearch，适合需要复杂查询、全文搜索的场景。

```
应用区块
  → account_history_plugin（内存中的操作历史对象）
      → elasticsearch_plugin（批量 POST 到 ES）
```

配置：

```ini
elasticsearch-node-url = http://localhost:9200/
elasticsearch-bulk-replay = 10000  # 批量推送的操作数
elasticsearch-index-prefix = graphene
```

---

## witness 插件：出块逻辑

这是见证人节点最关键的插件。

```cpp
// 简化的出块流程
void witness_plugin::block_production_loop()
{
    while (true) {
        // 计算下一个我负责出块的 slot
        auto slot = next_production_slot();

        // 等到那个时刻
        fc::usleep_until(slot.time);

        // 打包交易，生成区块
        signed_block b = database.generate_block(
            slot.time,
            slot.witness_id,
            _private_keys[slot.witness_id],
            skip_flags
        );

        // 广播给网络
        p2p_node.broadcast(block_message{b});
    }
}
```

私钥配置（`config.ini`）：

```ini
witness-id = "1.6.1"
private-key = ["BTS公钥", "私钥WIF格式"]
```

---

## 创建新插件

项目提供了脚手架脚本：

```bash
cd libraries/plugins
bash make_new_plugin.sh my_plugin
```

生成：

```
my_plugin/
├── CMakeLists.txt
├── include/graphene/my_plugin/my_plugin.hpp
└── my_plugin.cpp
```

然后在 `libraries/plugins/CMakeLists.txt` 和 `programs/witness_node/main.cpp` 中注册即可。

---

## 插件的对象空间

为避免与核心链对象 ID 冲突，插件使用独立的空间 ID（`space_id >= 2`）。但注意：不同插件也需要协调各自的 space_id，防止冲突：

```cpp
// account_history 插件对象
class operation_history_object : public abstract_object<...,
    implementation_ids,          // space = 2
    impl_operation_history_object_type>   // type = 某个枚举值
```

具体 type_id 枚举定义在 `libraries/chain/include/graphene/chain/types.hpp`。
