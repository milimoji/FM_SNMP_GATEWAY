# FM_SNMP_GATEWAY

Universal FM transmitter telemetry gateway for ESP32-WROOM-32U.

## Current architecture (v1 skeleton)

- ConfigManager: persistent settings in Preferences
- AnalogManager: filtered telemetry pipeline with linear, quadratic, and LUT calibration
- AlarmManager: debounced digital alarm/status inputs
- NetworkManager: link abstraction with Ethernet-first hook and Wi-Fi fallback
- SNMPManager: SNMP GET/GETNEXT/SET(read-only)/TRAP agent and alarm event flow
- WebManager: web/WebSocket integration hook
- OTAManager: OTA update support
- EventLogger: fixed-size in-memory event ring for operational logs

## Source layout

- src/main.cpp
- src/AnalogManager.h, src/AnalogManager.cpp
- src/SNMPManager.h, src/SNMPManager.cpp
- src/NetworkManager.h, src/NetworkManager.cpp
- src/WebManager.h, src/WebManager.cpp
- src/AlarmManager.h, src/AlarmManager.cpp
- src/ConfigManager.h, src/ConfigManager.cpp
- src/OTAManager.h, src/OTAManager.cpp
- src/EventLogger.h, src/EventLogger.cpp

## Build and run

- Build: PlatformIO Build
- Upload: PlatformIO Upload
- Monitor: PlatformIO Monitor

Upload when module arrives:

- Connect ESP32-WROOM-32UE via USB UART.
- Set upload_port in platformio.ini if auto-detection fails.
- Run PlatformIO Upload.
- Run PlatformIO Monitor at 115200.

## Hardware pinout (ESP32-WROOM-32UE)

- Alarm input: GPIO34
- Status input: GPIO35
- ADS1115 I2C SDA: GPIO21
- ADS1115 I2C SCL: GPIO22
- W5500 SPI SCK: GPIO18
- W5500 SPI MISO: GPIO19
- W5500 SPI MOSI: GPIO23
- W5500 CS: GPIO5 (hardcoded in firmware)
- W5500 RST: GPIO32 (hardcoded in firmware)
- FORCE WIFI jumper pin: GPIO27 to GND (configurable via network.forceWifiPin)
- Fan tach input (PULLUP): GPIO26 (configurable via fanTachPin)
- Fan PWM output (LEDC 25 kHz): GPIO25 (fixed in firmware)

Pin assignment in code:

- All hardware pins are centralized in [src/PinConfig.h](src/PinConfig.h).
- ADS1115 I2C is initialized explicitly with SDA GPIO21 and SCL GPIO22.
- W5500 SPI is initialized explicitly with SCK GPIO18, MISO GPIO19, MOSI GPIO23, CS GPIO5, and RST GPIO32.

## Basic connection parameters

WiFi AP:

- Mode: AP (device broadcasts its own WiFi network)
- SSID/password are fixed in firmware (hardcoded) and are not editable from Web UI.
- Hardcoded values are defined in src/ConfigManager.cpp.
- AP IP: default ESP32 SoftAP IP (typically 192.168.4.1)

Ethernet (W5500):

- Startup sequence: if Ethernet is enabled and link is available, device boots on Ethernet and keeps WiFi AP disabled unless FORCE WIFI is active
- Fallback: if Ethernet is not available, device starts WiFi AP
- Runtime priority (when Ethernet is enabled and link is up): Ethernet over WiFi
- Enable key in config: network.ethEnabled
- CS pin key in config: network.ethCsPin
- MAC key in config: network.ethMac
- DHCP default: enabled
- Static mode fields: network.ip, network.gateway, network.subnet, network.dns

WiFi access for phone-based configuration:

- Control key: network.wifiConfigAccessEnabled (default: true)
- Time window key: network.wifiConfigWindowSec (default: 300 sec)
- Behavior: WiFi AP is disabled whenever Ethernet is online, unless FORCE WIFI is active.
- FORCE WIFI jumper behavior: when jumper requests FORCE WIFI, WiFi AP remains enabled even if Ethernet is online.
- FORCE WIFI control keys: network.forceWifiPinEnabled, network.forceWifiPinActiveLow, network.forceWifiPin.

SNMP:

- Library: `patricklaf/SNMP` via GitHub dependency.
- Agent listens on UDP port 161.
- Default OIDs are mapped in `SNMPManager` and currently expose forward power, reflected power, temperature, fan RPM, fan duty, and alarm states.
- FM-style OID map (for Node-RED / integration):

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

SNMP TRAP (TX alarm from GPIO34):

- Trigger condition: edge change on alarm input GPIO34 (`kAlarmInputPin`).
- Alarm source behavior: open-collector alarm output, active when pulled LOW.
- Trap destination: configurable `trapIp` and `trapPort` (default port 162).
- Notification OID (`snmpTrapOID.0`): `1.3.6.1.4.1.35305.1.1.5.4` (eventTxAlarm).
- Alarm state varbind OID: `1.3.6.1.4.1.35305.1.1.1.6.0` (tsTxAlarm).
- Alarm state value:
	- `1` = alarm ON
	- `0` = alarm OFF

Persistence:

- WiFi and Ethernet core parameters are persisted in Preferences/NVS by ConfigManager.

## EventLogger transport

- EventLogger output path is restricted to WebSocket publishing path in WebManager.
- No EventLogger serial printing path is used.

## Web UI live data and control

- Top section is updated via WebSocket with Forward, Reflect, Temperature, Deviation, and Fan RPM.
- Top section also shows Fan PWM duty percent.
- Top section also shows fan health status (OK/CLAMPED/FALLBACK/TACH_MISSING).
- Deviation is shown as numeric value and horizontal VU bar.
- Lower section contains configuration fields and Save button.
- Reboot action is available to apply network changes when needed.
- Fan RPM is displayed in Web UI only; SNMP GET does not expose fan RPM.

## Fan PWM control

- PWM output uses ESP32 LEDC at 25 kHz.
- On power-up, fan is forced to 100% duty for first 15 seconds (default, configurable).
- After startup phase, duty is linear vs. temperature from T_start to T_max.
- Default curve: min 30% at 30 C, up to 100% at 65 C.
- Parameters are configurable and persisted: enable, T_start, T_max, min/max duty, startup max seconds.

## ADC calibration in Web UI (Specification point 8)

- Calibration fields are available for all 4 ADC channels.
- Supported modes:
	- Linear: Y = K * X + Offset
	- Quadratic: Y = A*X^2 + B*X + C
	- LUT table with linear interpolation between points
- LUT input format in UI: adc:value;adc:value (example: 1800:0;2200:25;2600:100)
- Calibration values are saved in Flash/NVS through ConfigManager.
- UI validates LUT rows before save and shows per-channel row error messages.
- Forward and Reflect channels include preset LUT buttons based on the reference example.

## Next hardware steps

1. Add custom OID editing if you want runtime SNMP map changes.
2. Extend SNMP access policy if writable OIDs are needed.
3. Add HTTP + WebSocket UI refinements in WebManager.

## Implemented from specification in this revision

- ADS1115 integration in AnalogManager with AIN0..AIN3 channel mapping.
- W5500 Ethernet priority path with WiFi fallback reconnect behavior.
- SNMP manager OID registry and request handlers are wired to the SNMP library.
