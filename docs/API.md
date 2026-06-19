Note: this documentation is probably outdated, please check https://github.com/bitshares/bitshares-core#using-the-api .
### Node API

The `witness_node` software provides several different API's, known as *node API*.

To provide the API's, the node need to be started with RPC connection enabled, which can be done by starting the node with the `--rpc-endpoint` parameter, E.G.

    ./programs/witness_node/witness_node --rpc-endpoint=127.0.0.1:8090

Each API has its own ID and a name.
When running `witness_node` with RPC connection enabled, initially two API's are available:
* API 0 has name *"database"*, it provides read-only access to the database,
* API 1 has name *"login"*, it is used to login and gain access to additional, restrictable API's.

Here is an example using `wscat` package from `npm` for websockets:

    $ npm install -g wscat
    $ wscat -c ws://127.0.0.1:8090
    > {"id":1, "method":"call", "params":[0,"get_accounts",[["1.2.0"]]]}
    < {"id":1,"result":[{"id":"1.2.0","annotations":[],"membership_expiration_date":"1969-12-31T23:59:59","registrar":"1.2.0","referrer":"1.2.0","lifetime_referrer":"1.2.0","network_fee_percentage":2000,"lifetime_referrer_fee_percentage":8000,"referrer_rewards_percentage":0,"name":"committee-account","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[],"address_auths":[]},"active":{"weight_threshold":6,"account_auths":[["1.2.5",1],["1.2.6",1],["1.2.7",1],["1.2.8",1],["1.2.9",1],["1.2.10",1],["1.2.11",1],["1.2.12",1],["1.2.13",1],["1.2.14",1]],"key_auths":[],"address_auths":[]},"options":{"memo_key":"GPH1111111111111111111111111111111114T1Anm","voting_account":"1.2.0","num_witness":0,"num_committee":0,"votes":[],"extensions":[]},"statistics":"2.7.0","whitelisting_accounts":[],"blacklisting_accounts":[]}]}

We can do the same thing using an HTTP client such as `curl` for API's which do not require login or other session state:

    $ curl --data '{"jsonrpc": "2.0", "method": "call", "params": [0, "get_accounts", [["1.2.0"]]], "id": 1}' http://127.0.0.1:8090/
    {"id":1,"result":[{"id":"1.2.0","annotations":[],"membership_expiration_date":"1969-12-31T23:59:59","registrar":"1.2.0","referrer":"1.2.0","lifetime_referrer":"1.2.0","network_fee_percentage":2000,"lifetime_referrer_fee_percentage":8000,"referrer_rewards_percentage":0,"name":"committee-account","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[],"address_auths":[]},"active":{"weight_threshold":6,"account_auths":[["1.2.5",1],["1.2.6",1],["1.2.7",1],["1.2.8",1],["1.2.9",1],["1.2.10",1],["1.2.11",1],["1.2.12",1],["1.2.13",1],["1.2.14",1]],"key_auths":[],"address_auths":[]},"options":{"memo_key":"GPH1111111111111111111111111111111114T1Anm","voting_account":"1.2.0","num_witness":0,"num_committee":0,"votes":[],"extensions":[]},"statistics":"2.7.0","whitelisting_accounts":[],"blacklisting_accounts":[]}]}

When using an HTTP client, the API ID can be replaced by the API name, E.G.

    $ curl --data '{"jsonrpc": "2.0", "method": "call", "params": ["database", "get_accounts", [["1.2.0"]]], "id": 1}' http://127.0.0.1:8090/

The definition of all node API's is available in the source code files including
[database_api.hpp](https://github.com/bitshares/bitshares-core/blob/master/libraries/app/include/graphene/app/database_api.hpp)
and [api.hpp](https://github.com/bitshares/bitshares-core/blob/master/libraries/app/include/graphene/app/api.hpp).
Corresponding documentation can be found in Doxygen:
* [database API](https://doxygen.bitshares.org/classgraphene_1_1app_1_1database__api.html)
* [other API's](https://doxygen.bitshares.org/namespacegraphene_1_1app.html)


### Wallet API

The `cli_wallet` program can also be configured to serve **all of its commands** as API's, known as *wallet API*.

Start `cli_wallet` with RPC connection enabled:

    $ ./programs/cli_wallet/cli_wallet --rpc-endpoint=127.0.0.1:8091

Access the wallet API using an HTTP client:

    $ curl --data '{"jsonrpc": "2.0", "method": "info", "params": [], "id": 1}' http://127.0.0.1:8091/
    $ curl --data '{"jsonrpc": "2.0", "method": "get_account", "params": ["1.2.0"], "id": 1}' http://127.0.0.1:8091/

Note: The syntax to access *wallet API* is a bit different than accessing *node API*.

**Important:**
* When RPC connection is enabled for `cli_wallet`, sensitive data E.G. private keys which is accessible via commands will be accessible via RPC too. It is recommended that only open network connection to localhost or trusted addresses E.G. configure a firewall.
* When using wallet API, sensitive data E.G. the wallet password and private keys is transmitted as plain text, thus may be vulnerable to network sniffing. It is recommended that only use wallet API with localhost, or in a clean network, and / or use `--rpc-tls-endpoint` parameter to only serve wallet API via secure connections.


Accessing restrictable node API's
---------------------------------

You can restrict node API's to particular users by specifying an `api-access` file in `config.ini`
or by using the `--api-access /full/path/to/api-access.json` startup node command.  Here is an example `api-access` file which allows
user `bytemaster` with password `supersecret` to access four different API's, while allowing any other user to access the three public API's
necessary to use the node:

    {
       "permission_map" :
       [
          [
             "bytemaster",
             {
                "password_hash_b64" : "9e9GF7ooXVb9k4BoSfNIPTelXeGOZ5DrgOYMj94elaY=",
                "password_salt_b64" : "INDdM6iCi/8=",
                "allowed_apis" : ["database_api", "network_broadcast_api", "history_api", "network_node_api"]
             }
          ],
          [
             "*",
             {
                "password_hash_b64" : "*",
                "password_salt_b64" : "*",
                "allowed_apis" : ["database_api", "network_broadcast_api", "history_api"]
             }
          ]
       ]
    }

Passwords are stored in `base64` as salted `sha256` hashes.  A simple Python script,
[`saltpass.py`](https://github.com/bitshares/bitshares-core/blob/master/programs/witness_node/saltpass.py)
is avaliable to obtain hash and salt values from a password.
A single asterisk `"*"` may be specified as username or password hash to accept any value.

With the above configuration, here is an example of how to call `add_node` from the `network_node` API:

    {"id":1, "method":"call", "params":[1,"login",["bytemaster", "supersecret"]]}
    {"id":2, "method":"call", "params":[1,"network_node",[]]}
    {"id":3, "method":"call", "params":[2,"add_node",["127.0.0.1:9090"]]}

Note, the call to `network_node` is necessary to obtain the correct API identifier for the network API.  It is not guaranteed that the network API identifier will always be `2`.

The restricted API's are accessible via HTTP too using *basic access authentication*. E.G.

    $ curl --data '{"jsonrpc": "2.0", "method": "call", "params": ["network_node", "add_node", ["127.0.0.1:9090"]], "id": 1}' http://bytemaster:supersecret@127.0.0.1:8090/

Our `doxygen` documentation contains the most up-to-date information
about API's for the [node](https://doxygen.bitshares.org/namespacegraphene_1_1app.html) and the
[wallet](https://doxygen.bitshares.org/classgraphene_1_1wallet_1_1wallet__api.html).
