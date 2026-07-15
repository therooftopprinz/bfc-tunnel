# Behavoir Definitions

## [Node] Node
###  [Node.PeerCreationAndDeletition] Peer Creation and Deletion
 Peer Creation:
 * Peer is created when a node receive its beacon from any of its transport.
 * Node should maintain peer link statistic on all the link the peer is available.

 Peer Deletion
 * Peer is deleted when no activity is detected for the configured (node.peer_timeout_s) amount of time.
 * Peer link statistic is deleted for the particular link that doesn't have activity for configured amount of time.

### [Node.NetworkKeyAcquisition] Network Keys Acquisition
Node will initially query_network_security procedure to identify all the peers network security contexts.

**Querying public network security information**

```uml
Alice                                    Bob (broadcast/all_unicast)
  | PUBLIC:query_network_security         |
  +-------------------------------------->|
  | PUBLIC:network_security_information   |
  |<--------------------------------------+

```

All node that received the query_network_security should respond network_security_information.
network_security_information is not used for conflict resolution.

For the initial setup, the network_security_information messages received will be used as a basis on which peers to get the network keys. Then the node will do peer network key aquisition.

**Querying private network security information**

```uml
Alice                                    Bob (peer)
  | PEER:msg1                             |
  +-------------------------------------->| 
  | PEER:msg2                             |
  |<--------------------------------------+
  | PEER:network_keys_request             |
  +-------------------------------------->|
  | PEER:network_keys_response            |
  |<--------------------------------------+
```

If peer security is already established, peer can skip peer security establishment.
The node may do one or more peer network key aquisition from other peers.

**Network Security Conflict Resolution**

Conflict occurs when there are multiple existing context with different expiration time and priority.
The oldest non expiring key with the highest priority wins.
The key is expiring if the expiration time is less than 30s from the current time.

Triggers:
* On startup
* On NETWORK RX
    * When context is unrecognized
    * When context is recognized and the integrity check failed 
* On NETWORK TX
    * When context is not available

### [Node.NetworkKeyRefresh] Network Key Refresh
Node will periodically (node.network_key_refresh_interval_s) send its network keys (NETWORK:network_key_refresh)

Triggers:
* Timer
