### Simulator
commands
* `/sim/config add ([key]=[value])+`
* `/sim/config mod ([key]=[value])+`
* `/sim/config rem ([key])+`
* `/sim/config print ([+-][key]=[filter_value])+`

#### Example Config
| key                   | value |
|-----------------------|-------|
| tcp_console_addr      |  9000 |
| multicast_device_addr |  9001 |

### Node
Commands
* `/node add [node_name] [console_port] [int_algs] [conf_algs] [dhke_keys]`
* `/node mod [node_name] ([key]=[value])+`
* `/node rem ([key])+`
* `/node start [node_name]`
* `/node stop  [node_name]`
* `/node print ([+-][key]=[filter_value])+`

#### Example Config
| node_name | console_addr | int_algs      | conf_algs | dhke_kts |
|-----------|--------------|---------------|-----------|----------|
| CENTER_A  |         5010 | HMAC_SHA2_256 | NONE      | X25519   |
| CENTER_B  |         5020 | HMAC_SHA2_256 | NONE      | X25519   |
| GROUND_1  |         5030 | HMAC_SHA2_256 | NONE      | X25519   |
| GROUND_2  |         5040 | HMAC_SHA2_256 | NONE      | X25519   |
|    UAS_1  |         5050 | HMAC_SHA2_256 | NONE      | X25519   |
|    UAS_2  |         5060 | HMAC_SHA2_256 | NONE      | X25519   |
|    UAS_3  |         5070 | HMAC_SHA2_256 | NONE      | X25519   |
|    UAS_4  |         5080 | HMAC_SHA2_256 | NONE      | X25519   |
|    UAS_5  |         5090 | HMAC_SHA2_256 | NONE      | X25519   |
|    UAS_6  |         5100 | HMAC_SHA2_256 | NONE      | X25519   |

```
/node add CENTER_A 5010 HMAC_SHA2_256 NONE X25519
/node add CENTER_B 5020 HMAC_SHA2_256 NONE X25519
/node add GROUND_1 5030 HMAC_SHA2_256 NONE X25519
/node add GROUND_2 5040 HMAC_SHA2_256 NONE X25519
/node add    UAS_1 5050 HMAC_SHA2_256 NONE X25519
/node add    UAS_2 5060 HMAC_SHA2_256 NONE X25519
/node add    UAS_3 5070 HMAC_SHA2_256 NONE X25519
/node add    UAS_4 5080 HMAC_SHA2_256 NONE X25519
/node add    UAS_5 5090 HMAC_SHA2_256 NONE X25519
/node add    UAS_6 5100 HMAC_SHA2_256 NONE X25519
```

### Node Identity
Commands
* `/node/identity add   [node_name] [node_id] [private_key] [ds_send] [ds_recv]`
* `/node/identity mod   [node_name] ([key]=[value])+`
* `/node/identity rem   ([node_name])+`
* `/node/identity print ([+-][key]=[filter_value])+`

#### Example Config
| name      | node_id  | private_key                                                      | ds_send | ds_recv |
|-----------|----------|------------------------------------------------------------------|---------|---------|
|  CENTER_A |  0:0:0:1 | MC4CAQAwBQYDK2VuBCIEIMnJb8Y/eIUuDVbWblfr3Clo0TLGk2AGHK4tIygJ7Qn3 |    5011 |    5012 |
|  CENTER_B |  0:0:0:2 | MC4CAQAwBQYDK2VuBCIEIH6uKx8oEaVEnIpJifd4QyhD2FINB893uZEo+HsXJG9p |    5021 |    5022 |
|  GROUND_1 |  0:0:0:3 | MC4CAQAwBQYDK2VuBCIEILO4VaKOkG+ZLpXn0xgOlmGn+UwHoTNXVJWMHITxLlkJ |    5031 |    5032 |
|  GROUND_2 |  0:0:0:4 | MC4CAQAwBQYDK2VuBCIEIOxDPfKkrukSk+J1EwV+ITtxtVdITcWqCnfudzn/9vlV |    5041 |    5042 |
|     UAS_1 |  0:0:0:5 | MC4CAQAwBQYDK2VuBCIEIDyxrCywRplmZ5R6EZ5dcbXteWYb4t0DxwiwI7HavWcI |    5051 |    5052 |
|     UAS_2 |  0:0:0:6 | MC4CAQAwBQYDK2VuBCIEIOPEiWVj5+xcwQTXb46+5fezLb7yJ3Wvc9T+/3zlG++4 |    5061 |    5062 |
|     UAS_3 |  0:0:0:7 | MC4CAQAwBQYDK2VuBCIEIDb3fG5IztnjL/yQ3UpRiIWQ2KdZTOYkLAUUAhied7ux |    5071 |    5072 |
|     UAS_4 |  0:0:0:8 | MC4CAQAwBQYDK2VuBCIEIF8cefm+L9k7bqn1GcllAsyOyFfyBSvgVBEafVafgqlX |    5081 |    5082 |
|     UAS_5 |  0:0:0:9 | MC4CAQAwBQYDK2VuBCIEIMVQgCR7HAbHu87tjQG88Xpi2dyG9oUzyRB0sSmRfBnK |    5091 |    5092 |
|     UAS_6 | 0:0:0:10 | MC4CAQAwBQYDK2VuBCIEIL4h0emtHtzyURNsZ+4WPpGHL8yH8/fKkPJSjIEfehqs |    5101 |    5102 |

#

```
/node add CENTER_A  0:0:0:1 MC4CAQAwBQYDK2VuBCIEIMnJb8Y/eIUuDVbWblfr3Clo0TLGk2AGHK4tIygJ7Qn3
/node add CENTER_B  0:0:0:2 MC4CAQAwBQYDK2VuBCIEIH6uKx8oEaVEnIpJifd4QyhD2FINB893uZEo+HsXJG9p
/node add GROUND_1  0:0:0:3 MC4CAQAwBQYDK2VuBCIEILO4VaKOkG+ZLpXn0xgOlmGn+UwHoTNXVJWMHITxLlkJ
/node add GROUND_2  0:0:0:4 MC4CAQAwBQYDK2VuBCIEIOxDPfKkrukSk+J1EwV+ITtxtVdITcWqCnfudzn/9vlV
/node add    UAS_1  0:0:0:5 MC4CAQAwBQYDK2VuBCIEIDyxrCywRplmZ5R6EZ5dcbXteWYb4t0DxwiwI7HavWcI
/node add    UAS_2  0:0:0:6 MC4CAQAwBQYDK2VuBCIEIOPEiWVj5+xcwQTXb46+5fezLb7yJ3Wvc9T+/3zlG++4
/node add    UAS_3  0:0:0:7 MC4CAQAwBQYDK2VuBCIEIDb3fG5IztnjL/yQ3UpRiIWQ2KdZTOYkLAUUAhied7ux
/node add    UAS_4  0:0:0:8 MC4CAQAwBQYDK2VuBCIEIF8cefm+L9k7bqn1GcllAsyOyFfyBSvgVBEafVafgqlX
/node add    UAS_5  0:0:0:9 MC4CAQAwBQYDK2VuBCIEIMVQgCR7HAbHu87tjQG88Xpi2dyG9oUzyRB0sSmRfBnK
/node add    UAS_6 0:0:0:10 MC4CAQAwBQYDK2VuBCIEIL4h0emtHtzyURNsZ+4WPpGHL8yH8/fKkPJSjIEfehqs
```

###  Node Transport
Commands
* `/node/transport add [name] [type] [itf] [in_addr] [ext_addr]`
* `/node/transport rem [name] [type] [itf] [in_addr] [ext_addr]`
* `/node/transport print ([+-][key]=[filter_value])+`

#### Example Config
| name     | type      | itf | in_addr | ext_addr |
|----------|-----------|-----|---------|----------|
| CENTER_A | UNICAST   | UC0 |   20100 |    40100 |
| CENTER_B | UNICAST   | UC0 |   20200 |    40200 |
| GROUND_1 | UNICAST   | UC0 |   20300 |    40300 |
| GROUND_2 | UNICAST   | UC0 |   20400 |    40400 |
| UAS_1    | MULTICAST | MC0 |   20500 |          |
| UAS_2    | MULTICAST | MC0 |   20600 |          |
| UAS_3    | MULTICAST | MC0 |   20700 |          |
| UAS_4    | MULTICAST | MC0 |   20800 |          |
| UAS_5    | MULTICAST | MC0 |   20900 |          |
| UAS_6    | MULTICAST | MC0 |   21000 |          |

```
/node/transport add CENTER_A UNICAST   UC0 20100 40100
/node/transport add CENTER_B UNICAST   UC0 20200 40200
/node/transport add GROUND_1 UNICAST   UC0 20300 40300
/node/transport add GROUND_2 UNICAST   UC0 20400 40400
/node/transport add UAS_1    MULTICAST MC0 20500
/node/transport add UAS_2    MULTICAST MC0 20600
/node/transport add UAS_3    MULTICAST MC0 20700
/node/transport add UAS_4    MULTICAST MC0 20800
/node/transport add UAS_5    MULTICAST MC0 20900
/node/transport add UAS_6    MULTICAST MC0 21000
```

### Graph
Commands
* `/graph add [name] [itf] [type] [peer_node] [ab_flowgrp] [ab_flowgrp]`
* `/graph mod [name] [itf] [type] [peer_node] ([key]=[value])+`
* `/graph rem [name] [itf] [type] [peer_node]`
* `/graph print ([+-][key]=[filter_value])+`

#### Example Config
| name     | itf | type      | peer     | ab_flowgrp  | ab_flowgrp  |
|----------|-----|-----------|----------|-------------|-------------|
| CENTER_B | UC0 | UNICAST   | CENTER_A | CENTER_B_UL | CENTER_A_UL |
| GROUND_1 | UC0 | UNICAST   | CENTER_A | GROUND_1_UL | CENTER_A_UL |
| GROUND_2 | UC0 | UNICAST   | CENTER_B | GROUND_2_UL | CENTER_B_UL |
| GROUND_1 | MC0 | MULTICAST | UAS_1    | WIRELESS_1  | WIRELESS_1  |
| GROUND_1 | MC0 | MULTICAST | UAS_2    | WIRELESS_1  | WIRELESS_1  |
| GROUND_1 | MC0 | MULTICAST | UAS_3    | WIRELESS_1  | WIRELESS_1  |
| UAS_1    | MC0 | MULTICAST | GROUND_1 | WIRELESS_1  | WIRELESS_1  |
| UAS_1    | MC0 | MULTICAST | UAS_2    | WIRELESS_1  | WIRELESS_1  |
| UAS_1    | MC0 | MULTICAST | UAS_3    | WIRELESS_1  | WIRELESS_1  |
| UAS_2    | MC0 | MULTICAST | GROUND_1 | WIRELESS_1  | WIRELESS_1  |
| UAS_2    | MC0 | MULTICAST | UAS_1    | WIRELESS_1  | WIRELESS_1  |
| UAS_2    | MC0 | MULTICAST | UAS_3    | WIRELESS_1  | WIRELESS_1  |
| UAS_3    | MC0 | MULTICAST | GROUND_1 | WIRELESS_1  | WIRELESS_1  |
| UAS_3    | MC0 | MULTICAST | UAS_1    | WIRELESS_1  | WIRELESS_1  |
| UAS_3    | MC0 | MULTICAST | UAS_2    | WIRELESS_1  | WIRELESS_1  |
| GROUND_2 | MC0 | MULTICAST | UAS_4    | WIRELESS_2  | WIRELESS_2  |
| GROUND_2 | MC0 | MULTICAST | UAS_5    | WIRELESS_2  | WIRELESS_2  |
| GROUND_2 | MC0 | MULTICAST | UAS_6    | WIRELESS_2  | WIRELESS_2  |
| UAS_4    | MC0 | MULTICAST | GROUND_2 | WIRELESS_2  | WIRELESS_2  |
| UAS_4    | MC0 | MULTICAST | UAS_5    | WIRELESS_2  | WIRELESS_2  |
| UAS_4    | MC0 | MULTICAST | UAS_6    | WIRELESS_2  | WIRELESS_2  |
| UAS_5    | MC0 | MULTICAST | GROUND_2 | WIRELESS_2  | WIRELESS_2  |
| UAS_5    | MC0 | MULTICAST | UAS_4    | WIRELESS_2  | WIRELESS_2  |
| UAS_5    | MC0 | MULTICAST | UAS_6    | WIRELESS_2  | WIRELESS_2  |
| UAS_6    | MC0 | MULTICAST | GROUND_2 | WIRELESS_2  | WIRELESS_2  |
| UAS_6    | MC0 | MULTICAST | UAS_4    | WIRELESS_2  | WIRELESS_2  |
| UAS_6    | MC0 | MULTICAST | UAS_5    | WIRELESS_2  | WIRELESS_2  |

```
/graph add CENTER_B UC0 UNICAST   CENTER_A CENTER_B_UL CENTER_A_UL
/graph add GROUND_1 UC0 UNICAST   CENTER_A GROUND_1_UL CENTER_A_UL
/graph add GROUND_2 UC0 UNICAST   CENTER_B GROUND_2_UL CENTER_B_UL
/graph add GROUND_1 MC0 MULTICAST UAS_1    WIRELESS_1  WIRELESS_1
/graph add GROUND_1 MC0 MULTICAST UAS_2    WIRELESS_1  WIRELESS_1
/graph add GROUND_1 MC0 MULTICAST UAS_3    WIRELESS_1  WIRELESS_1
/graph add UAS_1    MC0 MULTICAST GROUND_1 WIRELESS_1  WIRELESS_1
/graph add UAS_1    MC0 MULTICAST UAS_2    WIRELESS_1  WIRELESS_1
/graph add UAS_1    MC0 MULTICAST UAS_3    WIRELESS_1  WIRELESS_1
/graph add UAS_2    MC0 MULTICAST GROUND_1 WIRELESS_1  WIRELESS_1
/graph add UAS_2    MC0 MULTICAST UAS_1    WIRELESS_1  WIRELESS_1
/graph add UAS_2    MC0 MULTICAST UAS_3    WIRELESS_1  WIRELESS_1
/graph add UAS_3    MC0 MULTICAST GROUND_1 WIRELESS_1  WIRELESS_1
/graph add UAS_3    MC0 MULTICAST UAS_1    WIRELESS_1  WIRELESS_1
/graph add UAS_3    MC0 MULTICAST UAS_2    WIRELESS_1  WIRELESS_1
/graph add GROUND_2 MC0 MULTICAST UAS_4    WIRELESS_2  WIRELESS_2
/graph add GROUND_2 MC0 MULTICAST UAS_5    WIRELESS_2  WIRELESS_2
/graph add GROUND_2 MC0 MULTICAST UAS_6    WIRELESS_2  WIRELESS_2
/graph add UAS_4    MC0 MULTICAST GROUND_2 WIRELESS_2  WIRELESS_2
/graph add UAS_4    MC0 MULTICAST UAS_5    WIRELESS_2  WIRELESS_2
/graph add UAS_4    MC0 MULTICAST UAS_6    WIRELESS_2  WIRELESS_2
/graph add UAS_5    MC0 MULTICAST GROUND_2 WIRELESS_2  WIRELESS_2
/graph add UAS_5    MC0 MULTICAST UAS_4    WIRELESS_2  WIRELESS_2
/graph add UAS_5    MC0 MULTICAST UAS_6    WIRELESS_2  WIRELESS_2
/graph add UAS_6    MC0 MULTICAST GROUND_2 WIRELESS_2  WIRELESS_2
/graph add UAS_6    MC0 MULTICAST UAS_4    WIRELESS_2  WIRELESS_2
/graph add UAS_6    MC0 MULTICAST UAS_5    WIRELESS_2  WIRELESS_2
```

### Flow Group
Commands
* `/graph/flowgroup add [name] [lat_ms] [throughput_kbps]`
* `/graph/flowgroup mod [name] ([key]=[value])+`
* `/graph/flowgroup rem [name]`
* `/graph/flowgroup print ([+-][key]=[filter_value])+`

#### Example Config
| name        | lat_ms | tput_kbps |
|-------------|--------|-----------|
| CENTER_A_UL |     50 |    102400 |
| CENTER_B_UL |     50 |    102400 |
| GROUND_1_UL |     50 |    102400 |
| GROUND_2_UL |     50 |    102400 |
| WIRELESS_1  |      1 |     25600 |
| WIRELESS_2  |      1 |     25600 |

```
/graph/flowgroup add CENTER_A_UL 50 102400
/graph/flowgroup add CENTER_B_UL 50 102400
/graph/flowgroup add GROUND_1_UL 50 102400
/graph/flowgroup add GROUND_2_UL 50 102400
/graph/flowgroup add WIRELESS_1   1  25600
/graph/flowgroup add WIRELESS_2   1  25600
```

### Config Generation
Each instance of node will be deployed in `sim/nodes/$node_name`
File Structure
* `sim/nodes/$node_name/cfg.ini` - main config file
* `sim/nodes/$node_name/private_key.csv` - this node private keys
* `sim/nodes/$node_name/public_key.csv` - this network public keys

#### private_key.csv
Derivation:
```
${select node_id, private_key from $(/node print +node_name=$node_name)}
```

#### public_key.csv
```
${select node_id, to_public_key(private_key) from '/node print'}
```

#### cfg.ini
```
security.private_key_file = private_keys.csv
security.public_key_file  = public_keys.csv
security.supported_integrity_algorithms       = ${select int_algs from $(/node print +node_name='$node_name')}
security.supported_confidentiality_algorithms = ${select conf_algs from $(/node print +node_name='$node_name')}
security.supported_dhke_key_types             = ${select dhke_kts from $(/node print +node_name='$node_name')}

transport_transform(${select itf,to_type(type),to_addr(${select value from $(/sim/config print +key='mcd_addr')),to_addr(in_addr) from $(/node/transport print +node_name=$node_name)})
node_identity_transform(${select node_id, to_addr(ds_send), to_addr(ds_recv) from $(/node/identity print +node_name='$node_name')})
node_static_peer_transform(${select to_addr(t.ext_addr) from $(/graph print +type=UNICAST +name='$node_name') g inner join $(/node/transport print) t on (g.peer=t.name and g.itf=t.itf) )
```

Functions
* `to_public_key` - converts pks#8 private key to a public key
* `to_addr` - converts partial address (only port) to full address `127.0.0.1:$port`, otherwise nothing if already full address
* `to_type` - converts MULTICAST to EXTERNAL_MULTICAST, otherwise UNICAST
* `transport_transform` convert rows into transport entries, generate size
* `node_identity_transform` convert rows into node.identity entries, generate size
* `node_static_peer_transform` convert rows into node.static_peers entries, generate size

### Implementation

Packet flow
```
to_id(a,b):
    return (max(a,b),min(a,b))

on_packet(in_socket, packet):
    flow_id = to_id(packet.from, in_socket.addr)
    is_ab = packet.from > socket.addr
    flow = flows.find(flow_id)
    profile = is_ab ? flow.ab_profile : flow.ba_profile
    if (profile.should_drop(packet.size)):W
        return
    flow.to_output(packet);
```