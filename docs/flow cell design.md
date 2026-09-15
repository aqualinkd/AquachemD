
<p align="center">
  <img src="../web/aquachemd.png" width="120" alt="AquachemD logo">
</p>

<h1 align="center">AquachemD</h1>
<p align="center"><b>Open-source, automated pool water chemistry — pH, ORP, and dosing, done right.</b></p>
<hr><br><br>

# AquachemD — Flow Cell Design Notes
  

## Overview

Four options were evaluated for mounting pH, ORP, sensors and PT-1000
temperature sensors to read pool water chemistry. This document summarises each
option.
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

## Option 2 — Commercial Flow Cell (IPS FC100G, Hayward CAX-20272, Jandy Drudose flow cell)

A purpose-built acrylic bypass chamber with pre-drilled sensor ports, plumbed
as a sidestream off the main line.

**Pros**
- Sensors removable for calibration without disturbing main flow or shutting
  down the pump
- Controlled low-velocity flow gives more stable sensor readings
- Pre-positioned sensor ports with correct geometry
- Clear acrylic provides visual confirmation of water flow
- Integrated float switch port on most models
- Most major manufactures use this flow cell under different names / part#
- Pre-drilled ports and gland nuts are sized and positioned for IPS's own sensor ecosystem / sensors —
  which happens identical to Atlas Scientific and many other manufacturer probes.


**Cons**
- Cost: $80–$150 depending on model
- The IPS FC100G has only two usable top ports (pH and ORP), with the third
  occupied by the integrated flow/float switch — no port available for a
  temperature sensor without modification
- The bottom drain/sample port is a poor candidate for a temperature sensor as
  it faces downward and partially exposes the sensor to air when the cell drains
- Less flexibility in orientation — commercial cells are designed for a specific
  mounting position


---

## Option 3 — Inline "FLow Cell" (eg Jandy TruSense cell-only / Poolside Tech ATT-FLOW-CELL)

A compact, purpose-built chamber plumbed directly into the main 2" return line (not a bypass — full system flow passes through it), with pre-sized NPT ports for pH/ORP probes and, on some models, an integrated acid-injection port in the same housing.

**Pros**
- Readings always reflect the water actually reaching the pool, with no bypass restrictor that could clog or throttle down and starve the sensors of flow
- Simpler plumbing than a true bypass loop — one inline section with two unions, versus tapping two separate points into the main line at different pressures
- The chamber's internal geometry is still wider than the pipe it interrupts, so it meaningfully reduces velocity and turbulence around the probe tips compared to threading sensors straight into standard pipe (Option 1) — just not as dramatically as a low-flow bypass cell
- Purpose-sized ports mean clean installation without adapters, similar convenience to a commercial bypass cell
- Some models combine the sensing chamber and the acid-dosing injection port in one component, reducing total fittings

**Cons**
- Still carries the full return-line flow, so it does essentially nothing to keep debris away from the probes; suspended particulates in the pool's normal flow pass directly across the sensor tips, same exposure as Option 1
- Because it's genuinely in the main line, installation is more invasive than a bypass tap — it typically requires cutting a section of the return pipe and providing a straight run either side (Poolside specifies 12") rather than just tapping two ports into an existing pipe
- Servicing generally means interrupting flow through the whole return line, not just isolating a bypass loop — a bypass design with valves on both taps can often be serviced with the main pump still running; this can't
- Vendor lock-in similar to Option 2 — ports are sized for that manufacturer's own probes, not a generic threading standard
- Pricing is comparable to or higher than a commercial bypass cell — the Poolside unit alone runs around $335, above the $80–150 range quoted for Option 2, before probes




---

## Option 4 — DIY Flow Cell with Clear 3/4" PVC

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

---

---

## DIY options Vertical vs Horizontal Orientation

### Why Vertical (Bottom Inlet, Top Outlet)

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
