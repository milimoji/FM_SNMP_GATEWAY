# FM_SNMP_GATEWAY

Universal FM transmitter telemetry gateway for Raspberry Pi Pico 2 (RP2350A) with an external W5500 Ethernet module.

## Architecture

- ConfigManager: persistent settings in LittleFS (`/fm_config.bin`)
- AnalogManager: filtered telemetry pipeline with linear, quadratic, and LUT calibration
- AlarmManager: debounced digital alarm/status inputs
- NetworkManager: W5500 Ethernet only
- SNMPManager: SNMP GET/GETNEXT/SET(read-only)/TRAP over EthernetUDP
- WebManager: HTTP dashboard on Ethernet port 80
- EventLogger: fixed-size in-memory event ring

Measurement runs on core 1, communication on core 0.

## Build and run

- Board env: `rp2350a` (Pico 2 / RP2350A)
- Build: PlatformIO Build
- Upload: PlatformIO Upload
- Monitor: PlatformIO Monitor at 115200

## Hardware pinout (Pico 2)

- ADS1115 I2C SDA: GP4
- ADS1115 I2C SCL: GP5
- Alarm input: GP6 (active low, pull-up)
- Status input: GP7 (active low, pull-up)
- W5500 MISO: GP16
- W5500 CS: GP17
- W5500 SCK: GP18
- W5500 MOSI: GP19
- W5500 RST: GP20 (active low)
- W5500 INT: GP21 (reserved, unused)
- Network reset button: GP22 to GND (internal pull-up)
- Fan tach: GP26 (pull-up, interrupt)
- Fan PWM: GP27 (25 kHz)

Hold the network-reset button for 5 seconds to restore DHCP, IP `192.168.1.50`, gateway `192.168.1.1`, mask `255.255.255.0`, DNS `8.8.8.8`, and hostname `fm-snmp-gateway`. Calibration, fan, and SNMP trap settings are kept, then the board reboots. A button held at boot does not wipe settings.

## Network

- Ethernet only (no WiFi, no mDNS)
- DHCP default, static IP optional from the web UI
- MAC is derived from the Pico unique board id (locally administered)
- If the link drops, Ethernet is retried every 5 seconds

## SNMP

- Library: `patricklaf/SNMP`
- Agent listens on UDP port 161
- OID map:

```json
{
	"fwd": "1.3.6.1.4.1.35305.1.1.2.5.0",
	"ref": "1.3.6.1.4.1.35305.1.1.2.6.0",
	"temp": "1.3.6.1.4.1.35305.1.1.2.12.0",
	"freq": "1.3.6.1.4.1.35305.1.1.2.3.0",
	"modu": "1.3.6.1.4.1.35305.1.1.2.7.0",
	"alarms": {
		"tx": "1.3.6.1.4.1.35305.1.1.1.6.0",
		"fault": "1.3.6.1.4.1.35305.1.1.1.7.0"
	}
}
```

TRAP on alarm input edge (`kAlarmInputPin`):

- Notification OID: `1.3.6.1.4.1.35305.1.1.5.4`
- State OID: `1.3.6.1.4.1.35305.1.1.1.6.0` (`1` = ON, `0` = OFF)

## Web UI

- `http://<ip>/`
- Tabs: Telemetry, Network, Fan, Calibration
- Live data via `GET /api/state`
- Save: `POST /api/config/save`
- Reboot: `POST /api/reboot`

## Fan PWM

- 25 kHz analog PWM on GP27
- 100% duty for the first 15 seconds (configurable)
- Then linear from T_start to T_max (default 30% at 30 C, 100% at 65 C)
