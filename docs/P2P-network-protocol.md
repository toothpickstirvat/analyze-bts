## Overview

BitShares nodes communicate with each other through a peer-to-peer (P2P) network protocol.

Each node accepts connections on a (not necessarily public) TCP socket. Immediately after the connection has been established, nodes exchange cryptographic keys that are subsequently used for encrypting the traffic within that connection.

The protocol consists of messages that are exchanged through the encrypted connection. The protocol supports various message types for requesting information or transmitting blockchain items.

## Communication layers

### Encryption layer

All network traffic after the initial key exchange is encrypted with AES-256.

For the key exchange, each node creates a random private key on the secp256k1 curve, computes the corresponding public key and transmits that in clear over the connection.

Upon receiving the remote public key, it is multiplied with the own private key. The resulting curve point is hashed with SHA-512 to produce a 512 bit shared secret.

From this shared secret, a 256 bit key is created by hashing it with SHA-256. Similarly, a 128 bit IV is created by hashing the secret with city_hash_128. The 256-bit key and the 128-bit IV are then used to set up AES-256-CBC en/decryption streams for sending and receiving data.

### Messaging layer

Messages consist of an 8 bytes header (4 bytes little-endian integer size, 4 bytes little-endian integer type) plus actual message content. The content is a binary serialized representation of a data structure indicated by the type field.

For transmission, messages are padded to a multiple of 16 bytes. (16 bytes is the block size handled by the underlying AES streams. Thus, an messages can always be en/decrypted without having to wait for further data.)

### Messages

Message types are defined in [libraries/net/include/graphene/net/core_messages.hpp](https://github.com/bitshares/bitshares-core/blob/3.2.1/libraries/net/include/graphene/net/core_messages.hpp#L68-L87). Data structures are defined in the same file.

Unknown message types between 5000 and 5099 are silently ignored. They are assumed to be newer protocol messages.
Message types outside this range are assumed to contain blockchain objects. Currently, this can be either stand-alone transactions, or blocks.

## Connection lifecycle

P2P connections are typically long-lived. A node will try to connect to a certain minimum number of peers and can accept additional connections up to a certain maximum number. Peers are only disconnected when they are misbehaving in some sense.

### Handshake

After setting up encryption, both peers send a [hello_message](https://github.com/bitshares/bitshares-core/blob/3.2.1/libraries/net/include/graphene/net/core_messages.hpp#L189-L201) to each other. Both sides perform some sanity checking on the hello_messages they receive, and reply with either [connection_accepted_message](https://github.com/bitshares/bitshares-core/blob/3.2.1/libraries/net/include/graphene/net/core_messages.hpp#L225-L230) or [connection_rejected_message](https://github.com/bitshares/bitshares-core/blob/3.2.1/libraries/net/include/graphene/net/core_messages.hpp#L241-L249). In the latter case, they also initiate disconnecting the peer.

After receiving a connection_accepted_message, a node will request its peer's peers by sending an address_request_message. Also, if it doesn't know its own firewall status, or considers its knowledge stale, it will send a check_firewall_message.

After receiving a connection_rejected_message, a node will also send an address_request_message, although it may be unlikely(?) to receive a response before the connection is closed.

During the handshake, the node keeps the connection in its "handshaking" list. After a successful handshake (i. e. after receiving an address_message in reply to the initial address_request_message, or, for inbound connections, after receiving a fetch_blockchain_item_ids_message), the node moves the connection into its "active" list and sends a current_time_request_message to the peer. Finally, it starts the [synchronization process](#fetch_blockchain_item_ids_message) with the new peer.

The handshake can also time out, which leads to the connection being closed.

### Disconnecting

A node that wants to disconnect and has already received a [closing_connection_message](https://github.com/bitshares/bitshares-core/blob/3.2.1/libraries/net/include/graphene/net/core_messages.hpp#L305-L311) simply closes the connection. If it hasn't, it sends a closing_connection_message and waits for the peer either sending a closing_connection_message or closing it.

Note: before sending a closing_connection_message, the node moves the connection from its "active" list to its "closing" list. It is likely that the peer will subsequently timeout due to this.

Note: a node will close the connection forcibly at most `GRAPHENE_NET_PEER_DISCONNECT_TIMEOUT` seconds after sending a closing_connection_message.

## Normal operation

The node implements several ["loops"](https://github.com/bitshares/bitshares-core/wiki/Threading#p2p-thread) for handling connections. Depending on connection state (i. e. the list containing the connection), the connection may be used by one or more such loops.

Active connections are used by
* fetch_sync_items_loop
* fetch_items_loop
* advertise_inventory_loop
* terminate_inactive_connections_loop (disconnect if last message was received too long ago)
* fetch_updated_peer_lists_loop

Note: a known problem from testnet is that active connections will be terminated if the connection is too slow to transmit a large message (such as a huge block) within the timeout, because terminate_inactive_connections_loop only considers *complete* messages.

### address_request_message

An [address_request_message](https://github.com/bitshares/bitshares-core/blob/3.2.1/libraries/net/include/graphene/net/core_messages.hpp#L263-L265) is sent every 15 minutes by `fetch_updated_peer_lists_loop` to all active peers.

Upon receiving it, the node compiles a list of all its active peers into an address_message (see below) and sends it back to the requesting peer.

### address_message

Endpoints contained in incoming [address_message](https://github.com/bitshares/bitshares-core/blob/test-3.2.1/libraries/net/include/graphene/net/core_messages.hpp#L298-L302)s are merged into the node's list of potential peers, updating all entries that are already present.

If the local node is still in handshaking mode, the incoming address_message signals the end of the handshaking process. Success or failure are determined by a previously received connection_accepted_message or connection_rejected_message (see [above](#Handshake)). In case of success, the connection is moved into the "active" list.

### check_firewall_message

A [check_firewall_message](https://github.com/bitshares/bitshares-core/blob/test-3.2.1/libraries/net/include/graphene/net/core_messages.hpp#L351-L355) can mean two things:

1. The sending node wants to know its own firewall status. In this case, the message contains an empty node id and IP endpoint. Note that the sending node may not know its external IP, but the receiving node does. In that case, the receiving node fills in the missing fields and forwards the message to a different peer.

2. The receiving node is asked to check the firewall status of the node specified by the message's node id and IP endpoint, probably because the sending node has forwarded a request as described above. If the receiving node is already connected to the target node, it prepares (but doesn't send, BUG???) a reply indicating that it is unable to check. Otherwise, it attempts to connect to the specified node, marking the peer_connection object with an appropriate firewall_check_state. If a connection attempt succeeds or fails for a peer that is thus marked, a [check_firewall_reply_message](https://github.com/bitshares/bitshares-core/blob/test-3.2.1/libraries/net/include/graphene/net/core_messages.hpp#L365-L370) is sent back to the node from which the check_firewall_message was received.

### check_firewall_reply_message

Like the request, the reply can mean different things depending on the origin of the request. Unsolicited replies are ignored.

If the receiving node was the originator of the request, it updates its view of itself (i. e. its externally visible endpoint and firewall status) according to the reply.

If the receiving node had forwarded the request, it forwards the reply to the originating node. In case of success it also updates its own peer database accordingly. An unable_to_check reply leads to the request being forwarded to another peer.

### current_time_request_message

[current_time_request_message](https://github.com/bitshares/bitshares-core/blob/3.2.1/libraries/net/include/graphene/net/core_messages.hpp#L323-L326) is sent after a successful handshake, and also in regular intervals on otherwise idle connections (as a keepalive mechanism).

Upon receiving one, a node immediately sends back a [current_time_reply_message](https://github.com/bitshares/bitshares-core/blob/test-3.2.1/libraries/net/include/graphene/net/core_messages.hpp#L334-L339).

### current_time_reply_message

When a node receives a current_time_reply_message, it updates the internal clock_offset and round_trip_delay fields associated with the sending node, by calculating approximate values for these from the fields of the message structure.

### get_current_connections_request_message

Currently never sent by core code. Node replies with [get_current_connections_reply_message](https://github.com/bitshares/bitshares-core/blob/test-3.2.1/libraries/net/include/graphene/net/core_messages.hpp#L390-L399).

### get_current_connections_reply_message

Ignored by receiving node.

### trx_message and block_message

[trx_message](https://github.com/bitshares/bitshares-core/blob/test-3.2.1/libraries/net/include/graphene/net/core_messages.hpp#L93-L97)s contain transactions and [block_message](https://github.com/bitshares/bitshares-core/blob/test-3.2.1/libraries/net/include/graphene/net/core_messages.hpp#L104-L113)s contain blocks (obviously).

The P2P layer handles them both in the same way: if they were sent unsolicited, they are ignored. If they were asked for, they are forwarded to the application layer and processed, i. e. transactions are applied to the chain and blocks are either applied or added to the fork database. If processing was successful, they are added to the list of known items, which is then broadcast to all connected peers.

### item_ids_inventory_message

advertise_inventory_loop generates and broadcasts [item_ids_inventory_message](https://github.com/bitshares/bitshares-core/blob/3.2.1/libraries/net/include/graphene/net/core_messages.hpp#L117-L122) to all connected peers whenever new items appear in the local inventory. Only "new" items are sent to a peer, i. e. items that we have neither sent to nor received from it.

When such a message is received from a peer, all items that are not locally available yet are added to our knowledge of the peer's inventory, and will eventually be fetched by the fetch_items loop.

Note: the cache timeout for detecting "new" items per peer is GRAPHENE_NET_MAX_INVENTORY_SIZE_IN_MINUTES (currently 2).

### fetch_items_message

[fetch_items_message](https://github.com/bitshares/bitshares-core/blob/test-3.2.1/libraries/net/include/graphene/net/core_messages.hpp#L163-L168) is used by fetch_items_loop and fetch_sync_items_loop to request items from peers.

Upon receiving a fetch_items_message, the node responds with one message per requested item. For items that it doesn't know about it sends an [item_not_available_message](https://github.com/bitshares/bitshares-core/blob/3.2.1/libraries/net/include/graphene/net/core_messages.hpp#L177-L181), for others it replies with a corresponding trx_message or block_message, depending on the requested item type.

### item_not_available_message

When a node receives this in response to a fetch_items_message requesting that item, it is removed from our cache of the peer's inventory. If it was a sync item, the node stops fetching blocks from that peer, and possibly disconnects.

The message is ignored if the corresponding item wasn't requested.

### fetch_blockchain_item_ids_message

[fetch_blockchain_item_ids_message](https://github.com/bitshares/bitshares-core/blob/3.2.1/libraries/net/include/graphene/net/core_messages.hpp#L149-L154) and its response counterpart, [blockchain_item_ids_inventory_message](https://github.com/bitshares/bitshares-core/blob/3.2.1/libraries/net/include/graphene/net/core_messages.hpp#L131-L137), form the backbone of the blockchain synchronization mechanism. Sychronization with a peer is started immediately after the initial handshake has been completed, and re-triggered whenever a new block has been applied to the chain successfully.

The message contains a [synopsis](https://github.com/bitshares/bitshares-core/blob/3.2.1/libraries/app/application.cpp#L747-L804) of the undoable blocks the sending node has on its main chain. The receiving node compares this with its own list of known blocks. The following outcomes are possible:

1. The receiving peer is on a fork that is unreachable for the sender, i. e. the synopsis contains no block IDs that are on its main chain.
2. The peers are on different forks but the sender could switch if it wanted to.
3. The receiver is ahead of the sender, i. e. it has blocks that the other one needs.
4. The peers are in sync.
5. The receiver is behind the sender and needs blocks from them.

In the first case (unreachable fork), the node replies with empty list, then disconnects the sender.
In all other cases, the node replies with a (possibly empty) list of up to 2000 block IDs from its main chain that it thinks the sender needs. 
In addition, in cases 2 (reachable fork) and 5 (receiver behind sender), the receiving node also restarts synchronization with the peer by sending a new fetch_blockchain_item_ids_message.

Note: a node never sends item IDs that are not on its main chain, even if the sender is on a fork that it knows about.

### blockchain_item_ids_inventory_message

If this message is received unsolicited, i. e. not in response to a previously sent fetch_blockchain_item_ids_message, it is ignored.

Otherwise, the contained list is checked for plausibility, and if successful it is merged into the cached list of blocks the peer knows about. If the sending peer has items that the receiving node needs, the fetch_sync_items_loop is triggered, otherwise another batch of item IDs is requested by sending a new fetch_blockchain_item_ids_message.
