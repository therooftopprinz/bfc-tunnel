**Milestone**
* [x] Peer Security
  * [x] Noise KK MSG1/MSG2 handshake + key derivation
  * [x] MSG1 plaintext (`sec_ctx` NONE)
  * [x] Peer ctx grace / renew / expire / prefer non-expiring
  * [x] Peer create on beacon; teardown clears timers, txs, sec ctx
  * [x] Peer TX protection (`peer_send_message`: select sec_ctx, MAC, confidentiality)
  * [x] Peer RX accept path (MAC verify + decrypt against peer sec ctx)
  * [x] Store receive-direction keys (`derive_receive_*`) for bidirectional crypto
  * [x] Reserve `sec_ctx` 0 (`NONE`); allocate peer ctx ids from 1..15
  * [x] Concurrent MSG1: priority arbitration (docs aligned)
* [x] Network Security
  * [x] PUBLIC query + peer-defer retry; one query+acquire cycle
  * [x] Skip MSG1/MSG2 when peer security already usable
  * [x] No empty `NETWORK_SECURITY_INFORMATION` / `NETWORK_KEYS_RESPONSE`
  * [x] Acquisition triggers (startup / unknown ctx / unproven MAC fail / TX miss)
  * [x] Conflict resolution + integrity_success/failure counters
  * [x] Key refresh NETWORK broadcast TTL 0 + integrity MAC
  * [x] Peer-secure `NETWORK_KEYS_REQUEST` / `NETWORK_KEYS_RESPONSE` channel
  * [x] Network confidentiality on NETWORK / NETWORK_OVER_PEER frames
  * [x] Refresh install via conflict resolution after accept (documented)
  * [x] Finish `behavior.md` Node.Security key selection; implement selection policy
* [x] Both (Peer + Network)
  * [x] `sn` replay-window checks on RX
* [ ] Simulation
* [ ] Routing
