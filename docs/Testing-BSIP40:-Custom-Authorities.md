# Preface

This document is intended to assist with the testing of custom authorities (CA) per [BSIP 40 Specifications](https://github.com/bitshares/bsips/blob/master/bsip-0040.md) that is available starting in the [4.0.0 Consensus Release](https://github.com/bitshares/bitshares-core/milestone/17?closed=1).

- [Testing](#testing-overview)
	- [Blockchain Initialization](#initialize-blockchain-for-custom-authorities)
	- [Authorized Transfers](#scenario-authorized-transfers)

Additional custom authorities instructions can be found at [Custom Authority Templates](https://github.com/bitshares/bitshares-core/wiki/Custom-Authority-Templates)


# Testing Overview

These test instructions should be executed from a command line interface wallet (CLI) that has been **built for your test environment**.  For example, testing performed with the public testnet requires the CLI built for the [BitShares Public Testnet](https://testnet.bitshares.eu).  The following instructions were executed on a private testing environment where TEST was the core token.  These exact instructions may differ on your test environment in the following ways:

- The core token may be different than "TEST" (e.g. "BTS").  Modify the commands to use your core token symbol.
- The account names that are created might already exist in your test environment.  Check for their existence by running `get_account <ACCOUNT_NAME>`.  Modify the commands to use alternate account names).
- The asset names that are created might already exist in your test environment.  Check for the existence by running `get_asset <ACCOUNT_NAME>`.  Modify the commands to use alternate asset names).


## Initialize blockchain for Custom Authorities

The instructions in this section are for configuring a blockchain that has not yet been initialized for custom authorites.  These instructions will likely need to be tailored for your particular blockchain.  You may alternatively test these instructions on the BitShares public testnet.


### 1. Nathan: Fund the committee-account

Fund the committee-account so that it can pay for any proposal that is approved by committee members

```
transfer nathan committee-account 2000 TEST "" true
```


### 2. Nathan: Fund the committee-account

Create a dominating committee member (nathan)

```
create_committee_member nathan "" true
vote_for_committee_member nathan nathan true true
```

### 3. Nathan: Propose the CA settings

Propose the following _example_ settings for CA

```
propose_parameter_change "nathan" "2020-01-14T00:00:00" {"extensions": {"custom_authority_options": {"max_custom_authority_lifetime_seconds": 3000000, "max_custom_authorities_per_account": 9, "max_custom_authorities_per_account_op": 4, "max_custom_authority_restrictions": 8}} } true
```

|Warning|
|-|
|This proposed parameter change will also override the other setting in the `extensions` corresponding to HTLC (`updatable_htlc_options`).  To preserve the blockchains current settings for HTLC, the proposed parameter change should also include the blockchain's existing setting.  For example, it could be expressed as:<p>`propose_parameter_change "nathan" "2020-01-14T00:00:00" {"extensions": {"custom_authority_options": {"max_custom_authority_lifetime_seconds": 3000000, "max_custom_authorities_per_account": 9, "max_custom_authorities_per_account_op": 4, "max_custom_authority_restrictions": 8}, "updatable_htlc_options": {"max_timeout_secs": 2419200, "max_preimage_size": 19200}} } true`|

Identify the proposal ID by reviewing the last transaction in the proposing account's history

```
get_account_history nathan 5
```

The proposal ID will have an identifier such as 1.10.x.  The proposal can be reviewed with

```
get_object 1.10.x
```

The proposal can be approved with

```
approve_proposal "nathan" 1.10.x {"active_approvals_to_add":["nathan"]} true
```

The settings will take effect when the proposal expires and will be reflected in the output of `get_global_properties`.


# <div id="scenario-authorized-transfers"/> Scenario: Authorized Transfers

This scenario involves one account (alice) authorizing another account (bob) to another funds from her account to any other account.

## Initialize the Test Environment

The following test scenarios portray the interaction of four actors: an account registrar ("faucet"), and three regular accounts ("alice", "bob", "charlie").  **Each actor will require their own wallet with their own keys to ensure that the keys for the _authorizing_ account are not accidentally signing transactions for the _authorized_ account.  Certain steps must be performed by specific actors from their respective wallet.**  Each step of the instructions describe which actor is performing that step (e.g. "Registrar: ..." indicates that the action should be performed from the wallet of the registrar account).  The reader should use the respective actor's wallet.


## 1. Create and fund accounts

|Tip|
|-|
|The example assumes that a "faucet" account exists and is a lifetime member that can register accounts.  The tester may choose to use another account under their control as a substitute for the "faucet" account.|

### Alice

#### Alice: Create keys

```
unlocked >>> suggest_brain_key 
{
  "brain_priv_key": "...",
  "wif_priv_key": "5Ki...",
  "pub_key": "TEST8fur3fMGPat5ffGgTuMRstPEtE33FJZVJY6ciRr1Jr1EyowdyG"
}

unlocked >>> suggest_brain_key 
{
  "brain_priv_key": "...",
  "wif_priv_key": "5Ja...",
  "pub_key": "TEST6VEKPCcz6E27qcrTP2zAGk4EXJsK5y7RoukBmAJc3pwxLzB4hv"
}
```

#### Faucet: Register Alice

Register the account of alice

```
register_account alice TEST8fur3fMGPat5ffGgTuMRstPEtE33FJZVJY6ciRr1Jr1EyowdyG TEST6VEKPCcz6E27qcrTP2zAGk4EXJsK5y7RoukBmAJc3pwxLzB4hv faucet faucet 60 true
```

Transfer core tokens to alice

```
transfer faucet alice 500 TEST "" true
```

#### Alice: Import active key into Alice's wallet

```
import_key alice 5Ja...
```


### Bob

#### Bob: Create keys

```
unlocked >>> suggest_brain_key 
{
  "brain_priv_key": "...",
  "wif_priv_key": "5JC...",
  "pub_key": "TEST8cNcBs8ra3AWeRFQ6RfYSVsmby5cpRVuXAfg4Hj8XoXVXdD1io"
}

unlocked >>> suggest_brain_key 
{
  "brain_priv_key": "...",
  "wif_priv_key": "5J6...",
  "pub_key": "TEST59nKbp4fb1aYxdxMkuALPkjMV4b4iDuWenivjwTjyE3pM3wE3H"
}
```

#### Faucet: Register Bob

Register the account of bob

```
register_account bob TEST8cNcBs8ra3AWeRFQ6RfYSVsmby5cpRVuXAfg4Hj8XoXVXdD1io TEST59nKbp4fb1aYxdxMkuALPkjMV4b4iDuWenivjwTjyE3pM3wE3H faucet faucet 60 true
```

#### Bob: Import active key into Bob's wallet

```
import_key alice 5J6...
```

### Charlie

#### Charlie: Create keys

```
unlocked >>> suggest_brain_key 
{
  "brain_priv_key": "...",
  "wif_priv_key": "5JY...",
  "pub_key": "TEST6WacA9VfxetmDTYDuzuBNyEQYf4pqWMjCpxtZ47swKH3iPUcPY"
}

unlocked >>> suggest_brain_key 
{
  "brain_priv_key": "...",
  "wif_priv_key": "5KV...",
  "pub_key": "TEST8S3K1dwwUk8N886AHxcewrQEsT1qyHRH5cCsbmpxpYMqfrBNPh"
}
```


#### Faucet: Register Charlie

Register the account of charlie

```
register_account charlie TEST6WacA9VfxetmDTYDuzuBNyEQYf4pqWMjCpxtZ47swKH3iPUcPY TEST8S3K1dwwUk8N886AHxcewrQEsT1qyHRH5cCsbmpxpYMqfrBNPh charlie charlie 60 true
```

#### Charlie: Import active keys into wallet

```
import_key alice 5KV...
```

 	
## 2. Alice authorizes Bob to transfer funds from her account

Alice authorizes Bob to transfer funds from her account to Charlie's account.  Authorizing this transaction will require building a transaction in the CLI Wallet.

The `custom_authority_create_operation` has an identifier of 54.  In this example, Alice has an account identifier of 1.2.19, Bob has an account identifier of 1.2.20, and Charlie has an account identifier of 1.2.21.

|Tip|
|-|
|Each custom authority is limited to a finite timespan that is bounded between `valid_from` and `valid_to`.  Custom authorities are further limited by the blockchain's Committee to a maximum lifetime durations (`max_custom_authority_lifetime_seconds`).  This duration can be querying the blockchain's global properties with `get_global_properties`.  Therefore, when creating an authority for a test, the tester should set the `valid_to` date-time to be within the duration of the current date-time.  For example, on January 1, 2020 a tester learns that that maximum lifetime for a custom authority is 15 days.  Consequently the `valid_to` should be set to before January 16, 2020.|

```
begin_builder_transaction
add_operation_to_builder_transaction 0 [54, {"account":"1.2.19","enabled":true,"valid_from":"1970-01-01T00:00:00","valid_to":"2020-01-31T00:00:00","operation_type":0,"auth":{"weight_threshold":1,"account_auths":[["1.2.20",1]],"key_auths":[],"address_auths":[]},"restrictions":[{"member_index":2,"restriction_type":0,"argument":[7,"1.2.21"]}]}]
set_fees_on_builder_transaction 0 1.3.0
```

Optionally preview the transaction before signing.

```
preview_builder_transaction 0
```

which will produce an output similar to

```json
{
  "ref_block_num": 0,
  "ref_block_prefix": 0,
  "expiration": "1970-01-01T00:00:00",
  "operations": [[
      54,{
        "fee": {
          "amount": 260000,
          "asset_id": "1.3.0"
        },
        "account": "1.2.19",
        "enabled": true,
        "valid_from": "1970-01-01T00:00:00",
        "valid_to": "2030-01-01T00:17:20",
        "operation_type": 0,
        "auth": {
          "weight_threshold": 1,
          "account_auths": [[
              "1.2.20",
              1
            ]
          ],
          "key_auths": [],
          "address_auths": []
        },
        "restrictions": [{
            "member_index": 2,
            "restriction_type": 0,
            "argument": [
              7,
              "1.2.21"
            ],
            "extensions": []
          }
        ],
        "extensions": []
      }
    ]
  ],
  "extensions": []
}
```

Sign and broadcast the transaction.

```
sign_builder_transaction 0 true
```

Identify the custom authority ID by reviewing the last transaction in the authorizing account's history

```
get_account_history alice 5
```

The custom authority ID will have an identifier such as 1.17.x.  The custom authority can be reviewed with

```
get_object 1.17.x
```

For example, 

```json
[{
    "id": "1.17.0",
    "account": "1.2.19",
    "enabled": true,
    "valid_from": "1970-01-01T00:00:00",
    "valid_to": "2020-01-31T00:00:00",
    "operation_type": 0,
    "auth": {
      "weight_threshold": 1,
      "account_auths": [[
          "1.2.20",
          1
        ]
      ],
      "key_auths": [],
      "address_auths": []
    },
    "restrictions": [[
        0,{
          "member_index": 2,
          "restriction_type": 0,
          "argument": [
            7,
            "1.2.21"
          ],
          "extensions": []
        }
      ]
    ],
    "restriction_counter": 1
  }
]
```

## 3. Bob: Transfers funds from Alice to Charlie

Bob uses the CLI Wallet's transacation builder to transfer funds from Alice to Charlie

Bob's active key, as determined by `get_account`, is TEST59nKbp4fb1aYxdxMkuALPkjMV4b4iDuWenivjwTjyE3pM3wE3H

```
begin_builder_transaction
add_operation_to_builder_transaction 0 [0, {"from":"1.2.19","to":"1.2.21","amount": {"amount":"10","asset_id":"1.3.0"} } ]
set_fees_on_builder_transaction 0 1.3.0
sign_builder_transaction2 0 ["TEST59nKbp4fb1aYxdxMkuALPkjMV4b4iDuWenivjwTjyE3pM3wE3H"] true
```

|Note|
|-|
|In order for an authorized party to use CA in the CLI wallet requires using the "builder transaction" and the specific command `sign_builder_transaction2` but not `sign_builder_transaction`.  `sign_builder_transaction2` requires an extra parameter where optional public keys may be specified for signing a transaction.  This should be used by the _authorized_ account (e.g. Bob) to include their authorized public key _which should have been previously imported into the wallet_ for the signing to be successful.|

The successful transfer can be confirmed by checking Charlie's account history

```
get_account charlie 5
```

and by inspecting Charlie's balances

```
list_account_balances charlie
```