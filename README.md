# iFL03 — modern iRacing overlays with a CEF settings UI <!-- omit in toc -->

iFL03 is a lightweight, modern overlay suite for iRacing with a built‑in Chromium (CEF) UI. It brings a polished visual style, sensible defaults, and powerful customization while staying intuitive for the user.

This is a continuation of L. E. Spalt’s excellent iRon project. Many of the core ideas, rendering scaffolding, and the overall approach to simple, dependency‑light overlays originate there. The CEF UI and new overlays are built on that foundation.

Acknowledgement: Huge thanks to lespalt for the original work. iFL03’s CEF GUI and overlays are built upon that base. See Credits & License below.

![Overview](assets/readmeimg/index.png)

# Contents <!-- omit in toc -->

- [Where to Download](#where-to-download)
- [What’s New in iFL03](#whats-new-in-ifl03)
- [Overlays](#overlays)
  - [Relative](#relative)
  - [Standings](#standings)
  - [DDU (Dashboard)](#ddu-dashboard)
  - [Inputs](#inputs)
  - [Delta](#delta)
  - [Flags](#flags)
  - [Weather](#weather)
  - [Track Map](#track-map)
  - [Radar](#radar)
  - [Cover](#cover)
- [Editing, Layout and Configuration](#editing-layout-and-configuration)
- [CEF UI](#cef-ui)
- [Installing & Running](#installing--running)
- [Building from source](#building-from-source)
- [Dependencies](#dependencies)
- [Bug reports and feature requests](#bug-reports-and-feature-requests)
- [Credits & License](#credits--license)

---

## Where to Download

- Grab binaries from this repository’s Releases page.
- Or build from source (see below).

---

## What’s New in iFL03

Alongside the original overlays, iFL03 adds several new overlays and many quality‑of‑life upgrades:

- New overlays
  - Delta: Circular delta with trend‑aware coloring and predicted lap time.
  - Flags: Clean, high‑contrast flag callouts with two‑band design.
  - Weather: Track temp, wetness bar, precipitation/air temp, and a wind compass relative to car.
  - Track Map: Scaled track rendering with start/finish markers and cars for most tracks.
  - Radar: Proximity radar with 8 m/2 m guides and sticky alerts.
- Enhanced original overlays
  - Relative: Optional minimap, license or SR, iRating, pit age, last lap, average of last 5 laps, positions gained, class colors, buddy highlighting, scrollable list, optional iRating prediction in races.
  - Standings: Class‑aware grid with fastest‑lap highlighting, car brand icons, deltas/gaps, average of last 5 laps, configurable top/ahead/behind visibility, scroll bar.
  - DDU: Fuel calculator refinements, P1 last, delta vs session best, shift light behavior, temperatures, brake bias, incident count, RPM lights, etc.
  - Inputs: Dual graph plus vertical bars (clutch/brake/throttle), steering ring or image wheel (Moza KS / RS v2), on‑wheel speed/gear.
- Preview mode: Populate overlays with stub data even when disconnected to place layouts.
- Global opacity: All overlays respect a global opacity for easy blending with broadcasts/streams.

---

## Overlays

![Overlays Overview](assets/readmeimg/overlays.png)

### Relative

Competitor list centered on your car with rich context:
- Position, car number, driver name
- License or SR, iRating (k notation)
- Pit age (laps since last stop) with PIT indicator when on pit road
- Last lap, delta to you, and average of last 5 laps (L5) color‑coded vs your L5
- Positions gained/lost (+/−) with color
- Buddy and flagged highlighting, optional class colors; pace car handling
- Optional minimap (relative or absolute)
- Scroll the list with the mouse wheel when there are many cars

![relative](assets/readmeimg/relative.png)

### Standings

Full‑field view with class awareness and a compact, readable grid:
- Position, car number, driver/team name
- Pit age with live PIT indicator
- License/SR and iRating
- Car brand icon per entry (loaded from assets) when available
- Gap (time or laps), last lap, best lap (highlight class fastest), delta to you
- Average of last 5 laps (L5) color‑coded vs your own L5
- Configurable number of “top”, “ahead” and “behind” rows and auto scroll bar
- SoF, track temp, session end and laps summary footer

![standings](assets/readmeimg/standings.png)

### DDU (Dashboard)

Compact dashboard with everything you’d otherwise flip through in black boxes:
- Gear and speed, RPM shift lights and rev limiter alert
- Current lap and laps remaining (or time‑to‑go)
- Position and lap delta to leader
- Best, last, and P1’s last lap
- Delta vs session best (green/red bar)
- Session clock, incident count, brake bias
- Oil and water temperatures (C/F with warnings)
- Fuel module with: remaining fuel, per‑lap average, estimated laps left, fuel to finish, scheduled add, progress bar; auto‑filters laps under cautions or pit to keep averages clean; safety factor configurable
- Tire wear (LF/RF/LR/RR) and service indicators

![ddu](assets/readmeimg/ddu.png)

### Inputs

Pedal traces and steering visualization for driving consistency work:
- Scrolling throttle and brake traces (configurable thickness/colors)
- Vertical percentage bars for clutch, brake, throttle with numeric readouts
- Steering indicator: ring + rotating column or an image wheel (Moza KS / RS v2)
- On‑wheel speed and gear when using the built‑in ring

![inputs](assets/readmeimg/inputs.png)

### Delta

Circular delta with trend‑aware coloring and prediction:
- Reference modes: all‑time best, session best, all‑time optimal, session optimal, and a fallback to last lap
- Only shows once you’re on track, out of the pits, and past initial sector; hides otherwise
- Outer progress ring scales with delta magnitude (up to ±2.00s)
- Side panel shows reference lap time and predicted current lap time (reference + delta)
- Auto scales gracefully with overlay size

![delta](assets/readmeimg/delta.png)

### Flags

High‑contrast two‑band banner for session and race control flags:
- Top band on a dark background with the flag’s color; bottom band uses the flag color as background
- Handles iRacing’s full set of flags (black/penalty, meatball/repair, red, green, yellow, white, blue, debris, crossed, start‑ready/set/go, caution variants, one/five/ten‑to‑go, disqualify, etc.)
- Preview mode lets you force a specific flag for stream layout testing

![flags](assets/readmeimg/flags.png)

### Weather

Vertical weather pillar with clear typography and iconography:
- Track temperature with units (C/F)
- Track wetness bar (sun ←→ water) based on iRacing’s 0–7 wetness enum
- Precipitation percentage when relevant, otherwise shows air temperature
- Wind compass: car fixed in the center; arrow shows wind flow over the car (wind minus car yaw)
- Updates at a sensible cadence (weather changes are gradual)

![weather](assets/readmeimg/weather.png)

### Track Map

Scaled track rendering with start/finish markers and cars:
- Loads normalized points from assets/tracks/track‑paths.json (by trackId)
- Auto scale/center to overlay; configurable stroke widths and colors
- Draws start/finish (and extended) lines perpendicular to the path tangent
- Self marker highlighted with number; optional opponents with class colors; pace/safety car handling
- Auto‑detect start offset when crossing S/F; manual start offset and reverse direction supported

![track map](assets/readmeimg/trackmap.png)

### Radar

Racelab‑style proximity radar with readable distance cues:
- Circular background (optional)
- Guide lines at 8 m front/back and 2 m left/right near the car
- Yellow zones (8–2 m) and red zones (≤2 m) for front/back with subtle radial fades
- Red side zones for close lateral proximity, biased by the opponent’s along‑track location
- Sticky timers to avoid flicker as cars move in/out of thresholds

![radar](assets/readmeimg/radar.png)

### Cover

Plain rectangle that can hide distracting in‑car dashes (e.g., next‑gen stock car). Useful for broadcasts and focused driving.

Screenshot: (not applicable)

---

## Editing, Layout and Configuration using the Console

- Preview Mode: Press ALT‑J to toggle. Drag overlays to move; drag bottom‑right corner to resize. Press ALT‑J again to exit.
- Hotkeys: Overlays can be toggled at runtime (see console on startup). Hotkeys are configurable.
- Live config: Most colors, fonts, sizes and behavior can be tuned in the generated config.json. Edits are applied live when you save.
- Preview mode: Enable to populate overlays with stub data when disconnected, for layout/design.
- Global opacity: Each overlay’s colors are automatically multiplied by a global opacity setting.

---

## CEF Settings UI

iFL03 ships with an embedded Chromium UI for a friendlier setup experience. Use it to:
- Toggle overlays on/off and adjust per‑overlay options
- Change fonts, sizes, spacing and colors
- Preview and fine‑tune positions and global opacity

If you prefer, all settings remain available via config.json without opening the UI.

![Preview Mode](assets/readmeimg/previewmode.png)

![Settings UI](assets/readmeimg/settings.png)

![User Settings](assets/readmeimg/usersettings.png)

---

## Installing & Running

- No installer. Place the executable in any writable folder (so iFL03 can save config.json).
- Run iFL03 before or after launching iRacing; overlays will appear once you’re in the car.
- Borderless‑window mode in iRacing is recommended. Other modes may work but are less tested.

---

## Building from source

- Toolchain: Visual Studio 2022 (Community is fine). The solution should open and build as‑is.
- You may need the standard DirectX SDK components that ship with Visual Studio.

---

## Dependencies

- Runtime: Only standard Windows components (Direct3D/Direct2D, DirectWrite, etc.).
- Source: iRacing SDK, picojson, and minimal helper code included in this repository.

---

## Bug reports and feature requests

Please open an issue in this repository. Repro steps and logs/screenshots help a lot. PRs welcome.

---

## Credits & License

- iFL03 is based on iRon by L. E. Spalt (lespalt). The rendering architecture and many overlay ideas come from that project. The CEF GUI builds on that foundation. Thank you!
- License: MIT (see LICENSE). Please retain copyright notices in source files.


## Donations

If you like this project enough to wonder whether you can contribute financially: first of all, thank you! Make sure to also checkout lesphalt's repo/github to show some appreciation. you can support me (only if you really want to) using this [link](https://paypal.me/sems0drmans). If not **please 
consider giving to any charities of your choosing instead**.
