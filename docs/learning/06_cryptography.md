# 密码学：BitShares 中的加密原语

## 1. 椭圆曲线：secp256k1

BitShares（与比特币相同）使用 **secp256k1** 曲线，定义为：

```
y² = x³ + 7  (mod p)
p = 2²⁵⁶ - 2³² - 2⁹ - 2⁸ - 2⁷ - 2⁶ - 2⁴ - 1
```

这是一条在有限域 𝔽ₚ 上的椭圆曲线，基点 G 的阶（order）为 n（256 位素数）。

### 密钥对

```
私钥：secret ∈ [1, n-1]  （256 位随机数，即 fc::sha256）
公钥：pubkey = secret × G  （椭圆曲线上的点，压缩格式 33 字节）
```

在代码中：

```cpp
// libraries/fc/include/fc/crypto/elliptic.hpp

typedef fc::sha256          private_key_secret;          // 32 字节
typedef zero_initialized_array<unsigned char, 33>  public_key_data;  // 压缩公钥

// 生成私钥
private_key key = private_key::generate();
fc::sha256  secret = key.get_secret();

// 从已知 secret 还原
private_key key2 = private_key::regenerate( secret );

// 获取对应公钥
public_key  pub = key.get_public_key();
```

### ECDSA 签名

```cpp
// 签名
fc::sha256     digest = fc::sha256::hash( data );
compact_signature sig = private_key.sign_compact( digest );  // 65 字节

// 从签名恢复公钥（不需要预先知道公钥！）
public_key recovered = public_key( sig, digest );

// 验证：比较恢复的公钥与预期公钥
```

**签名格式（compact_signature，65 字节）：**

```
byte[0]:  recovery_id + 27 + 4  （用于从签名恢复公钥）
byte[1-32]:  r  （签名的 r 分量）
byte[33-64]: s  （签名的 s 分量）
```

**从签名恢复公钥**（Bitcoin/BitShares 的核心技巧）：给定消息哈希和签名 (r, s)，可以数学上推算出两个可能的公钥，recovery_id
指明用哪个。好处是交易中不需要包含公钥，节省空间。

### 规范签名（Canonical Signature）

为防止签名延展性攻击（signature malleability），BitShares 要求签名是"规范的"：

```cpp
// 要求 s < n/2（低 s 值）
bool public_key::is_canonical( const compact_signature& c ) {
    return !(c[1] & 0x80)
        && !(c[1] == 0 && !(c[2] & 0x80))
        && !(c[33] & 0x80)
        && !(c[33] == 0 && !(c[34] & 0x80));
}
```

如果签名不规范，`sign_compact` 会用不同的 nonce 重签，直到得到规范签名。

---

## 2. secp256k1-zkp：扩展签名库

项目使用的是 **secp256k1-zkp**（`libraries/fc/vendor/secp256k1-zkp`），是比特币 libsecp256k1 的扩展版本，额外支持：

- **Pedersen Commitment**：`secp256k1_pedersen_commit(ctx, commit, blind, value)`
- **Bulletproofs / Range Proof**：`secp256k1_rangeproof_sign/verify`
- **Schnorr 签名**（实验性）

这些扩展用于 BitShares 的**机密转账（Confidential Transfers / Stealth）**功能。

### Pedersen Commitment 原理

```
C = blind × G + value × H

其中：
  G = secp256k1 基点
  H = 另一个生成点（与 G 无已知离散对数关系）
  blind = 随机盲因子
  value = 实际金额
```

验证者看到 C，能验证 C 是合法承诺（range proof 证明 value ∈ [0, 2⁶⁴]），但无法从 C 推算出 value（因为 blind 的加入使承诺信息论安全）。

### 范围证明（Range Proof）

证明 committed value 在 `[0, 2⁶⁴)` 范围内，而不泄露具体金额。使用 Borromean Ring Signature 实现，可以选择只证明部分位（精度
vs 证明大小的权衡）。

---

## 3. 哈希函数

项目使用多种哈希函数，均通过 OpenSSL EVP 接口调用：

| 函数         | 输出    | 用途                              |
|------------|-------|---------------------------------|
| SHA-256    | 32 字节 | 私钥、chain ID、交易 ID、内部散列          |
| SHA-512    | 64 字节 | HMAC-SHA512（BIP32 HD 密钥派生）      |
| SHA-1      | 20 字节 | 已较少使用                           |
| SHA-224    | 28 字节 | 个别场景                            |
| RIPEMD-160 | 20 字节 | 区块 ID、交易 ID                     |
| Hash160    | 20 字节 | RIPEMD160(SHA256(data))，比特币地址风格 |

### 使用示例

```cpp
// 单次哈希
fc::sha256 h = fc::sha256::hash( data.data(), data.size() );
fc::sha256 h2 = fc::sha256::hash( "hello world" );  // 字符串重载

// 流式（大数据分块）
fc::sha256::encoder enc;
enc.write( buf1, len1 );
enc.write( buf2, len2 );
fc::sha256 result = enc.result();
```

### EVP 接口的作用（OpenSSL 3.0）

旧代码直接调用 `SHA256_Init/Update/Final`，OpenSSL 3.0 将这些标记为废弃。迁移到 EVP 接口后：

```cpp
// 内部实现（libraries/fc/src/crypto/sha256.cpp）
struct sha256::encoder::impl {
    EVP_MD_CTX* ctx;

    impl() : ctx(EVP_MD_CTX_new()) {
        EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    }
    ~impl() { EVP_MD_CTX_free(ctx); }

    // 拷贝构造（fc::fwd<impl,N> 需要）
    impl(const impl& o) : ctx(EVP_MD_CTX_new()) {
        EVP_MD_CTX_copy_ex(ctx, o.ctx);
    }
};

void sha256::encoder::write( const char* d, uint32_t dlen ) {
    EVP_DigestUpdate( my->ctx, d, dlen );
}

sha256 sha256::encoder::result() {
    sha256 h;
    unsigned int dlen = sizeof(h._hash);
    EVP_DigestFinal_ex( my->ctx, (unsigned char*)h._hash, &dlen );
    reset();
    return h;
}
```

EVP（Envelope API）是 OpenSSL 的高层统一接口，通过算法对象（`EVP_MD`）抽象了底层实现，便于：

- 算法热插拔（如替换为硬件加速）
- 未来切换算法不改调用代码
- FIPS 合规（EVP 自动路由到 FIPS 算法提供者）

---

## 4. AES-256 对称加密

用于加密备忘（Memo）内容，以及钱包文件加密。

```cpp
// libraries/fc/include/fc/crypto/aes.hpp
std::vector<char> aes_encrypt( const fc::sha256& key,
                                const std::vector<char>& plain_text );
std::vector<char> aes_decrypt( const fc::sha256& key,
                                const std::vector<char>& cipher_text );
```

内部使用 **AES-256-CBC**，密钥直接来自 `fc::sha256`（32 字节）。

### Memo 加密流程

```cpp
// 发送者：
fc::sha512 shared_secret = sender_private.get_shared_secret( recipient_public );
// shared_secret = ECDH(sender_priv, recipient_pub) = sender_priv × recipient_pub

fc::sha256 key = fc::sha256::hash( shared_secret );
auto cipher = aes_encrypt( key, message );

// 接收者：
fc::sha512 shared_secret2 = recipient_private.get_shared_secret( sender_public );
// shared_secret2 = ECDH(recipient_priv, sender_pub) = recipient_priv × sender_pub
// 注：sender_priv × recipient_pub = recipient_priv × sender_pub（椭圆曲线 DH 原理）
```

**ECDH（椭圆曲线 Diffie-Hellman）**：双方可以独立计算出相同的共享秘密，而不需要在网络上传输该秘密。这是端对端加密的数学基础。

---

## 5. 密钥派生（HD 钱包 / BIP32 风格）

BitShares 支持层级确定性密钥派生：

```cpp
// 子密钥 = f(父密钥, 派生路径)
private_key child = parent_private.child( offset );

// 扩展密钥（支持公钥派生）
extended_private_key master = extended_private_key::generate_master("seed phrase");
extended_public_key  pub    = master.get_extended_public_key();

// 从公钥派生子公钥（不需要私钥！）
extended_public_key child_pub = pub.derive_normal_child( 0 );
```

派生算法（类似 BIP32）：

```
HMAC-SHA512(parent_chain_code, parent_pubkey || index)
  → left_256_bits  ← 子密钥偏移量
  → right_256_bits ← 子链码（chain code）

child_secret = (parent_secret + left_256_bits) mod curve_order
child_pubkey = child_secret × G = parent_pubkey + left_256_bits × G
```

正因为最后一步可以用公钥计算（加法在椭圆曲线上的同态性），所以观察者钱包可以从父公钥派生所有子公钥，无需知道私钥。

---

## 6. Base58 编码

公钥和地址使用 Base58Check 编码（同比特币），去掉了容易混淆的字符（0、O、I、l）：

```cpp
// 公钥的 Base58 表示
std::string key_str = public_key.to_base58();
// 输出类似：BTS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV

// BitShares 公钥格式：前缀 "BTS" + Base58(compressed_pubkey + checksum_4bytes)
```

---

## 7. 签名验证的执行路径

一笔交易被广播时，节点如何验证签名？

```
signed_transaction tx = ...

// 1. 收集交易中所有操作需要的权限
set<public_key_type> required_keys = get_required_signatures(tx, available_keys);

// 2. 对每个签名，恢复公钥
for (const auto& sig : tx.signatures) {
    fc::sha256 digest = tx.sig_digest(chain_id);
    public_key pk = public_key(sig, digest);
    recovered_keys.insert(pk);
}

// 3. 验证恢复出的公钥集合满足所有权限的阈值要求
verify_authority(tx.operations, recovered_keys, ...);
```

关键路径：`libraries/chain/transaction_evaluation_state.cpp`

---

## 8. 密码学安全性小结

| 组件                  | 安全级别                         | 备注                |
|---------------------|------------------------------|-------------------|
| secp256k1 ECDSA     | ~128 位                       | 被比特币、以太坊验证，实践可信   |
| AES-256-CBC         | 256 位                        | NIST 标准，无已知攻击     |
| SHA-256             | 128 位碰撞抗性                    | 符合生日悖论上界          |
| RIPEMD-160          | 80 位碰撞抗性                     | 较弱，但仅用于 ID（非安全关键） |
| Pedersen Commitment | 信息论安全（hiding）/ 计算安全（binding） | 依赖 ECDLP 困难性      |
