# fc 库：BitShares 的基础设施

`libraries/fc` 是整个项目的根基，是一个独立的 C++ 工具库（子模块），提供其他所有库依赖的基本能力。理解 fc 等于理解整个项目
80% 的 C++ 技法。

---

## 1. 异步与协程（fc::thread / fc::future）

fc 实现了**协作式多任务**，不依赖 std::thread 或 std::async，而是基于 Boost.Context（用户态协程，stackful fiber）。

### 核心 API

```cpp
// 异步提交任务到当前线程的任务队列
fc::future<T> f = fc::async( []() { return compute(); }, "task-name" );

// 等待结果（会让出控制权，调度其他任务）
T result = f.wait();

// 延迟调度
fc::schedule( []() { do_something(); }, fc::time_point::now() + fc::seconds(5), "name" );
```

### 与 std::thread 的区别

| 特性   | fc::async               | std::thread      |
|------|-------------------------|------------------|
| 并发方式 | 协程（用户态切换）               | 系统线程（内核调度）       |
| 内存开销 | 小（共享栈）                  | 大（每线程独立栈，默认 8MB） |
| 同步原语 | fc::mutex / fc::promise | std::mutex       |
| 适用场景 | I/O 密集（网络等待）            | CPU 密集           |

项目代码中，`fc::async` 大量出现在网络层，用于处理并发连接而不创建大量系统线程。

### 位置

- `libraries/fc/include/fc/thread/future.hpp` — future/promise 定义
- `libraries/fc/include/fc/thread/thread.hpp` — 线程/任务调度
- `libraries/fc/src/thread/` — 实现（基于 Boost.Context）

---

## 2. 静态类型变体（fc::static_variant）

`fc::static_variant<T1, T2, T3, ...>` 是编译期已知类型集合的**可辨别联合**（discriminated union），类似 `std::variant`
（C++17），但 fc 的版本早于标准，且添加了反射支持。

整个操作系统就建立在它上面：

```cpp
// libraries/protocol/include/graphene/protocol/operations.hpp
using operation = fc::static_variant<
    transfer_operation,          // 0
    limit_order_create_operation,// 1
    // ... 共 77 个类型
>;
```

使用方式：

```cpp
operation op = transfer_operation{ ... };

// 访问具体类型
op.visit( [](const auto& o) {
    // o 的类型在编译期已知
    std::cout << fc::get_typename<decltype(o)>::name() << "\n";
});

// 类型判断
if( op.which() == operation::tag<transfer_operation>::value ) {
    auto& t = op.get<transfer_operation>();
}
```

`visit` 是零开销抽象：编译器生成跳转表，比 `dynamic_cast` 快。

### 位置

- `libraries/fc/include/fc/static_variant.hpp`

---

## 3. 静态反射（FC_REFLECT）

fc 提供了编译期静态反射，不依赖 RTTI，通过宏 `FC_REFLECT` 在类型旁边注册字段信息。

```cpp
// 定义结构体
struct transfer_operation {
    account_id_type from;
    account_id_type to;
    asset           amount;
    fc::optional<memo_data> memo;
};

// 注册反射
FC_REFLECT( graphene::protocol::transfer_operation,
            (from)(to)(amount)(memo) )
```

这个宏展开后生成 `fc::reflector<transfer_operation>` 的特化，包含：

- 字段数量（编译期常量）
- 字段名字符串
- 字段类型
- 成员指针（`&T::field`）

反射的用途：

- **序列化**：`fc::raw::pack/unpack` 通过反射自动处理所有字段
- **JSON**：`fc::to_variant / from_variant` 自动转换为 JSON 对象
- **RPC**：函数参数/返回值自动序列化
- **调试**：可以遍历所有字段打印对象

不需要手写序列化代码，只要加 `FC_REFLECT` 就能通过网络传输、存储、打印。

### 位置

- `libraries/fc/include/fc/reflect/reflect.hpp` — 宏定义
- `libraries/fc/include/fc/reflect/typename.hpp` — 类型名查询
- `libraries/fc/include/fc/reflect/typelist.hpp` — 编译期类型列表

---

## 4. 二进制序列化（fc::raw）

BitShares 的网络传输和磁盘存储都使用 fc 的二进制序列化，**小端序（Little-Endian）**，变长整数用 unsigned LEB128（类似
protobuf varint）。

```cpp
// 序列化到字节流
std::vector<char> data;
fc::datastream<char*> ds( data.data(), data.size() );
fc::raw::pack( ds, my_object );

// 反序列化
fc::raw::unpack( ds, my_object );

// 便捷包装
auto bytes = fc::raw::pack( my_operation );
auto obj   = fc::raw::unpack<MyType>( bytes );
```

对于需要加速编译（避免头文件中实例化模板）的类型：

```cpp
// .hpp 中
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( my_type )

// .cpp 中
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( my_type )
```

### 位置

- `libraries/fc/include/fc/io/raw.hpp` — 模板实现
- `libraries/fc/include/fc/io/datastream.hpp` — 字节流包装

---

## 5. JSON / variant（fc::variant）

fc::variant 是一个动态类型容器，可以持有 null / bool / int64 / uint64 / double / string / array / object，是连接 C++ 类型系统和
JSON 的桥梁。

```cpp
// C++ 对象 → JSON
fc::variant v;
fc::to_variant( my_object, v );
std::string json = fc::json::to_string( v );

// JSON → C++ 对象
fc::variant v2 = fc::json::from_string( json_str );
MyType obj;
fc::from_variant( v2, obj );
```

`to_variant / from_variant` 的实现依赖 FC_REFLECT，对反射过的类型自动生成转换代码。

---

## 6. 异常系统（FC_THROW_EXCEPTION）

fc 有自己的异常体系，所有异常继承 `fc::exception`，携带结构化的调用栈和上下文信息。

```cpp
FC_THROW_EXCEPTION( fc::assert_exception, "余额不足: ${amount}", ("amount", amount) );

// 断言宏（失败时抛 assert_exception）
FC_ASSERT( balance >= amount, "余额 ${b} 不足支付 ${a}", ("b", balance)("a", amount) );

// 捕获
try {
    // ...
} catch( const fc::exception& e ) {
    elog( "${e}", ("e", e.to_detail_string()) );
}
```

异常携带的信息：

- 错误码（int64_t）
- 错误名（字符串）
- 调用栈（文件名 + 行号 + 函数名）
- 格式化的上下文变量（键值对）

---

## 7. 日志（fc::logger）

```cpp
// 不同级别
dlog( "调试信息: ${x}", ("x", val) );
ilog( "普通信息" );
wlog( "警告" );
elog( "错误" );

// 带格式的变量替换（编译期解析，不用 printf）
ilog( "区块 #${n} 已应用，见证人: ${w}", ("n", block_num)("w", witness_name) );
```

日志支持运行时配置（级别过滤、输出到文件/控制台/syslog），通过 `config.ini` 中的 `log-appender` 配置。

---

## 8. 文件系统路径（fc::path）

```cpp
fc::path data_dir = fc::path("/tmp") / "graphene";
fc::create_directories( data_dir );
bool exists = fc::exists( data_dir );
```

封装了 `boost::filesystem::path`，在旧版 API 上层提供统一接口。

---

## 小结：fc 是"标准库的替代品"

| 标准/Boost               | fc 等价物                     | 优势                     |
|------------------------|----------------------------|------------------------|
| `std::variant` (C++17) | `fc::static_variant`       | 带反射，早于 C++17           |
| RTTI / `typeid`        | `FC_REFLECT`               | 编译期，零运行时开销             |
| `std::future`          | `fc::future`               | 协程友好，可在 fiber 中 wait   |
| `nlohmann::json`       | `fc::variant` + FC_REFLECT | 自动序列化 C++ 类型           |
| `protobuf`             | `fc::raw` + FC_REFLECT     | 无 .proto 文件，直接用 C++ 类型 |
| `std::exception`       | `fc::exception`            | 结构化上下文，调用栈             |
