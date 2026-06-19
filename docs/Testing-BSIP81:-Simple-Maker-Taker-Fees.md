# Preface

This document is intended to assist with the testing of simple maker-taker fees per [BSIP 81 Specifications](https://github.com/bitshares/bsips/blob/master/bsip-0081.md).

These test instructions should be executed from a command line interface wallet (CLI) that has been **built for your test environment**.  For example, testing performed with the public testnet requires the CLI built for the [BitShares Public Testnet](https://testnet.bitshares.eu).  The following instructions were executed on a private testing environment where TEST was the core token.  These exact instructions may differ on your test environment in the following ways:

- The core token may be different than "TEST" (e.g. "BTS").  Modify the commands to use your core token symbol.
- The account names that are created might already exist in your test environment.  Check for their existence by running `get_account <ACCOUNT_NAME>`.  Modify the commands to use alternate account names).
- The asset names that are created might already exist in your test environment.  Check for the existence by running `get_asset <ACCOUNT_NAME>`.  Modify the commands to use alternate account names).


# Overview

From [BSIP-81](https://github.com/bitshares/bsips/blob/master/bsip-0081.md):

_"This BSIP proposes a protocol change to enable asset owners to specify different market fee rate for maker orders and taker orders."_

All trades that are filled on the DEX are charged a fee called the "market fee".  For an asset _without_ any [market fee reward](https://github.com/bitshares/bitshares-core/wiki/Testing-HF-1268:-Market-Fee-Sharing), the market fee equals the amount of an asset that is being received by an account (`filled_trade`) multiplied by the market fee % (`market_fee_percent`), which is set by the asset owner; this amount is capped by the maximum market fee which is also set by the asset owner.

The formula for calculating the fee (`market_fee`) that is distributed to the asset owner is

market_fee = filled_trade &times; market_fee_percent

This BSIP introduces a market fee % that is different if the filled order is a maker order or a taker order.


**<div id="example-1" /> Example 1 </div>**

100 units of an asset, called XRAY, is traded and filled on the DEX.  The asset's market fee percent is 2% for a maker order, and is 4% for a taker order.  The resulting market fee for a maker order is

market_fee = 100 XRAY &times; 2% = 2 XRAY

The resulting market fee for a taker order is

market_fee = 100 XRAY &times; 5% = 5 XRAY


# Test Scenario: Simple Maker Taker Fees for Two User-Issued Assets

This test scenario will demonstrate the charging of different market fees based on whether the filled order is a maker or a taker order.

## <div id="initialize_accounts" /> Initialize Accounts

The following test scenario portrays the interaction of four actors: a faucet account ("faucet"), an asset owner ("asset-owner"), and two trading accounts ("alice" and "bob").  Each will require their own wallet with their own keys (see [Tips for Creating New Keys](https://github.com/bitshares/bitshares-core/wiki/CLI-Wallet-Cookbook#creating-new-keys)).  Certain steps must be performed by specific actors from their respective wallet.  Each step of the instructions describe which actor is performing that step (e.g. "Alice: ..." indicates that the action should be performed from the wallet of the "alice" account).  The reader should use the respective actor's wallet.

|Tip|
|-|
|Testers who prefer convenience over strict separation of keys may prefer to keep the private keys of every actor in a single wallet.|


### Faucet: Create the actors

Create the accounts of the asset owner, alice, and bob from the **faucet** account (or any other LTM account)

```
register_account asset-owner TEST... TEST... faucet faucet 0 true
register_account alice TEST... TEST... faucet faucet 0 true
register_account bob TEST... TEST... faucet faucet 0 true
```


### Faucet: Fund the accounts with core token

```
transfer faucet asset-owner 1000000 TEST "" true
transfer faucet alice 5000 TEST "" true
transfer faucet bob 5000 TEST "" true
```


## Create and Issue the XRAY UIA

### Asset Owner: Create the Asset Type

The asset, called XRAY, will be created to be consistent with [Example 1](#example-1) with a 2% **maker** market fee (expressed as 200 in terms of hundredths of a percent) and a 5% **taker** market fee (expressed as 400 in terms of the hundredths of a percent) that is paid to the asset owner.

```
create_asset asset-owner XRAY 2 {"max_supply":"1000000000000000","market_fee_percent":200,"max_market_fee":"1000000000000000","issuer_permissions":1,"flags":1,"core_exchange_rate":{"base":{"amount":1,"asset_id":"1.3.0"},"quote":{"amount":1,"asset_id":"1.3.1"}},"whitelist_authorities":[],"blacklist_authorities":[],"whitelist_markets":[],"blacklist_markets":[],"description":"","extensions":{"taker_fee_percent":500}} null true
```

### Asset Owner: Issue the Asset

The asset will be issued to one of the trading accounts, "alice".

```
issue_asset alice 5000 XRAY "" true
```


## Create and Issue the ZULU UIA

### Asset Owner: Create the Asset Type

The asset, called ZULU, will be created with a 1% **maker** market fee (expressed as 100 in terms of hundredths of a percent) and a 3% **taker** market fee (expressed as 400 in terms of the hundredths of a percent) that is paid to the asset owner.

```
create_asset asset-owner ZULU 2 {"max_supply":"1000000000000000","market_fee_percent":100,"max_market_fee":"1000000000000000","issuer_permissions":1,"flags":1,"core_exchange_rate":{"base":{"amount":1,"asset_id":"1.3.0"},"quote":{"amount":1,"asset_id":"1.3.1"}},"whitelist_authorities":[],"blacklist_authorities":[],"whitelist_markets":[],"blacklist_markets":[],"description":"","extensions":{"taker_fee_percent":300}} null true
```

### Asset Owner: Issue the Asset

The asset will be issued to one of the trading accounts, "bob".

```
issue_asset bob 5000 ZULU "" true
```



## Place Orders

### Overview

Two orders will be constructed such that ZULU should receive 100 XRAY minus the market fee that is dictated by [Example 1](#example-1).

Alice will **first** offer to sell 100 XRAY in exchange for 50 ZULU.  This should make Alice's order the **maker** in the exchange in a future match and fill of her order.

Bob will **next** offer to sell 50 ZULU in exchange for 100 XRAY.  This should match Alice's _pre-existing_ limit order to make Bob's order the **taker** in the match.

Those orders should be matched and filled.

Alice should be charged a **maker** market fee of 1% for her received asset type ZULU.  She should ultimately receive

50 ZULU &times; (100% - 1%) = 50 ZULU &times; (99%) = 49.5 ZULU

Bob should be charged a **taker** market fee of 5% for his received asset type XRAY.

100 XRAY &times; (100% - 5%) = 50 ZULU &times; (95%) = 95 XRAY


### Alice: Sell 100 XRAY

Place an order to sell 100 XRAY for at least 50 ZULU.  Set the order to expire after 600 seconds.

```
sell_asset alice 100 XRAY 50 ZULU 600 false true
```

Check the order book to find the newly placed order.

```
get_order_book XRAY ZULU 1
```

The order will be waiting on the books for a match.


### Bob: Sell 50 ZULU

Place an order to sell 50 ZULU for at least 100 XRAY.  Set the order to expire after 600 seconds.

```
sell_asset bob 50 ZULU 100 XRAY 600 false true
```

Check the order book and find the order

```
get_order_book XRAY ZULU 1
```

The original order by alice will no longer be in the book because it was matched and filled with the order from bob.


### Alice: Check balances and history

Check the account history for more details

```
get_account_history alice 2
```

The fee for the filled transaction should be 1% of the filled order of 50 ZULU = 0.5 ZULU.

Check the balance of Alice to confirm reception of 49.5 ZULU.

```
list_account_balances alice
```

### Bob: Check balances and history

Check the account history for more details

```
get_account_history bob 2
```

The fee for the filled transaction should be 5% of the filled order of 100 XRAY = 5 XRAY.

Check the balance of Bob to confirm reception of 95 XRAY.

```
list_account_balances bob
```



## Check the Distribution of the Market Fees

### Check the Distribution of the Market Fee of XRAY

The market fee of 5 XRAY should be distributed to the asset owner.

#### <div id="check-vesting-balances-of-asset-owner" /> Any Wallet: Check the fees distributed to the Asset Owner

First find the `dynamic_asset_data_id` of the asset

```
get_asset XRAY
```

Use the value for `dynamic_asset_data_id` field in an invocation of `get_object`

```
get_object 2.3.x
```

Inspect the value found in the output for `accumulated_fees` which will be listed in satoshis.  This asset has a precision of 2 which means that the actual fees collected should be divided by 10<sup>2</sup>.  _The result should be 5.00 XRAY which is expressed as 500 XRAY satoshis._


### Check the Distribution of the Market Fee of ZULU

The market fee of 0.5 ZULU should be distributed to the asset owner.

#### <div id="check-vesting-balances-of-asset-owner" /> Any Wallet: Check the fees distributed to the Asset Owner

First find the `dynamic_asset_data_id` of the asset

```
get_asset ZULU
```

Use the value for `dynamic_asset_data_id` field in an invocation of `get_object`

```
get_object 2.3.x
```

Inspect the value found in the output for `accumulated_fees` which will be listed in satoshis.  This asset has a precision of 2 which means that the actual fees collected should be divided by 10<sup>2</sup>.  _The result should be 0.5 ZULU which is expressed as 50 ZULU satoshis_



# Reference

- [Creating New Keys](https://github.com/bitshares/bitshares-core/wiki/CLI-Wallet-Cookbook#creating-new-keys)
- [Claiming Accumulated Fees](https://github.com/bitshares/bitshares-core/wiki/CLI-Wallet-Cookbook#claiming-accumulated-fees)

## Unit Tests

The following unit tests that have been prepared to test the new functionality ([2136](https://github.com/bitshares/bitshares-core/pull/2136)) for the BSIP ([BSIP-81](https://github.com/bitshares/bsips/blob/master/bsip-0081.md)).

- [setting taker fees for a user-issued asset](https://github.com/bitshares/bitshares-core/blob/3408aac0b3b39be992a332f8439573e12f0a4bef/tests/tests/simple_maker_taker_fee_tests.cpp#L82)
- [setting taker fees for a smart asset](https://github.com/bitshares/bitshares-core/blob/3408aac0b3b39be992a332f8439573e12f0a4bef/tests/tests/simple_maker_taker_fee_tests.cpp#L341)
- [default taker fees after hardfork](https://github.com/bitshares/bitshares-core/blob/3408aac0b3b39be992a332f8439573e12f0a4bef/tests/tests/simple_maker_taker_fee_tests.cpp#L434)
- [different maker and taker fees charged when filling limit orders after HF for a user-issued asset](https://github.com/bitshares/bitshares-core/blob/3408aac0b3b39be992a332f8439573e12f0a4bef/tests/tests/simple_maker_taker_fee_tests.cpp#L644)
- [different maker and taker fees charged when filling limit orders after HF for a user-issued asset when the maker fee % is zero](https://github.com/bitshares/bitshares-core/blob/3408aac0b3b39be992a332f8439573e12f0a4bef/tests/tests/simple_maker_taker_fee_tests.cpp#L807)
- [different maker and taker fees charged when filling limit orders after HF for a user-issued asset when the taker fee % is zero](https://github.com/bitshares/bitshares-core/blob/3408aac0b3b39be992a332f8439573e12f0a4bef/tests/tests/simple_maker_taker_fee_tests.cpp#L972)
- [different maker and taker fees charged when filling limit orders after HF for a smart asset](https://github.com/bitshares/bitshares-core/blob/3408aac0b3b39be992a332f8439573e12f0a4bef/tests/tests/simple_maker_taker_fee_tests.cpp#L1134)
- [different maker and taker fees charged when partially filling limit orders](https://github.com/bitshares/bitshares-core/blob/3408aac0b3b39be992a332f8439573e12f0a4bef/tests/tests/simple_maker_taker_fee_tests.cpp#L1324)