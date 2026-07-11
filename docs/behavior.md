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
When the node has doesn't have the security context for a network:
1. Identify a peers that have a key (beacon.flag & K_BEACON_FLAG_HAS_NETWORK_KEY)
2. Randomly select a peer with active security context or if the peer has no active security, do security procedure.
3. Exchange network keys with the peer (PEER:network_key_exchange).

Triggers:
* On NETWORK RX
* On NETWORK TX

### [Node.NetworkKeyRefresh] Network Key Refresh
Node will periodically (node.network_key_refresh_interval_s) send its network keys (NETWORK:network_key_refresh)

Triggers:
* Timer
