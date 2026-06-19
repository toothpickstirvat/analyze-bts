# Preface

This document is intended to assist with the use of custom authorities (CA) per [BSIP 40 Specifications](https://github.com/bitshares/bsips/blob/master/bsip-0040.md) in the [4.0.0 Consensus Release](https://github.com/bitshares/bitshares-core/milestone/17?closed=1).

- [Templates for Creating a Custom Authority](#custom-authority-json-templates)
	- [Authorized Restricted Transfers](#template-authorized-restricted-transfers)
	- [Authorized Unrestricted Trading](#template-authorized-unrestricted-trading)
	- [Authorized Restricted Trading](#template-authorized-restricted-trading)
	- [Authorized Feed Publishing by an Account](#template-authorized-feed-publishing-by-an-account)
	- [Authorized Feed Publishing by a Key](#template-authorized-feed-publishing-by-a-key)
	- [Authorized Account Registration](#template-authorized-account-registration)
	- [Authorized Voting by a Key](#template-authorized-voting-by-a-key)
	- [Authorized Changing of a Witness Signing Key](#template-authorized-changing-of-a-witness-signing-key-by-a-key)
- [Updating a Custom Authority](#updating-a-custom-authority)
	- [Updating the Authorization Period](#updating-the-authorization-period)
	- [Disabling a Custom Authority](#disabling-a-custom-authority)
	- [Enabling a Custom Authority](#enabling-a-custom-authority)
	- [Deleting a Custom Authority](#deleting-a-custom-authority)
	- [Changing the Authorized Account](#changing-the-authorized-account)
	- [Adding Restrictions to a Custom Authority](#adding-restrictions-to-a-custom-authority)
	- [Removing a Restriction from a Custom Authority](#removing-a-restriction-from-a-custom-authority)

# Custom Authority JSON Templates

_One_ way for an account to _create_ a custom authority is with the CLI Wallet's `add_operation_to_builder_transaction` command.  This command is used as one step of the of "builder transaction" sequence.

```
begin_builder_transaction
add_operation_to_builder_transaction <builder_handle> [54, <JSON_template>]
set_fees_on_builder_transaction <builder_handle> 1.3.0
preview_builder_transaction <builder_handle>
sign_builder_transaction <builder_handle> true
```

where the `<builder_handle>` is the integer "handle" output (e.g. 0, 1, 2, etc.) from the first command in the sequence `begin_builder_transaction`.  <JSON_template> is the JSON encoding of the `custom_authority_create_operation` (Operation 54) that can be broadcast to the network.

Different authorizations require different templates.  This section contains the JSON-encoded templates for various authorizations.  Each of the templates have validity period from `valid_from` through `valid_to` that should be tailored for your use case **and** which must be compatible with the _existing_ limitations on custom authorites (`custom_authority_options`) that may be queried by invoking the `get_global_properties` command in the CLI Wallet or on an RPC-API node.

Every custom authority has a unique identifier.  **This identifier can most easily be tracked immediately after the authorization is created by inspecting the authorizing account's history.**

```
get_account_history <account_name> 1
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

The properties of these custom authority [can be updated after creation](#updating-a-custom-authority).


## Template: Authorized Restricted Transfers

Alice (1.2.19) authorizes Bob (1.2.20) to transfer any amount of any asset from her account to Charlie's account (1.2.21).  `transfer_operation` is Operation 0.

```json
{"account":"1.2.19","enabled":true,"valid_from":"1970-01-01T00:00:00","valid_to":"2020-01-31T00:00:00","operation_type":0,"auth":{"weight_threshold":1,"account_auths":[["1.2.20",1]],"key_auths":[],"address_auths":[]},"restrictions":[{"member_index":2,"restriction_type":0,"argument":[7,"1.2.21"]}]}
```

## Template: Authorized Unrestricted Trading

Alice (1.2.17) authorizes Bob (1.2.18) to _create_ limit orders for her account without any restrictions.  `limit_order_create_operation` is Operation 1.

```json
{"account":"1.2.17","enabled":true,"valid_from":"1970-01-01T00:00:00","valid_to":"2030-01-01T00:17:25","operation_type":1,"auth":{"weight_threshold":1,"account_auths":[["1.2.18",1]],"key_auths":[],"address_auths":[]},"restrictions":[]}
```

Alice (1.2.17) authorizes Bob (1.2.18) to _cancel_ limit orders for her account without any restrictions.  `limit_order_cancel_operation` is Operation 2.

```json
{"account":"1.2.17","enabled":true,"valid_from":"1970-01-01T00:00:00","valid_to":"2030-01-01T00:17:25","operation_type":2,"auth":{"weight_threshold":1,"account_auths":[["1.2.18",1]],"key_auths":[],"address_auths":[]},"restrictions":[]}
```

## Template: Authorized Restricted Trading

Alice (1.2.17) authorizes a public key (BTS74YKubbAGUpihj1BP9cCNfdtUbiAhathRs92Ai5EvEQegbpTm8) to _create_ limit orders for her account but is restricted to trading ACOIN1 (1.3.2) against BCOIN1 (1.3.4), BCOIN2 (1.3.5), or BCOIN3 (1.3.6).

These operations are combined into a single custom authority by using a "logical or" branch.  The first branch authorizes the selling of ACOIN1 for ACOIN1 for BCOIN1, BCOIN2, or BCOIN3.  The second branch authorizes selling of BCOIN1, BCOIN2, or BCOIN3 for ACOIN1.  `limit_order_create_operation` is Operation 1.

```json
{"account":"1.2.17","enabled":true,"valid_from":"1970-01-01T00:00:00","valid_to":"2030-01-01T00:17:25","operation_type":1,"auth":{"weight_threshold":1,"account_auths":[],"key_auths":[["BTS74YKubbAGUpihj1BP9cCNfdtUbiAhathRs92Ai5EvEQegbpTm8",1]],"address_auths":[]},"restrictions":[{"member_index":999,"restriction_type":11,"argument":[40,[[{"member_index":2,"restriction_type":10,"argument":[39,[{"member_index":1,"restriction_type":6,"argument":[27,["1.3.2"]]}]]},{"member_index":3,"restriction_type":10,"argument":[39,[{"member_index":1,"restriction_type":6,"argument":[27,["1.3.3","1.3.4","1.3.5"]]}]]}],[{"member_index":2,"restriction_type":10,"argument":[39,[{"member_index":1,"restriction_type":6,"argument":[27,["1.3.3","1.3.4","1.3.5"]]}]]},{"member_index":3,"restriction_type":10,"argument":[39,[{"member_index":1,"restriction_type":6,"argument":[27,["1.3.2"]]}]]}]]]}]}
```

_It is not possible to restrict the cancellation of orders as a function of the the assets that are involved.  Therefore the authorization to cancel limit orders will be unrestricted.  `limit_order_cancel_operation` is Operation 2.

```json
{"account":"1.2.17","enabled":true,"valid_from":"1970-01-01T00:00:00","valid_to":"2030-01-01T00:17:25","operation_type":2,"auth":{"weight_threshold":1,"account_auths":[],"key_auths":[["BTS74YKubbAGUpihj1BP9cCNfdtUbiAhathRs92Ai5EvEQegbpTm8",1]],"address_auths":[]},"restrictions":[]}
```

## Template: Authorized Feed Publishing by an Account

A feed publisher (1.2.16) authorizes/delegates Bob (1.2.17) to publish feeds for an asset.  `asset_publish_feed_operation` is Operation 19.

```json
{"account":"1.2.16","enabled":true,"valid_from":"1970-01-01T00:00:00","valid_to":"2030-01-01T00:17:30","operation_type":19,"auth":{"weight_threshold":1,"account_auths":[["1.2.17",1]],"key_auths":[],"address_auths":[]},"restrictions":[]}
```

## Template: Authorized Feed Publishing by a Key

A feed publisher (1.2.16) authorizes/delegates a public key (BTS74YKubbAGUpihj1BP9cCNfdtUbiAhathRs92Ai5EvEQegbpTm8) to publish feeds for an asset.  `asset_publish_feed_operation` is Operation 19.

```json
{"account":"1.2.16","enabled":true,"valid_from":"1970-01-01T00:00:00","valid_to":"2030-01-01T00:17:25","operation_type":19,"auth":{"weight_threshold":1,"account_auths":[],"key_auths":[["BTS74YKubbAGUpihj1BP9cCNfdtUbiAhathRs92Ai5EvEQegbpTm8",1]],"address_auths":[]},"restrictions":[]}
```

## Template: Authorized Account Registration

A faucet account (1.2.16) authorizes a public key (BTS74YKubbAGUpihj1BP9cCNfdtUbiAhathRs92Ai5EvEQegbpTm8) to register accounts on its behalf.  `account_create_operation` is Operation 5.

```json
{"account":"1.2.16","enabled":true,"valid_from":"1970-01-01T00:00:00","valid_to":"2030-01-01T00:17:20","operation_type":5,"auth":{"weight_threshold":1,"account_auths":[],"key_auths":[["BTS74YKubbAGUpihj1BP9cCNfdtUbiAhathRs92Ai5EvEQegbpTm8",1]],"address_auths":[]},"restrictions":[]}
```

## Template: Authorized Voting by a Key

Alice (1.2.19) authorizes a public key (BTS74YKubbAGUpihj1BP9cCNfdtUbiAhathRs92Ai5EvEQegbpTm8) to update her voting slate.  This requires authorizing the `account_update_operation` with several restrictions that prohibits updating the owner key, the active key, the special owner authority, and the special active authority, and requires the memo key to be specified as Alice's _current_ memo key (BTS7zsqi7QUAjTAdyynd6DVe8uv4K8gCTRHnAoMN9w9CA1xLCTDVv).  `account_update_operation` is Operation 6.

```json
{"account":"1.2.16","enabled":true,"valid_from":"1970-01-01T00:00:00","valid_to":"2030-01-02T00:00:40","operation_type":6,"auth":{"weight_threshold":1,"account_auths":[],"key_auths":[["BTS74YKubbAGUpihj1BP9cCNfdtUbiAhathRs92Ai5EvEQegbpTm8",1]],"address_auths":[]},"restrictions":[{"member_index":2,"restriction_type":0,"argument":[0,{}]},{"member_index":3,"restriction_type":0,"argument":[0,{}]},{"member_index":5,"restriction_type":10,"argument":[39,[{"member_index":1,"restriction_type":0,"argument":[0,{}]}]]},{"member_index":5,"restriction_type":10,"argument":[39,[{"member_index":2,"restriction_type":0,"argument":[0,{}]}]]},{"member_index":4,"restriction_type":10,"argument":[39,[{"member_index":0,"restriction_type":0,"argument":[5,"BTS7zsqi7QUAjTAdyynd6DVe8uv4K8gCTRHnAoMN9w9CA1xLCTDVv"]}]]}]}
```

## Template: Authorized Changing of a Witness Signing Key by a Key

A witness account (1.2.16) authorizes a public key (BTS74YKubbAGUpihj1BP9cCNfdtUbiAhathRs92Ai5EvEQegbpTm8) to update its signing key.  This requires authorizing the `witness_update_operation` with one restriction that prohibits updating the witness URL.  `witness_update_operation` is Operation 21.

```json
{"account":"1.2.16","enabled":true,"valid_from":"1970-01-01T00:00:00","valid_to":"2030-01-02T00:00:50","operation_type":21,"auth":{"weight_threshold":1,"account_auths":[],"key_auths":[["BTS74YKubbAGUpihj1BP9cCNfdtUbiAhathRs92Ai5EvEQegbpTm8",1]],"address_auths":[]},"restrictions":[{"member_index":3,"restriction_type":0,"argument":[0,{}]}]}
```

# Updating a Custom Authority

## Updating the Authorization Period

Grants of custom authorities are limited in duration by the authorizing account.  When a custom authority expires it is removed from an existence an can no longer be updated.  The time period of authorization can be updated while the authorization is still active.

The `custom_authority_update_operation` has an identifier of 55.  In this example, the _original authorizing account_ (1.2.17) will build a transaction to update to change the time period (`valid_from` and `valid_to`) of the original custom authority (1.17.5) to be valid from July 1, 2020 through July 30, 2020.  Naturally this attempted change must be compatible with the _existing_ limitations on custom authorites (`custom_authority_options`) that may be queried by invoking the `get_global_properties` command in the CLI Wallet or on an RPC-API node.

```
begin_builder_transaction
add_operation_to_builder_transaction <builder_handle> [55, {"account":"1.2.17","authority_to_update":"1.17.5","valid_from":"2020-07-01T00:00:00","valid_to":"2020-07-30T00:00:00"}]
set_fees_on_builder_transaction <builder_handle> 1.3.0
preview_builder_transaction <builder_handle>
sign_builder_transaction <builder_handle> true
```

## Disabling a Custom Authority

Grants of custom authorities may be disabled while the custom authority has not yet expired.

The `custom_authority_update_operation` has an identifier of 55.  In this example, the _original authorizing account_ (1.2.17) will build a transaction to disable the original custom authority (1.17.5).

```
begin_builder_transaction
add_operation_to_builder_transaction <builder_handle> [55, {"account":"1.2.17","authority_to_update":"1.17.5","new_enabled":"false"}]
set_fees_on_builder_transaction <builder_handle> 1.3.0
preview_builder_transaction <builder_handle>
sign_builder_transaction <builder_handle> true
```

## Enabling a Custom Authority

Grants of custom authorities may be enabled while the custom authority has not yet expired.

The `custom_authority_update_operation` has an identifier of 55.  In this example, the _original authorizing account_ (1.2.17) will build a transaction to enable the original custom authority (1.17.5).

```
begin_builder_transaction
add_operation_to_builder_transaction <builder_handle> [55, {"account":"1.2.17","authority_to_update":"1.17.5","new_enabled":"true"}]
set_fees_on_builder_transaction <builder_handle> 1.3.0
preview_builder_transaction <builder_handle>
sign_builder_transaction <builder_handle> true
```

## Deleting a Custom Authority

Grants of custom authorities may be permanently deleted while the custom authority has not yet expired.

_The `custom_authority_delete_operation` has an identifier of 56_.  In this example, the _original authorizing account_ (1.2.17) will build a transaction to delete the original custom authority (1.17.5).

```
begin_builder_transaction
add_operation_to_builder_transaction <builder_handle> [56, {"account":"1.2.17","authority_to_delete":"1.17.5"}]
set_fees_on_builder_transaction <builder_handle> 1.3.0
preview_builder_transaction <builder_handle>
sign_builder_transaction <builder_handle> true
```

## Changing the Authorized Account

Grants of custom authorities may be changed while the custom authority has not yet expired.

The `custom_authority_update_operation` has an identifier of 55.  In this example, the _original authorizing account_ (1.2.17) will build a transaction to change the authorization (1.17.5) to another account (1.2.22).

```
begin_builder_transaction
add_operation_to_builder_transaction <builder_handle> [55, {"account":"1.2.17","authority_to_update":"1.17.5","new_auth":{"weight_threshold":1,"account_auths":[["1.2.22",1]],"key_auths":[],"address_auths":[]}}]
set_fees_on_builder_transaction <builder_handle> 1.3.0
preview_builder_transaction <builder_handle>
sign_builder_transaction <builder_handle> true
```

## Adding Restrictions to a Custom Authority

Restrictions to an existing custom authority may be added while the custom authority has not yet expired.  The additional restriction is directly dependent on the custom authority's operation type.

In this example, Alice (1.2.17) had _previously_ authorized Bob to [_create_ limit orders for her account without any restrictions](#template-authorized-unrestricted-trading).  Alice has decided to restrict the trading to only permit selling the core asset (1.3.0) to buy another asset (1.3.2).

The `custom_authority_update_operation`, which has an identifier of 55, will be used to update the original custom authority (1.17.5).

```
begin_builder_transaction
add_operation_to_builder_transaction <builder_handle> [55, {"account":"1.2.17","authority_to_update":"1.17.5","restrictions_to_add":[{"member_index":2,"restriction_type":10,"argument":[39,[{"member_index":1,"restriction_type":0,"argument":[8,"1.3.0"]}]]},{"member_index":3,"restriction_type":10,"argument":[39,[{"member_index":1,"restriction_type":0,"argument":[8,"1.3.2"]}]]}]} ]
set_fees_on_builder_transaction <builder_handle> 1.3.0
preview_builder_transaction <builder_handle>
sign_builder_transaction <builder_handle> true
```


## Removing a Restriction from a Custom Authority

Restrictions to an existing custom authority may be removed while the custom authority has not yet expired.  The restriction is identified by its 0-indexed position among the existing restrictions.

The `custom_authority_update_operation` has an identifier of 55.  In this example, the _original authorizing account_ (1.2.17) will build a transaction to change the authorization (1.17.5) by removing the first restriction (0).

```
begin_builder_transaction
add_operation_to_builder_transaction <builder_handle> [55, {"account":"1.2.17","authority_to_update":"1.17.5","restrictions_to_remove":[0]} ]
set_fees_on_builder_transaction <builder_handle> 1.3.0
preview_builder_transaction <builder_handle>
sign_builder_transaction <builder_handle> true
```