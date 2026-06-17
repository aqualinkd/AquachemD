# AquachemD  
Linux daemon to read pH, ORP, Temperature sensors, control chem feeders & GPIO. Provides MQTT client. Compatible with most Home control systems including Apple HomeKit, Home Assistant, Samsung, Alexa, Google, etc.


# AquaChemD: Autonomous Pool Chemistry Management System

**AquaChemD** is an open-source, high-precision automated pool chemical dosing and monitoring controller designed to bridge the gap between expensive proprietary systems and DIY enthusiasts. It provides a professional-grade platform for monitoring pH, ORP (Oxidation-Reduction Potential), and Temperature with autonomous logic for balancing water chemistry[cite: 1].

# Currently in development, (Any release before V1.0.0 is development)
## ToDo before 1st release
* Finish web config editor
* Config file writebac (core dump)
* Restart from UI not showing startup
* Print stats on a reset.
* Use averages for dosing.
* MQTT Value Sensor time default to state vs value before MQTT message
* Look at using SWG% to increase dose times.
---

## 🚀 Project Overview
The core mission of AquaChemD is to ensure pool water safety and clarity through precise chemical dispensing while providing total transparency and integration for the modern smart home[cite: 1]. 

Built on a lightweight C core, the system runs on low-power Linux-based hardware (such as Raspberry Pi) and interfaces with industrial sensors via I2C and GPIO[cite: 1]. This ensures high reliability in harsh pool-equipment environments[cite: 1].

---

## 🔌 Connectivity & Integration

### 1. MQTT & Open API
AquaChemD utilizes a comprehensive **MQTT API** for all telemetry and control[cite: 1]. 
* **Universal Access:** Allows any third-party software to subscribe to live chemistry data or trigger manual dosing events[cite: 1].
* **Local-First:** Avoids cloud dependencies, ensuring data remains private and responsive[cite: 1].
* **Interlock Sync:** Listens to external equipment (like AqualinkD) to ensure dosing only occurs when the filter pump is running[cite: 1].

### 2. Home Assistant (HA) Integration
The system features native **MQTT Discovery**, automatically populating Home Assistant with high-utility entities[cite: 1]:
* **Live Sensors:** pH, ORP, and Temperature with historical graphing (Long-Term Statistics)[cite: 1].
* **Status Entities:** "OK/Problem" binary sensors for flow and safety interlocks[cite: 1].
* **Dose Tracking:** Volumetric sensors tracking mL of Acid and Chlorine dispensed per day/week using `total_increasing` state classes[cite: 1].
* **Control Selectors:** Mode switches (Off / On / Auto) for each pump[cite: 1].

### 3. Apple HomeKit
By leveraging the Home Assistant HomeKit Bridge, AquaChemD data is exposed to the Apple ecosystem[cite: 1]:
* **Visibility:** View pool temperature and status directly in the **Apple Home App**[cite: 1].
* **Siri Voice Control:** Ask "Siri, what is the pool pH?" or "Is the chlorine pump running?"
* **Critical Alerts:** Receive iOS notifications for hardware alerts, such as "Acid Tank Low" or flow failures[cite: 1].

---

## 🛡️ Safety & Logic
Dosing is controlled by a multi-step threshold engine. The amount dispensed is calculated based on the deviation from the setpoint, preventing over-correction[cite: 1]:

$$Dose = Duration_{seconds} \times FlowRate_{mL/s}$$

**Critical Safety:** Multi-level hardware and software interlocks ensure that no chemical is dispensed if the water is not flowing, preventing the buildup of dangerous chlorine gas[cite: 1].

---

## 🛠️ Technical Architecture
* **Sensors:** Support for Atlas Scientific EZO circuits (pH, ORP, Temp) and One-Wire (DS18B20) probes[cite: 1].
* **Actuators:** GPIO-driven relay control for peristaltic pumps[cite: 1].
* **Interlocks:** Supports physical flow cell level sensors and software-based MQTT permissives[cite: 1].



# 

# AquaChemD Configuration Guide

This document describes the configuration options for the **AquaChemD** pool chemistry controller.

## 1. System & Web Settings
| Option | Description |
| :--- | :--- |
| `main_label` | The display name for the primary sampling/control group (e.g., Sampling). |
| `listen_address` | The IP and port for the internal web server (e.g., `http://0.0.0.0:88`). |
| `log_level` | Controls logging verbosity. Options: `notice`, `info`, `debug`. |
| `web_directory` | The local file system path where the web UI assets are stored. |

---

## 2. MQTT Configuration
Settings for connecting to your MQTT broker and Home Assistant discovery.

| Option | Description |
| :--- | :--- |
| `mqtt_server` | The URI of the MQTT broker (e.g., `mqtt://homeassistantdev:1883`). |
| `mqtt_user` | Username for MQTT authentication. |
| `mqtt_passwd` | Password for MQTT authentication. |
| `mqtt_aquachemd_topic` | The base topic for this device's data (default: `aquachemd`). |
| `mqtt_aqualinkd_topic` | The base topic to listen for data from an AqualinkD instance. |
| `mqtt_discovery_topic` | (Optional) The prefix for Home Assistant MQTT Discovery. |
| `mqtt_discovery_use_mac` | If `YES`, appends the device MAC address to discovery IDs. |
| `mqtt_convert_to_degF` | If `YES`, converts temperature readings from Celsius to Fahrenheit for MQTT. |
| `mqtt_timed_update` | If `YES`, forces an MQTT update at regular intervals even if values haven't changed. |

---

## 3. Global Sensor Logic
| Option | Description |
| :--- | :--- |
| `gpio_chip` | The path to the GPIO character device (e.g., `/dev/gpiochip0`). |
| `sensor_poll_time` | Frequency in seconds to poll the connected sensors. |
| `temp_compensated_ph` | If `YES`, pH readings are automatically adjusted based on current water temperature. |

---

## 4. Dosing Logic (Thresholds)
AquaChemD uses a multi-step threshold system to determine pump runtimes based on chemical readings.

### pH Dosing (Acid)
*   **Default Time:** `ph_default_dose_time` (Seconds used as a fallback).
*   **Range Logic:** `ph_dose_range=threshold:seconds`
    *   *Example:* `8.0:20` means if pH is **>= 8.0**, run the pump for 20 seconds.
    *   The system evaluates thresholds to find the highest matching bracket. A value of `0` stops dosing.

### ORP Dosing (Chlorine)
*   **Default Time:** `orp_default_dose_time`
*   **Range Logic:** `orp_dose_range=threshold:seconds`
    *   *Example:* `650:1500` means if ORP is **<= 650**, run for 1500 seconds.
    *   The system evaluates thresholds in ascending order. A value of `0` (e.g., at 750) stops dosing.

---

## 5. Safety & Interlock Conditions
These conditions act as "permissives." If any condition is not met, dosing is disabled to prevent chemical damage or dangerous gas buildup.

### MQTT Interlocks
Used to monitor external states like a "Filter Pump" or "Salt Cell Flow."
*   `mqtt_condition_label`: Display name for the safety check.
*   `mqtt_condition_topic`: The MQTT topic to monitor.
*   `mqtt_condition_value`: The required raw value to permit dosing.

### GPIO Interlocks
Used for local hardware safety sensors (e.g., physical Flow Switches or Tank Level Sensors).
*   `gpio_condition_label`: Display name.
*   `gpio_condition_pin`: Physical GPIO pin number.
*   `gpio_condition_pin_mode`: `active_high` or `active_low`.
*   `gpio_condition_required_state`: `on` or `off`.

---

## 6. Sensor Definitions
Define the probes attached to the system. **Note:** The `_label` must always be the first line of a sensor block.

| Type | Required Keys | Description |
| :--- | :--- | :--- |
| **ezo** | `_address` | Atlas Scientific EZO circuit via I2C (e.g., `0x63`). |
| **mqtt** | `_topic` | Pulls sensor data from an external MQTT topic (e.g., AqualinkD). |
| **d1w** | `_path`, `_offset`, `_scale` | DS18B20 One-Wire temperature sensors via sysfs. |

---

## 7. Doser (Pump) Definitions
Configure the relay or GPIO pins driving the chemical delivery pumps.

| Option | Description |
| :--- | :--- |
| `ph_doser_label` | Name of the acid pump. |
| `orp_doser_label` | Name of the chlorine pump. |
| `_type` | Hardware type, usually `gpio`. |
| `_pin` | The GPIO pin number on the specified `gpio_chip`. |
| `_pin_mode` | `active_low` (relay triggered by GND) or `active_high`. |
| `_required_state` | State needed for the pump to be considered "Active." |
| `_ml_per_second` | The flow rate of the pump used to calculate and report dose volume. |