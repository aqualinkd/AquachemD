
# AquaChemD HomeKit Mapping Guide

Integrating AquaChemD into the Apple Home app via Home Assistant requires creative re-mapping because HomeKit lacks native support for pool chemistry sensors like pH or ORP. By utilizing Home Assistant as a bridge, you can translate these values into accessory types that are actionable within the Home ecosystem[cite: 1].

---

## 🧪 Chemistry Sensors (pH & ORP)
Since Apple Home does not have a "pH Sensor" category, you must use existing categories that display numerical values.

*   **Humidity Sensor (Recommended):** This is the most common workaround[cite: 1]. Even though it shows a `%` symbol, it allows you to see the actual numerical value (e.g., `7.4` for pH or `720` for ORP) directly on the main tile in the Home app.
*   **Light Sensor:** An alternative that displays values in `lux`[cite: 1]. 
*   **Occupancy/Status Alerts:** For critical levels, create a binary "Occupancy Sensor" in Home Assistant that triggers when chemistry is out of range. In the Home App, this appears as a "Status" alert at the top of the screen (e.g., *"Pool Chemistry Alert"*)[cite: 1].

---

## 💧 Dosing Pumps (Acid & Chlorine)
Using the **Faucet** or **Valve** categories allows you to take advantage of HomeKit's built-in UI for duration and state.

*   **Chlorine Doser (ORP):** The **Faucet** category is ideal for chlorine[cite: 1]. Since ORP doses often run for 5–25 minutes (e.g., `1500` seconds), they align perfectly with HomeKit’s "Remaining Duration" timer, which usually operates in **minutes**[cite: 1].
*   **Acid Doser (pH):** Because acid doses are typically short bursts in **seconds**, the Faucet UI may look awkward (showing `0` or `1` minute)[cite: 1]. For these, a standard **Switch** or **Valve** is cleaner[cite: 1]. You can use a script in Home Assistant to manage the precise "Off" command while HomeKit just sees an "Active" toggle.
*   **State Logic:** Your strategy of mapping every state other than "ON" to "OFF" works perfectly here[cite: 1]. HomeKit will simply see the pump as "Idle" (OFF) even if the controller is in "Enabled/Auto" mode, and "Active" (ON) only when liquid is actually moving.

---

## 🛡️ Safety Conditions & Interlocks
The "Conditions" (Flow sensors, Filter pump status) are the most critical safety elements. These should be mapped to HomeKit **Safety** sensors[cite: 1].

*   **Flow Cell Level Sensor:** Map this as a **Leak Sensor** or **Contact Sensor**[cite: 1]. If the level drops, the Home App will send a high-priority notification such as *"Leak Detected"* or *"Sensor Opened,"* ensuring you know immediately if flow has failed[cite: 1].
*   **Pump/SWG Status:** Map these as **Motion Sensors**[cite: 1]. When the pump is running, "Motion" is detected. This allows you to easily build HomeKit Automations like: *"Only allow Chlorine Faucet to turn on if Pump Motion Sensor detects activity"*[cite: 1].

---

## Summary Mapping Table

| AquaChemD Entity | HomeKit Category | Practical Benefit |
| :--- | :--- | :--- |
| **pH Reading** | **Humidity Sensor** | Shows the numerical value (7.x) on the tile[cite: 1]. |
| **ORP Reading** | **Light Sensor** | Alternative for displaying large numbers (700+)[cite: 1]. |
| **Acid Doser** | **Switch / Valve** | Best for precise, short "seconds-based" bursts[cite: 1]. |
| **Chlorine Doser** | **Faucet** | Supports "Remaining Duration" in minutes[cite: 1]. |
| **Flow Sensor** | **Leak / Contact** | Triggers high-priority "Status" alerts on iOS[cite: 1]. |
| **Pump Interlock** | **Motion Sensor** | Used as a "Permissive" for HomeKit automation rules[cite: 1]. |


AquaChemD Entity,HomeKit Category,Why?
pH Reading,Humidity Sensor,Shows the numerical value on the tile.
ORP Reading,Light Sensor,Alternative way to show numerical values.
Acid Doser,Switch / Valve,"Best for short ""seconds"" based bursts."
Chlorine Doser,Faucet,"Supports ""Remaining Duration"" in minutes."
Flow Sensor,Leak / Occupancy,"Triggers high-priority ""Status"" alerts in the Home App."
Pump Interlock,Motion Sensor,"Easiest way to use a state as a ""Condition"" for HomeKit rules."

