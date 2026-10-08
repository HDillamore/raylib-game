# Asteroids Survival


I developed Asteroids Survival as a miniature browser game, built in C using Raylib, and compiled to WebAssembly via Emsciptem, allowing the project to run in the browser with no plugins. In the game the player controls a ship using momentum based movement, avoiding incoming enemies which home onto their location. To kill the enemies, the player must dodge them and get them to crash into each other.

---

## Gameplay & Features

* **Controls & Handling:** The player has control of thrust and rotation using the W, A, and D keys, which is used to apply acceleration and velocity.
* **Enemy AI:** Enemies spawn along the edges of the screen periodically and are given slightly random speed and acceleration values, then they path-find directly towards the player. Killing the player on impact if they reach them.
* **Scoring Breakdown:**
  * **Enemy Collision:** If two enemies collide, they are both removed and the player is awarded 200 points.
  * **Near Miss:** If the player goes close enough to an enemy and survives, they are awarded 50 points.
* **Visual Feedback**: When points are awarded, they appear briefly on the screen with particle effects and screen shake when enemies collide.

---

## Technical Stack & Architecture

> **Note:** Outline the tools and explain the technical bridge between native C and browser APIs.

* **Language & Engine:** C (C99) using [raylib](https://www.raylib.com/) (`PLATFORM_WEB` / OpenGL ES 2.0).
* **Target:** WebAssembly (`wasm32-unknown-emscripten`) compiled with the Emscripten SDK.
* **Rendering:** Pure procedural vector geometry (triangles, circles, lines) rendered dynamically without external sprite sheets or textures.
* **Web Request Architecture:**
* **Backend Service:** [Specify your backend, e.g., serverless Cloudflare Worker with KV storage managing a global top-10 leaderboard].
* **C-to-Browser Bridge:** [Explain how you dispatch web requests without blocking the game thread: e.g., using Emscripten JS interop like `emscripten_run_script`, `_malloc`, `stringToUTF8`, and `EMSCRIPTEN_KEEPALIVE` exported functions to trigger asynchronous browser `fetch()` calls].



---

## Prerequisites

> **Note:** List every tool needed to build and run the game locally.

1. **Python 3.x:** Required for Emscripten utilities and local development hosting. (Ensure Python is on your system `PATH`).
2. **Emscripten SDK (`emsdk`):** Installed and ready to activate.
3. **Raylib Static Web Library (`libraylib.a`):** Compiled from source targeting `PLATFORM_WEB`.

---

## Compiling with Emscripten (`emcc`)

> **Note:** Provide exact, repeatable commands. Document the one-time library build, the full game compilation command, and the meaning of your critical compiler flags.

### 1. Build the Raylib Web Library (One-Time Setup)

Navigate to the Raylib source directory (e.g., `raylib/src`) and compile the static library for the web target:

```cmd
emcc -c rcore.c rshapes.c rtextures.c rtext.c rmodels.c raudio.c -Os -Wall -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2
emar rcs libraylib.a rcore.o rshapes.o rtextures.o rtext.o rmodels.o raudio.o

```

### 2. Compile the Game

Activate the Emscripten environment, navigate to your build directory, and compile the C source into WebAssembly:

```cmd
:: 1. Activate Emscripten environment
C:\path\to\emsdk\emsdk_env.bat

:: 2. Move to the directory containing libraylib.a and your shell HTML file
cd path\to\build_folder

:: 3. Compile source code to WebAssembly
emcc -o index.html "path\to\your\main.c" ^
    -Os -Wall ^
    -DPLATFORM_WEB ^
    -I. libraylib.a ^
    -s USE_GLFW=3 ^
    -s ASYNCIFY ^
    -s TOTAL_MEMORY=67108864 ^
    -s ALLOW_MEMORY_GROWTH=1 ^
    -s "EXPORTED_FUNCTIONS=['_main','_ClearLeaderboard','_AddLeaderboardEntry','_malloc','_free']" ^
    -s "EXPORTED_RUNTIME_METHODS=['ccall','cwrap','stringToUTF8','lengthBytesUTF8']" ^
    --shell-file minshell.html

```

> **Note on Compiler Flags:**
> * `-DPLATFORM_WEB`: Configures Raylib to use browser canvas contexts and event hooks.
> * `-s ASYNCIFY`: Allows C code to wait on asynchronous JS operations without stalling the browser loop.
> * `-s EXPORTED_FUNCTIONS` & `-s EXPORTED_RUNTIME_METHODS`: Exposes C entry points and memory helpers so your JavaScript bridge can pass network data back into C.
> * `--shell-file`: Uses a custom minimal HTML template hosting the canvas element.
> 
> 

#### Build Output:

* `index.html`: Web harness and Canvas container.
* `index.js`: JavaScript bridge, loader, and runtime helpers.
* `index.wasm`: Compiled WebAssembly game binary.

---

## Local Development: Serving & Playing in a Browser

> **Note:** Browsers block WebAssembly from running directly off the filesystem (`file://`) due to CORS security restrictions. Running a local HTTP server is required.

Run the built-in Python HTTP server from the folder containing your compiled build files:

```bash
# Start local development server on port 8000
python3 -m http.server 8000

```

*(On Windows systems using standard Python aliases, you can run `python -m http.server 8000`.)*

### How to Test:

1. Open your browser and navigate to: `http://localhost:8000/index.html`
2. **Clear Cache:** Use **Ctrl + F5** (or **Cmd + Shift + R** on macOS) after compiling new builds so your browser does not serve an older cached `.wasm` binary.
3. **Inspect Output:** Press **F12** to open the browser Developer Tools. Monitor the **Console** and **Network** tabs to verify asset loading and watch network log entries (e.g., checking that score submissions complete successfully).
4. **Stop Server:** Terminate the local server anytime in your terminal with **Ctrl + C**.

---

## Web Request Source & Leaderboard API

> **Note:** Clearly document the backend architecture, the API host, transport mechanism, and payload schemas used for network communication.

* **API Base URL:** `https://[your-subdomain].[worker-or-server].dev`
* **Backend Platform:** [e.g., Cloudflare Workers + Cloudflare KV Storage]
* **Transport Mechanism:** Asynchronous browser `fetch()` requests invoked through Emscripten's JavaScript interop layer.

### Endpoints

#### 1. Submit Player Score (`POST /`)

Dispatched on game over. The backend receives the payload, saves the entry to persistent storage, sorts all entries descending, trims the set to the top 10, and returns the updated leaderboard.

* **Request Headers:** `Content-Type: application/json`
* **Request Payload:**

```json
{
  "player": "Callsign",
  "score": 1450
}

```

* **Success Response (`200 OK`):**

```json
{
  "success": true,
  "scores": [
    {
      "player": "Callsign",
      "score": 1450,
      "date": 1713800000000
    }
  ]
}

```

#### 2. Fetch Leaderboard (`GET /`)

Called during game startup and immediately following player death to populate the in-game high-score overlay.

* **Success Response (`200 OK`):**

```json
[
  {
    "player": "Callsign",
    "score": 1450,
    "date": 1713800000000
  }
]

```

---

## Publishing to itch.io

> **Note:** Step-by-step checklist for packaging the WebAssembly output into a ready-to-deploy zip archive.

1. Locate your three build artifacts: `index.html`, `index.js`, and `index.wasm`.
2. Select all three files and zip them directly into the root of an archive (e.g., `game.zip`). **Do not** put them inside a parent folder before zipping.
3. Open [itch.io](https://itch.io/) and create a new project.
4. Set **Kind of project** to **HTML (You have a ZIP or HTML file that will be played in the browser)**.
5. Upload `game.zip` under the **Uploads** section and check **This file will be played in the browser**.
6. Set the embedded viewport dimensions to **900 px width** by **900 px height** (or match your canvas resolution).
7. Enable **Automatically start on page load**.
8. Save and publish.