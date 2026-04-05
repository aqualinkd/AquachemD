
# AquachemD — Flow Cell Design Notes
  

## TLDR Final Design

  Flow cell in vertical orientation build with 3/4 Clear PVC tubing.
  
```
  ┌ ← 3/4" elbow → horizontal ball valve → 3/8" (or 1/2") outlet tube (slopes down to pre-pump return)
  │
  ├──── Union Coupling        (disconnect for maintenance without cutting pipe)
  │
  ├──── T  [Flow Switch]      (triggers only when cell fully flooded — safety interlock)
  │
  ├──── T  [ORP Probe]        (downstream of pH per Atlas Scientific recommendation)
  │
  ├──── T  [pH Probe]         (upstream of ORP per Atlas Scientific recommendation)
  │
  ├──── T  [PT-1000 Temp]     (first to submerge on fill, provides temp compensation data)
  │
  ├──── Union Coupling        (disconnect for maintenance without cutting pipe)
  │
  └ ← 3/4" elbow → horizontal ball valve → 3/8" (or 1/2") inlet tube (from post-filter tap)
```
## Overview

Four options were evaluated for mounting Atlas Scientific pH, ORP, and PT-1000
temperature sensors to read pool water chemistry. This document summarises each
option, the reasons Option 3 (DIY clear PVC flow cell) was selected, and the
rationale for a vertical orientation.

---

## Option 1 — Inline in 2" Main PVC Pipe

Sensors threaded directly into the main 2" return line after the filter using
inline fittings.

**Pros**
- Simplest installation — no bypass plumbing required
- Sensors always in the primary flow stream
- Fewest components and potential leak points on the bypass side

**Cons**
- High flow velocity in 2" pipe causes turbulence around sensor tips, directly
  affecting pH and ORP accuracy which require stable water contact
- Sensor removal for calibration requires shutting down the pump or installing
  isolation valves around every sensor port individually
- Physically awkward to fit multiple sensors on a single 2" pipe run
- A failed fitting or loose sensor is a significant leak on the main line
- Sensor Damage Probes are exposed to the full force and debris of the pool's flow. High velocity (>10 ft/s) can significantly shorten their lifespan.
---

## Option 2 — Commercial Flow Cell (IPS FC100G or Hayward CAX-20272)

A purpose-built acrylic bypass chamber with pre-drilled sensor ports, plumbed
as a sidestream off the main line.

**Pros**
- Sensors removable for calibration without disturbing main flow or shutting
  down the pump
- Controlled low-velocity flow gives more stable sensor readings
- Pre-positioned sensor ports with correct geometry
- Clear acrylic provides visual confirmation of water flow
- Integrated float switch port on most models

**Cons**
- Cost: $80–$150 depending on model
- The IPS FC100G has only two usable top ports (pH and ORP), with the third
  occupied by the integrated flow/float switch — no port available for a
  temperature sensor without modification
- The bottom drain/sample port is a poor candidate for a temperature sensor as
  it faces downward and partially exposes the sensor to air when the cell drains
- Pre-drilled ports are sized and positioned for IPS's own sensor ecosystem —
  adapting Atlas Scientific probes with 3/4" NPT threads requires additional
  fittings
- Less flexibility in orientation — commercial cells are designed for a specific
  mounting position

**Why rejected:** The FC100G is fundamentally a two-sensor cell. Adding a third
sensor (PT-1000 temperature) requires fighting the design. At $100+ it is hard
to justify when the sensor port limitations remain.

---

## Option 3 — DIY Flow Cell with Clear 3/4" PVC (Selected)

A custom bypass manifold built from clear schedule 40 3/4" PVC T fittings in
series, with 3/8" quick-connect flexible tubing tapped into the 2" main line
after the filter (higher pressure) and returning before the pump (lower
pressure). The pressure differential creates natural bypass flow without a
secondary pump.

**Pros**
- Fully customisable — sensor count, spacing, and orientation chosen for the
  application not the manufacturer's ecosystem
- Atlas Scientific probes use 3/4" NPT threads which tap directly into 3/4" PVC
  T branches with no adapters or reducers
- Larger internal water volume than a commercial cell gives more representative
  readings and less susceptibility to stagnation between pump cycles
- Clear PVC gives visual confirmation of flow
- Can be oriented vertically or horizontally depending on available space
- Union couplings on inlet and outlet allow the entire cell to be disconnected
  for maintenance without cutting pipe
- Ball valves on inlet and outlet provide flow control and isolation
- Significantly cheaper than a commercial cell
- Infinitely expandable — adding a fourth or fifth sensor is simply another T
  fitting in the manifold

**Cons**
- More plumbing work than commercial options
- More individual fittings means more potential leak points
- Clear schedule 40 PVC fittings can be harder to source locally than standard
  white PVC — spa and hot tub suppliers are the best source
- Requires careful sizing of the 3/8" inlet restriction to balance flow rate
  through the cell

**Why selected:** Maximum flexibility, correct thread sizing for Atlas Scientific
probes without adapters, larger water volume for better readings, and
significantly lower cost. The ability to add sensors without redesigning the
cell is important for future expansion.

---

## Option 4 — Dedicated Sample Pump (Considered, Rejected)

A small peristaltic or centrifugal pump draws water from a post-filter tap,
circulates it through a sensor manifold, and returns it independently of the
main pool pump.

**Pros**
- Completely independent of main pump — sensors work even during pump off cycles
- Very controlled, low flow rate ideal for electrochemical sensors
- Can be pulsed periodically rather than running continuously

**Cons**
- Adds a second pump to install, power, and maintain
- Significantly more complex plumbing and wiring
- Additional failure point — if the sample pump fails, no chemistry readings

**Why rejected:** The EZO-PMP dosing pump in this project is reserved for acid
dosing, not water sampling. Adding a second pump purely for sample circulation
adds unnecessary complexity for a residential pool installation where the main
pump runtime already provides adequate bypass flow.

---

## Vertical vs Horizontal Orientation

### Why Vertical (Bottom Inlet, Top Outlet) Was Chosen

**Air purging — self-purging by geometry**

Water entering from the bottom and exiting from the top pushes air upward and
out through the outlet on every pump start. There are no dead-end sections or
high points where air can accumulate. A purge valve is unnecessary — the
orientation handles it naturally. This is the same principle used to bleed
radiators and any vertical fluid system.

**Cell stays flooded between pump cycles**

With the outlet at the top, the cell retains water when the pump stops. There
is no siphon effect to drain the cell. Sensors remain submerged and are ready
to read immediately on the next pump start with no warm-up or purge delay.

**Sensor submersion is guaranteed**

With sensors mounted in the horizontal T branch ports and the outlet at the top,
all sensor tips are permanently below the water level in a full cell. A partial
airlock would have to fill more than half the cell before reaching any sensor.

**Flow switch reliability**

The flow switch at the top of the manifold only triggers when the cell is
completely flooded and water is actively flowing out. This confirms both full
submersion of all sensors AND active circulation — the ideal safety interlock
before trusting sensor readings for dosing decisions.

**Outlet tube routing**

Since the cell is mounted a few feet above the main pool equipment, the outlet
tube runs naturally downhill to the pre-pump return point with no high points
or U-loops. Elbows on both inlet and outlet allow horizontal valve orientation,
eliminating the upside-down U problem that would otherwise create an airlock in
the outlet line.

### Why Horizontal Was Rejected

A horizontal cell requires careful attention to sensor tip angles to avoid air
pockets collecting around probe tips. The cell can drain partially when the pump
stops if there is any slope, and purging requires deliberate design rather than
being handled by gravity. Atlas Scientific notes horizontal mounting as
acceptable but not preferred.

---

## Final Cell Design

```
  ┌ ← 3/4" elbow → horizontal ball valve → 3/8" outlet tube (slopes down to pre-pump return)
  │
  ├──── Union Coupling        (disconnect for maintenance without cutting pipe)
  │
  ├──── T  [Flow Switch]      (triggers only when cell fully flooded — safety interlock)
  │
  ├──── T  [ORP Probe]        (downstream of pH per Atlas Scientific recommendation)
  │
  ├──── T  [pH Probe]         (upstream of ORP per Atlas Scientific recommendation)
  │
  ├──── T  [PT-1000 Temp]     (first to submerge on fill, provides temp compensation data)
  │
  ├──── Union Coupling        (disconnect for maintenance without cutting pipe)
  │
  └ ← 3/4" elbow → horizontal ball valve → 3/8" inlet tube (from post-filter tap)
```

**Pipe:** Clear schedule 40 3/4" PVC  
**Sensor threads:** 3/4" NPT tapped directly into T branch (no adapters)  
**PT-1000 thread:** 3/4" NPT (fits same T fitting as pH and ORP)  
**Flow switch thread:** sized to match selected paddle switch  
**Bypass tubing:** 3/8" quick-connect flexible tubing  
**Flow driving force:** pressure differential between post-filter and pre-pump  
**Flow control:** inlet ball valve (restricts flow, cannot increase beyond 3/8" limit)  
**Sensors:** Atlas Scientific EZO-pH, EZO-ORP, EZO-RTD (PT-1000) with isolated carrier boards  
**Grounding:** Atlas Scientific EZO isolated carrier boards on pH and ORP circuits  

---

## Sensor Ordering Rationale

| Position | Sensor | Reason |
|---|---|---|
| Bottom | PT-1000 Temperature | First to submerge, provides temp data for pH compensation, least sensitive to flow characteristics |
| Second | pH | Must be upstream of ORP per Atlas Scientific protocol documentation |
| Third | ORP | Downstream of pH to avoid chemical carryover from pH membrane affecting reading |
| Top | Flow Switch | Confirms cell fully flooded AND active flow before any sensor reading is trusted |

---

*Notes captured from AquachemD project design — March 2026*
