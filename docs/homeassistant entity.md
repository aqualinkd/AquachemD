# AquaChemD: Home Assistant Entity & Logic Guide

When integrating **AquaChemD** into Home Assistant, selecting the correct `device_class` and `state_class` is critical. This ensures that data is not only displayed correctly but is also compatible with long-term statistics, history graphing, and the Energy/Utility dashboards[cite: 1].

---

## 📊 Chemistry Sensors (pH, ORP, Temp)
For these probes, we use standard sensor definitions with specific metadata to enable Home Assistant's high-resolution graphing.

*   **pH Sensor**
    *   **Unit:** `pH`
    *   **State Class:** `measurement`[cite: 1]
    *   **Why:** Tells HA to keep a history of every small change. pH doesn't have a `device_class`, but setting the unit to `pH` allows HA to scale the Y-axis correctly (typically 0–14)[cite: 1].
*   **ORP Sensor**
    *   **Unit:** `mV`
    *   **Device Class:** `voltage` (Optional)[cite: 1]
    *   **Why:** While ORP isn't "electrical voltage" in the traditional sense, using `voltage` or leaving it as a generic `measurement` ensures HA treats it as a volatile numerical value for graphing[cite: 1].
*   **Temperature**
    *   **Unit:** `°C` or `°F`
    *   **Device Class:** `temperature`[cite: 1]
    *   **Why:** This automatically enables the correct icons and allows HA to perform unit conversions (C to F) globally based on user settings[cite: 1].

---

## 💧 Dosing Consumption (The "Utility" Logic)
This is the most specialized part of the configuration. To track how much acid/chlorine you use over time, we use the **Volumetric Event** model[cite: 1].

*   **Total Dose Sensor**
    *   **Topic:** `aquachemd/<id>/total_acid_ml` or `total_chlorine_ml`[cite: 1]
    *   **Unit:** `mL`[cite: 1]
    *   **Device Class:** `volume`[cite: 1]
    *   **State Class:** `measurement`[cite: 1]
    *   **Why:** By posting each dose as a `measurement` of `volume`, you can point a **HA Utility Meter Helper** at this sensor. By checking the **"Delta Values"** box in the helper, HA adds every incoming dose together to create Daily, Weekly, and Monthly totals[cite: 1].

---

## 🛡️ Safety & Interlock Conditions
These are the "permissives" that allow the system to run safely.

*   **Binary Sensors (Flow/Level)**
    *   **Device Class:** `problem`, `motion`, or `moisture`[cite: 1]
    *   **Why:** Using `device_class: problem` for a flow sensor means HA will show "OK" when the state is `on` and "Problem" (in red) when the state is `off`[cite: 1]. Using `motion` for the filter pump status makes it easy to see "Active" vs "Idle" in the UI[cite: 1].

---

## ⚙️ Doser Controls (Switches & Selects)
*   **Mode Select (`select` entity)**
    *   **Options:** `Off`, `Enabled (Auto)`, `On (Manual)`[cite: 1]
    *   **Why:** A `select` entity is better than a simple toggle because it allows for the "Auto" state. This distinguishes between the pump being "Broken/Off" and the pump being "Waiting for a Trigger (Auto)"[cite: 1].
*   **Status Switch (`switch` entity)**
    *   **Why:** Only used if you want a quick "Kill Switch" for the dosing logic. In HomeKit, we map everything other than `On` to `Off` to maintain a simple user experience[cite: 1].

---

## Summary of Home Assistant Metadata

| Attribute | Value | Result in HA |
| :--- | :--- | :--- |
| **`state_class: measurement`** | Numerical | Enables 24h/48h history line graphs[cite: 1]. |
| **`state_class: total_increasing`** | Accumulating | Enables the "Long Term Statistics" and Energy Dashboard[cite: 1]. |
| **`device_class: volume`** | `mL` / `L` / `gal` | Allows HA to auto-convert units and use liquid icons[cite: 1]. |
| **`device_class: problem`** | Binary | Highlights sensor failures in the UI with color coding[cite: 1]. |