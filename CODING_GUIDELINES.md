# CODING GUIDELINES

Universal FM SNMP Telemetry Module
Version 1.0

## 1. General Rules

- Firmware must be designed for 24/7 operation.
- Priorities: stability, reliability, readability, maintainability, memory efficiency.
- Do not implement quick fixes that cannot scale.

## 2. Language and Environment

- Language: C++
- Platform: PlatformIO
- Target: ESP32-WROOM-32UE
- Use ESP-IDF capabilities and FreeRTOS where needed.
- Avoid blocking functions and long delay calls in main execution paths.

## 3. Code Organization

- Keep code modular.
- Each functional block must have its own class and .h/.cpp pair.
- main.cpp should only initialize modules and start tasks.

## 4. Naming

- Classes: PascalCase
- Functions and variables: camelCase
- Constants: UPPER_CASE

## 5. Global Variables

- Avoid global variables when possible.
- Use managers and structured objects.
- Keep shared configuration in ConfigManager.

## 6. Memory Constraints

- Prefer flash for static data.
- Avoid large mutable String usage.
- Prefer fixed-size buffers and static structures.
- Keep web assets in flash.

## 7. FreeRTOS Structure

- Use a small number of tasks with clear ownership.
- Suggested split:
  - Communication Task: Ethernet, WiFi, Web, WebSocket, SNMP
  - Measurement Task: ADS1115, filtering, calibration, alarms
- Use events for cross-module notifications.

## 8. Blocking Rules

- No long waits or endless blocking loops.
- Use vTaskDelay, timers, and event-driven flow.
- No module may block the entire system.

## 9. Analog Subsystem

- ADS1115 access must stay in AnalogManager.
- AnalogManager handles ADC reads, filtering, calibration, and engineering units.
- Other modules must not access ADC hardware directly.

## 10. Calibration

- Keep calibration data out of hardcoded formulas.
- Support offset, gain, LUT, and interpolation.
- Store calibration in flash/NVS.

## 11. SNMP Rules

- SNMPManager must not read hardware directly.
- SNMP should consume values from dedicated managers.

## 12. Alarm Rules

- AlarmManager handles digital input processing, debounce, and state change events.
- On alarm change: log event, send SNMP trap, notify web clients.

## 13. Web Interface

- Use async web architecture and WebSocket for realtime updates.
- Avoid periodic full-page refresh.

## 14. Configuration

- Persist user parameters in Preferences/NVS.
- Avoid hardcoded values that operators need to modify.

## 15. Logging

- Use structured event logging with timestamp, type, and description.

## 16. Error Recovery

- Communication modules must implement timeout, reconnect, and recovery.
- A single link failure must not stop the device.

## 17. Watchdog

- Use watchdog and feed it from each main task.

## 18. Comments

- Comments must explain why, not restate obvious code behavior.

## 19. AI Coding Rule

- Implement in phases.
- Prefer simpler and more reliable solutions when multiple options exist.
