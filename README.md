# Pasko sa Piyu — FEU Manila Grandstand 3D Concert & Light Show

**Course:** CS0045 — Computer Graphics and Visual Computing (Final Project)  
**Tech Stack:** C++, OpenGL, FreeGLUT, GLEW

An interactive 3D OpenGL concert simulation set at the **FEU Manila Grandstand & Athletic Field**, featuring a multi-zone intelligent lighting rig, animated performers and crowd, atmospheric haze machines, aerial fireworks, custom VBO centerpiece geometry, procedural/BMP texture mapping, and a real-time 2D console HUD.

---

## Features & Scene Highlights

### 1. Environment & Festive Landmarks

- **FEU Manila Grandstand & Athletic Field:** Open grass field with white pitch markings, 10-tier covered FEU Green & Gold bleachers with structural columns, a 3D stroke-text `"FEU MANILA GRANDSTAND"` banner, flanking campus buildings with illuminated windows, and green/gold perimeter banners.
- **Giant Christmas Tree Behind Stage:** Five-tier evergreen tree mounted on an octagonal FEU Green & Gold planter base surrounded by wrapped gift boxes, spiral twinkling fairy lights, and a rotating 3D golden star topper.
- **Multi-Burst Aerial Fireworks (`[K]`):** Five synchronized firework shells launching and bursting into spherical starbursts with additive blending and gravity-affected spark trails.

### 2. Stage Architecture & LED Walls

- **Dome Tent Truss & Tensioned Canopy:** Five-ribbed arched truss structure with purlins and a semi-transparent canopy over an elevated stage deck with side access steps and front Mojo crowd barricades.
- **Animated Drum Riser (`[Up]` / `[Down]`):** Center elevated riser with golden chevron emblem crest and illuminated side steps that dynamically raises and lowers.
- **Triple LED Video Walls:**
  - **Center `9x24` Main Backdrop Wall:** Displays external `backdrop.bmp` or falls back to a procedurally generated FEU Green & Gold LED equalizer grid.
  - **Left & Right `9x12` Rising Live-Feed IMAG Walls:** Truss-mounted side screens displaying external `livefeed.bmp` or a procedural live stage camera feed with spotlight beams, performer silhouette, scanlines, and a blinking `LIVE` tally indicator.
- **Stage Wings & Totems:** Geometric wing scrims, apron chevrons, and four upstage vertical LED light totems.

### 3. Multi-Zone Lighting Rig (`GL_LIGHT0` – `GL_LIGHT7`)

- **17 Articulated Moving-Head Spotlights:** Pan-and-tilt yokes with volumetric light cones across three zones:
  - **Zone A (Mid-Truss):** Cyan (`[1]`), Magenta (`[2]`), and Amber (`[3]`) sweeping spotlights.
  - **Zone B (Upstage Arch Truss):** 4 FEU Green & Gold aerial beam fixtures (`[4]`).
  - **Zone C (Stage Floor):** 8 upright floor-mounted moving-head beam fixtures (`[5]`).
- **Auxiliary Stage Lighting:**
  - **House / Field Light (`[0]`):** Ambient illumination for the grandstand and field (`GL_LIGHT0`).
  - **High-Speed Strobes (`[F]`):** 4 truss-mounted white strobe units (`GL_LIGHT4`).
  - **RGB LED PAR Wash (`[P]`):** 8 color-cycling PAR cans (`GL_LIGHT5`).
  - **Stormy Blinders (`[T]`):** 4 quad-cell lightning/audience blinder units (`GL_LIGHT6`).
  - **LED Pixel Bars (`[E]`):** 8 multi-segment chaser pixel bars along the stage apron and truss (`GL_LIGHT7`).
- **Show Modes (`[L]`):** Cycles between **SWEEP** (smooth automated motion), **PARTY** (synchronized chase blinking), and **FREEZE** (manual hold).

### 4. Performers, Audio Rig, VBO Centerpiece & Crowd

- **Animated 5-Piece Band (`[B]`):** Lead vocalist at center mic, drummer with animated sticks on a throne stool, lead guitarist, bassist, and keyboardist—all synced to the beat alongside oscillating cymbals and bass-reactive speakers.
- **Sound System & Haze:** Dual flown 8-box line arrays, 4 ground-stacked dual-driver subwoofers, and Stage Left/Right road-case haze machines (`[M]`) emitting volumetric smoke plumes toward center stage.
- **Custom VBO Centerpiece:** Rotating 16-facet textured 3D concert crystal suspended beneath the dome apex using OpenGL Vertex Buffer Objects (`glGenBuffers`, `glBindBuffer`, `glBufferData`).
- **Interactive Concert Audience:** 154 animated spectators (7 rows × 22 columns) jumping, swaying, and waving glowing LED wristbands and concert light sticks.

### 5. 2D Orthographic Console HUD & Equalizer

- **Toggleable Control Overlay (`[H]`):** Displays active show mode, fixture states, and camera shortcuts.
- **Real-Time Audio Equalizer:** 14-band animated 2D equalizer bar display in the bottom-right corner.

---

## Controls

### Camera Navigation

| Input                  | Action                                                |
| :--------------------- | :---------------------------------------------------- |
| **Left-Click + Drag**  | Orbit camera (Yaw & Pitch)                            |
| **Right-Click + Drag** | Zoom in / out                                         |
| **Mouse Wheel**        | Zoom in / out                                         |
| **`W` / `S`**          | Zoom in / out                                         |
| **`A` / `D`**          | Pan camera left / right                               |
| **`R`**                | Reset camera view, riser height, and all show toggles |

### Lighting, Effects & Stage Controls

| Key                      | Action                                                  |
| :----------------------- | :------------------------------------------------------ |
| **`L`**                  | Cycle Light Show Mode (`SWEEP` → `PARTY` → `FREEZE`)    |
| **`0`**                  | Toggle Field / Grandstand House Light (`GL_LIGHT0`)     |
| **`1`**                  | Toggle Outer Cyan Moving-Head Spots (`GL_LIGHT1`)       |
| **`2`**                  | Toggle Inner Magenta Moving-Head Spots (`GL_LIGHT2`)    |
| **`3`**                  | Toggle Center Amber Moving-Head Spot (`GL_LIGHT3`)      |
| **`4`**                  | Toggle Upstage Arch FEU Green/Gold Beams                |
| **`5`**                  | Toggle 8 Stage-Floor Moving-Head Beams                  |
| **`F`**                  | Toggle High-Speed Strobe Lights (`GL_LIGHT4`)           |
| **`P`**                  | Toggle RGB LED PAR Wash Cans (`GL_LIGHT5`)              |
| **`E`**                  | Toggle Multi-Segment LED Pixel Bars (`GL_LIGHT7`)       |
| **`T`**                  | Toggle Stormy Blinders / Lightning Bursts (`GL_LIGHT6`) |
| **`B`**                  | Toggle Band, Crowd & Bass Equalizer Animation           |
| **`M`**                  | Toggle Left & Right Stage Haze Machines                 |
| **`K`**                  | Toggle Aerial Fireworks Show                            |
| **`Up` / `Down` Arrows** | Raise / Lower Center Drum Riser                         |
| **`H`**                  | Show / Hide 2D Console HUD                              |
| **`Esc`**                | Exit application                                        |

---

## Project Structure

```text
├── try.cpp          # Main C++ OpenGL/FreeGLUT/GLEW source code
├── backdrop.bmp     # (Optional) 24-bit BMP texture for the center 9x24 LED wall
├── livefeed.bmp     # (Optional) 24-bit BMP texture for the side 9x12 IMAG screens
└── README.md        # Project documentation
```
