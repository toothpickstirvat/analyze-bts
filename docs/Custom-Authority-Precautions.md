# Restricting the Power of Custom Authorities

Custom Authorities, which were introduced with [BSIP-40](https://github.com/bitshares/bsips/blob/master/bsip-0040.md), allow any account to authorize another key, account, or authority to submit an operation on behalf of the account.  This powerful feature carries some risk because some operations are so powerful or flexible that it may be undesirable to delegate its power to another entity.  For example, Alice may not wish to authorize Bob to transfer _everything_ from her account; instead Alice may desire to authorize transfers _if and only if_ the asset is Asset A _and_ it is being sent to Charlie.

Restrictions are available with Custom Authorities that place restrictions on the use of an operation that are conditioned on various members of that operation.  For example, Alice's desired authorization can be achieved by authorizing Bob to submit a `transfer_operation` on her behalf if and only if Charlie is specified as the `to` member, and the `amount`'s `asset_id` corresponds to Asset A.  If Bob constructs a transfer operation that satisfies all of the restrictions, **he may also set any of the other unrestricted members**.

Restrictions may be placed on all of the available members to safeguard against this.  It is up to the designer of the custom authority to consider every available member of an operation to safeguard the authorizing account.

There are, however, certain challenges or weaknesses with restricting the custom authorities.


# Precautions

## Restricting Extensions in Operations

Some operations have an `extension` type member which is intended to carry supplemental information.  When an authorizing party wishes to restrict the extension member from being used by an authorized party, it is not technically possible as of [BitShares 4.0](https://github.com/bitshares/bitshares-core/milestone/17?closed=1), to enforce the `extension` type member be set to `void_t` as is possible with other `optional<>` members.  Instead _each member of the `extension` type_ must be have restrictions placed on them such that the extension member is effectively prohibited from being set.
 
## Restricting Optional Members in Operations

Every optional member in an operation which should not be set by an authorized party needs to have a restriction placed on it such that the member is set equal to `void_t` .

## Restricting Optional Members in Operations

Every mandatory member in an operation which should be restricted, in the opinion of the authorizing party, should have an appropriate restriction placed on it.

## New Members in Old Operations

If future releases add a new member to an old operation, existing custom authorities for that operation will be unaware of them and will not be able to restrict authorized parties from specifying that value.  _Therefore such an addition should be cautiously implemented by future BitShares developers_.

## New Operations

New operations will be better suited for use with custom operations if they have a narrow focus.  For example, any account that wishes to update its voting slate may only do so, as of [BitShares 4.0](https://github.com/bitshares/bitshares-core/milestone/17?closed=1), by using an [authorized voting key](https://github.com/bitshares/bitshares-core/wiki/Custom-Authority-Templates#template-authorized-voting-by-a-key) which makes use of the `account_update_operation`.  That custom authority must be tightly restricted because that operation is both broad and flexible.  In contrast, the proposed `account_update_votes_operation` from [BSIP-47](https://github.com/bitshares/bsips/blob/master/bsip-0047.md) is a narrowly defined and narrowly interpreted operation which is ideally suited for custom authorities.


# Sample Restrictions

Further examples of restrictions on custom authorities may be found in [Custom Authority Templates](https://github.com/bitshares/bitshares-core/wiki/Custom-Authority-Templates) and in the [custom authority unit tests](https://github.com/bitshares/bitshares-core/blob/release/tests/tests/custom_authority_tests.cpp).

When designing a new custom authority for a particular use case it is recommended to create an additional unit test for it.  This will test the new custom authority, and one can optionally generate the JSON template for it, with the use of `wdump((<NEW_CUSTOM_AUTHORIZATION_OPERATION>));`, which may subsequently be signed and broadcast with the CLI Wallet as is demonstrated in the [Custom Authority Templates](https://github.com/bitshares/bitshares-core/wiki/Custom-Authority-Templates).