## Steps performed during application (`witness_node`) startup:

1. Create `application`

1.1. Create `database`

1.1.1 Initialize indexes

1.1.2 Initialize evaluators

2. Register plugins
3. Enable plugins from `plugins` option
4. Initialize `application`
5. Initialize plugins
6. Startup `application`

6.1 Open database (includes loading state from disk and replay if required)

6.2 Start listening on network sockets

7. Startup enabled plugins
8. Wait for shutdown signal
9. Shut down plugins
10. Shut down node

10.1 Shut down P2P network

10.2 Close database

10.2.1 Dump database to disk
