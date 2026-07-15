# Universal TX Site Flow (Node-RED)

This folder contains a universal flow template for one transmitter location:

- SNMP fast polling every 5 seconds:
  - reflection
  - deviation
  - tx alarm
  - fault alarm
- SNMP slow polling every 30 seconds:
  - temperature
  - forward power
  - frequency
- Ping health every 5 seconds:
  - gateway
  - PC
  - transmitter
  - Modbus relay controller
- Modbus relay status every 5 seconds through HTTP bridge:
  - GET `http://<ip>:<port>/?data=0101000000083DCC`
- Relay command/ack workflow:
  - API endpoint `POST /tx/relay/set`
  - queueing (serialized command execution)
  - status verification after command
  - retry on mismatch (max 2)
  - ACK event write to InfluxDB
- All data is written to InfluxDB with site tags.

## Files

- `universal_tx_site_flow.json` - importable Node-RED flow

## Import

1. Node-RED menu -> Import -> select `universal_tx_site_flow.json`.
2. Open function node `Init Site Config` and set:
   - `site_id`, `site_name`, `site_city`, `site_lat`, `site_lon`
   - `gateway_ip`, `pc_ip`, `transmitter_ip`, `modbus_ip`
   - SNMP community and OIDs
   - Modbus base URL
3. Configure InfluxDB connection in config node `InfluxDB`.
4. Deploy.

## Requirements

- Node-RED with `node-red-contrib-influxdb` installed.
- `snmpget` command available in OS PATH (net-snmp tools).
- Ping command syntax in `ping_cmd_template`:
  - Windows default in template: `ping -n 1 ${ip}`
  - Linux/macOS: change to `ping -c 1 ${ip}`

## Relay command API

Endpoint:

- `POST /tx/relay/set`

Body:

```json
{
  "channel": 1,
  "state": 1,
  "source": "automation"
}
```

Rules:

- `channel` is 1..8
- `state` is 0 or 1
- returns HTTP 202 when accepted

The flow also includes `TEST relay ch1 ON` inject node for quick manual check.

## Measurements written to InfluxDB

- `tx_telemetry_fast`
- `tx_telemetry_slow`
- `site_ping_health`
- `relay_status`
- `relay_command`
- `relay_ack`

All points include these tags:

- `site_id`
- `site_name`
- `site_city`
- `site_lat`
- `site_lon`
- `device`
- `source`
