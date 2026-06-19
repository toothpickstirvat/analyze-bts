# C++ 技法：项目中的高级用法

## 1. fc::fwd — 编译期 pimpl（无堆分配）

标准的 pimpl 模式用 `unique_ptr<impl>` 持有实现，需要堆分配。fc 的 `fwd<T, N>` 在对象内部预留 N 字节的**栈上存储**
，避免堆分配，同时保持编译隔离。

```cpp
// libraries/fc/include/fc/fwd.hpp
template<typename T, unsigned int S>
class fwd {
    // 内部有 char storage[S]，对齐为 double
    // T 实际上构造在这块空间上
    // 析构时调用 ((T*)this)->~T()
};
```

在 `elliptic.hpp` 中的用法：

```cpp
class public_key {
    // ...
private:
    fc::fwd<detail::public_key_impl, 33> my;
    // 含义：在 public_key 对象内部预留 33 字节，存放 public_key_impl 的实现
    // public_key_impl 包含一个 33 字节的公钥数据
};
```

`fc::fwd` 的特殊之处：

- `fwd` 不知道 T 的完整定义（只需要 `sizeof(T) <= S`）
- 析构时通过 `((T*)storage)->~T()` 调用，不需要 T 完整可见
- 拷贝构造需要 T 完整可见（通常在 `.cpp` 实现）
- 相比 `unique_ptr`：无堆分配开销，指针局部性更好；代价是对象大小固定

**使用场景**：隐藏私钥/公钥实现细节（OpenSSL 类型），同时避免堆分配（密码学操作通常频繁创建小对象）。

---

## 2. FC_REFLECT 宏展开详解

```cpp
FC_REFLECT( graphene::protocol::transfer_operation,
            (fee)(from)(to)(amount)(memo)(extensions) )
```

展开后大致等价于：

```cpp
namespace fc {
template<>
struct reflector<graphene::protocol::transfer_operation> {
    typedef graphene::protocol::transfer_operation type;
    typedef fc::true_type  is_defined;
    typedef fc::false_type is_enum;

    // 字段数量（编译期常量）
    enum  member_count_enum { local_member_count = 6 };

    // 遍历所有字段（访问者模式）
    template<typename Visitor>
    static void visit( Visitor& v ) {
        v.template operator()<fc::field_reflection<0, type, decltype(type::fee),    &type::fee>>(0);
        v.template operator()<fc::field_reflection<1, type, decltype(type::from),   &type::from>>(1);
        v.template operator()<fc::field_reflection<2, type, decltype(type::to),     &type::to>>(2);
        // ...
    }
};
// 字段名注册
namespace member_names {
template<> struct member_name<graphene::protocol::transfer_operation, 0> {
    constexpr static const char* value = "fee";
};
// ...
}
} // namespace fc
```

序列化时，`fc::raw::pack` 调用 `fc::reflector<T>::visit(pack_visitor)`，pack_visitor 对每个字段递归调用 `pack`
，无需手写任何序列化代码。

---

## 3. static_variant 的跳转表实现

```cpp
// static_variant 的 visit 实现原理（简化版）
template<typename... Types>
template<typename Visitor>
decltype(auto) static_variant<Types...>::visit(Visitor&& v) const {
    // 编译期生成一个函数指针数组（跳转表）
    static constexpr auto table = make_visitor_table<Types...>();
    // 按运行时 tag 索引跳转
    return table[tag_](std::forward<Visitor>(v), storage_);
}
```

编译器会为每个类型生成一个特化函数，填入数组，运行时通过下标跳转。这比 `if/else if` 链或 `dynamic_cast` 更快，因为：

- 时间复杂度：O(1)（间接跳转），vs O(n)（if 链）
- 无运行时类型信息开销

与 `std::visit` + `std::variant` 语义相同，但 fc 版本额外支持 FC_REFLECT。

---

## 4. 安全整数（fc::safe）

```cpp
// libraries/fc/include/fc/safe.hpp
template<typename T>
class safe {
    T v;
public:
    // 所有算术运算都检查溢出
    friend safe operator+( safe a, safe b ) {
        if( b.v > 0 && a.v > std::numeric_limits<T>::max() - b.v )
            FC_THROW_EXCEPTION( overflow_exception, "integer overflow" );
        if( b.v < 0 && a.v < std::numeric_limits<T>::min() - b.v )
            FC_THROW_EXCEPTION( underflow_exception, "integer underflow" );
        return safe(a.v + b.v);
    }
};

using share_type = fc::safe<int64_t>;
```

`share_type`（代币数量）是 `fc::safe<int64_t>`，防止金额计算溢出。这在金融合约中至关重要：溢出攻击（整数绕回）是 DeFi
历史上最常见的漏洞之一。

---

## 5. flat_map / flat_set（排序向量）

```cpp
#include <fc/container/flat.hpp>

fc::flat_map<account_id_type, weight_type> account_auths;
```

`flat_map` 不是 `std::map`（红黑树），而是**有序 vector 对**，用二分搜索查找：

|       | `std::map` | `fc::flat_map` |
|-------|------------|----------------|
| 存储    | 红黑树（链表节点）  | 有序 vector      |
| 查找    | O(log n)   | O(log n)       |
| 插入    | O(log n)   | O(n)（移动元素）     |
| 缓存友好性 | 差（指针跳转）    | 好（连续内存）        |
| 序列化   | 复杂         | 简单（同 vector）   |

**适用场景**：读多写少、元素数量小（通常 < 50）的集合，如权限列表、手续费表。在密码学对象和协议类型中大量使用。

---

## 6. 零初始化数组（zero_initialized_array）

```cpp
// 公钥数据类型
typedef zero_initialized_array<unsigned char, 33> public_key_data;

// 压缩签名：65 字节，默认零初始化
typedef zero_initialized_array<unsigned char, 65> compact_signature;
```

`zero_initialized_array<T, N>` 是标准 `std::array<T, N>` 的包装，区别在于**默认构造函数将所有字节清零**。

这在密码学中很重要：未初始化的密钥/签名缓冲区是安全漏洞的来源，默认零初始化确保即使代码遗漏了显式赋值，也不会泄露栈上的旧数据。

---

## 7. 扩展机制（extensions_type）

几乎每个操作和协议类型都有 `extensions_type extensions` 字段：

```cpp
typedef fc::static_variant<void_t>  future_extensions;
typedef flat_set<future_extensions>  extensions_type;
```

这允许在不改变操作序号的情况下向协议类型**追加新字段**。硬分叉后的新字段作为 extension 携带，老节点看到未知 extension
可以选择忽略或拒绝。

**C++ 学习点**：这是协议向前兼容性的经典设计——通过 `static_variant` 的类型列表扩展，而不是改变序列化格式。

---

## 8. 编译期计算（constexpr）

```cpp
constexpr int64_t GRAPHENE_MAX_SHARE_SUPPLY (1000000000000000LL);

// 在模板元编程中检查类型约束
static_assert(
    typelist::length<typelist::filter<list, std::is_reference>>() == 0,
    "static_variant cannot hold reference types"
);
```

`static_variant` 内部大量使用 `typelist` 做编译期类型操作（过滤、索引、长度），类似于 `boost::mp11`。

---

## 9. RAII 管理 OpenSSL 资源

OpenSSL 的 C 接口需要手动管理资源（`EC_GROUP_free`、`BN_CTX_free` 等），fc 用 RAII 包装：

```cpp
// 宏生成 RAII 包装类
SSL_TYPE_DECL(ec_group, EC_GROUP)
SSL_TYPE_DECL(ec_point, EC_POINT)
SSL_TYPE_DECL(bn_ctx,   BN_CTX)

// 使用：析构时自动释放
{
    ec_group group(EC_GROUP_new_by_curve_name(NID_secp256k1));
    ec_point point(EC_POINT_new(group));
    bn_ctx   ctx(BN_CTX_new());

    EC_POINT_oct2point(group, point, data, len, ctx);
    // 离开作用域时自动调用 EC_GROUP_free / EC_POINT_free / BN_CTX_free
}
```

`SSL_TYPE_DECL` 展开为一个继承 `ssl_wrapper<T>` 的结构体，禁用拷贝，只允许移动，析构函数调用对应的 `*_free` 函数。这是 C++
封装 C 库的标准模式。

---

## 10. 模板特化加速编译（外部序列化）

大型项目的头文件模板展开会使编译变慢。BitShares 通过**显式模板实例化**把模板展开推到单个 `.cpp` 文件：

```cpp
// 头文件（操作定义旁边）
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( transfer_operation )

// 对应的 .cpp 文件
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( transfer_operation )
// 展开为：
// template void fc::raw::pack<fc::datastream<char*>>(
//     fc::datastream<char*>&, const transfer_operation&, uint32_t);
// template void fc::raw::unpack<...>(...);
```

这样每种操作类型的序列化只在一个翻译单元中实例化，其他文件使用 `extern template`（`DECLARE`
宏的作用）声明该模板已在别处实例化，不再重复展开，大幅减少编译时间。
