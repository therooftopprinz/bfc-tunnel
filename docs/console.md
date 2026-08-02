# Node Console

Per-process control plane for `bfc_tunnel` (one node instance). The simulator and humans connect over plain TCP to `console.address` (default `127.0.0.1:5000`).

## Session

- Commands: one line, terminated by `\n` (optional `\r`).
- Replies: a markdown-style table body, then a blank line (`\n\n`). Tables must not contain empty lines.
- Disconnect (e.g. Ctrl-C in `nc`) closes only that TCP session. The node keeps running.
- `/stop` replies `STATUS=OK`, then terminates the process.
- No `/start`, `/quit`, or auth.

## Common rules

Verbs: `list`, `add`, `delete`, `modify` (plus `select` for identity).

Args are `key=value` tokens. `list` accepts optional `+/-key=filter` (`+` include, `-` exclude).

| Outcome | Reply |
|---------|--------|
| `list` | Data table (header + rows; may be header-only) |
| `add` / `modify` / `select` | Full affected row(s), same columns as `list` |
| Config `delete` | `key`/`value` row after reset to built-in default |
| Other `delete` / `/stop` | `\| STATUS \|\n\| OK \|` |
| Error | `\| STATUS \| MESSAGE \|\n\| ERR \| … \|` (omit unused columns) |

Config resources take settings as **`name=value` directly** (not `key=` / `value=`). Their `list` columns are always `key`, `value`.

## Commands

### `/stop`

Terminates the process after `STATUS=OK`.

### `/security/private_key`

| Verb | Args |
|------|------|
| `list` | filters |
| `add` / `modify` | `node_id=… private_key=…` |
| `delete` | `node_id=…` |

Columns: `node_id`, `private_key` (values are shown in full).

### `/security/public_key`

| Verb | Args |
|------|------|
| `list` | filters |
| `add` / `modify` | `node_id=… public_key=…` (optional `key_type=…`, default `X25519`) |
| `delete` | `node_id=…` |

Columns: `node_id`, `public_key` (and `key_type` when present).

### `/security/config`

Valid keys: `supported_integrity_algorithms`, `supported_confidentiality_algorithms`, `supported_dhke_key_types`.

| Verb | Args |
|------|------|
| `list` | filters |
| `add` / `modify` | `supported_integrity_algorithms=NONE,HMAC_SHA2_256` (etc.) |
| `delete` | key name as `supported_integrity_algorithms` (resets to default, echoes new row) |

### `/transport`

| Verb | Args |
|------|------|
| `list` | filters |
| `add` / `modify` | `name=… type=…` + type fields |
| `delete` | `name=…` |

Types:

- `UNICAST` — `bind`, `mtu`
- `INTERNAL_MULTICAST` — `group`, `port`, `interface`, `mtu` (IGMP join)
- `EXTERNAL_MULTICAST` — `send`, `recv`, `mtu` (`recv` = local bind; `send` = multicaster inject address; raw BTF). Multicaster fan-out is configured outside this process (sim).

### `/network/identity`

| Verb | Args |
|------|------|
| `list` | filters |
| `add` / `modify` | `node_id=… ds_send=… ds_recv=…` |
| `delete` | `node_id=…` |
| `select` | `node_id=…` (success echoes the identity row) |

Columns: `node_id`, `ds_send`, `ds_recv`.

### `/network/static_peer`

| Verb | Args |
|------|------|
| `list` | filters |
| `add` | `peer=host:port` |
| `delete` | `peer=host:port` |

No `modify` (single column). Columns: `peer`.

### `/network/config`

Valid keys: `beacon_interval_ms`, `check_peer_activity_interval_ms`, `default_peer_timeout_s`, `security_ctx_timeout_s`, `security_ctx_grace_period_s`, `transaction_timeout_ms`, `key_refresh_interval_s`, `security_query_timeout_ms`, `query_network_security_retry_timeout_s`, `network_key_seeder`.

`delete` resets to built-in default and echoes the new `key`/`value` row.

### `/peer list`

Active peers. Columns: `node_id`, `public_key`, `preferred_transport`, `preferred_address`.

### `/peer/link list`

Optional `node_id=…`. Without it, lists all peers and includes a `node_id` column.

Columns: `node_id` (when listing all), `transport_name`, `last_activity`, `sent_pkt`, `recv_pkt`, `sent_byt`, `recv_byt`.

## Bootstrap

`bfc_tunnel` may start empty (reactors + console only). Optional `config.ini` and CLI `--key=value` (e.g. `--console.address=127.0.0.1:5000`) apply the same settings the console can set dynamically.
