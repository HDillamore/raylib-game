# Asteroids Survival

A fast-paced, 2D arcade survival game developed in C using [raylib](https://www.raylib.com/), compiled to WebAssembly via Emscripten for web deployment.

The player controls a ship using momentum based physics to avoid homing enemies, manuvering so that enemies crash into each other, awarding points to the player. The aim of the game is to survive and gain as many points as possible by killing eneimes and having close calls.

---

## Game Features

* **Physics & Controls:** Inertia-based thrust, rotation, damping, and screen-boundary bounce physics.
* **Enemy AI:** Enemies spawn around screen edges and track the player using steering behaviors.
* **Scoring Mechanics:**
* **+200 pts:** Awarded when enemies crash into one another.
* **+50 pts:** Awarded for near-miss dodges when an enemy comes close without hitting the player.
* Floating score numbers render dynamically at the exact point of the event.


* **Game Loop & States:**
* **Start Screen:** Text input field for player callsign/name entry.
* **Gameplay:** Real-time steering, collisions, and particle effects.
* **Game Over:** Collision with any enemy triggers player death, high-impact screen shake, automatic score submission, and a live top-scorers leaderboard pull.



---

## Technical Stack & Architecture

* **Language:** C (C99 standard).
* **Game Engine / Library:** [raylib](https://www.raylib.com/) (built with `PLATFORM_WEB` / OpenGL ES 2.0).
* **Compilation Toolchain:** [Emscripten SDK (`emsdk`)](https://emscripten.org/) targeting WebAssembly (`wasm32-unknown-emscripten`).
* **Visuals:** Pure procedural geometry rendering (triangles, circles, dynamic lines) without external image or texture assets.
* **Backend Integration:**
* Cloudflare Worker running an asynchronous REST API.
* Cloudflare KV storage maintaining top 10 global leaderboard entries.
* Client-to-server bridge uses Emscripten's JavaScript interop (`emscripten_run_script`, `_malloc`, `stringToUTF8`, and `EMSCRIPTEN_KEEPALIVE` exported C functions) to dispatch browser `fetch()` requests asynchronously without blocking the game thread.



---

## Prerequisites

1. **Python 3.x:** Needed for Emscripten tooling and local hosting. Ensure Python is added to your system `PATH` (disable Windows App Execution Aliases if `python` redirects to the Microsoft Store).
2. **Emscripten SDK (`emsdk`):** Installed and activated.
3. **Web-ready Raylib Static Library (`libraylib.a`):** Built from source targeting `PLATFORM_WEB`.

---

## Build Instructions (Emscripten / `emcc`)

### 1. Build `libraylib.a` for Web (One-time setup)

Navigate to your Raylib source directory (e.g., `raylib/src`) and compile the core library modules:

```cmd
emcc -c rcore.c rshapes.c rtextures.c rtext.c rmodels.c raudio.c -Os -Wall -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2
emar rcs libraylib.a rcore.o rshapes.o rtextures.o rtext.o rmodels.o raudio.o

```

### 2. Compile the Game

Open your command prompt, initialize the Emscripten environment, and run the compiler:

```cmd
:: Activate emsdk environment (run once per terminal session)
C:\path\to\emsdk\emsdk_env.bat

:: Navigate to your build directory containing libraylib.a and minshell.html
cd D:\RayLib\raylib\src

:: Compile game to WebAssembly
emcc -o index.html "C:\path\to\your\project\main.c" ^
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

#### Build Output:

* `index.html`: Web harness and Canvas host.
* `index.js`: JavaScript bridge and WebAssembly loader.
* `index.wasm`: Compiled game binary.

---

## Local Testing

Web browsers restrict WebAssembly execution over local file paths (`file://`) due to CORS policies. Run a local HTTP server:

```cmd
python -m http.server 8000

```

1. Open your browser to `http://localhost:8000/index.html`.
2. Use **Ctrl + F5** (or **Ctrl + Shift + R**) to hard refresh and bypass browser caching when updating builds.
3. Press **F12** to view the developer console for network responses (`Score saved:`).
4. Terminate the server anytime with **Ctrl + C**.

---

## Online Leaderboard API

The game connects to a serverless backend hosted on Cloudflare Workers.

* **API Endpoint:** `[https://asteroids-leaderboard.2402521.workers.dev](https://asteroids-leaderboard.2402521.workers.dev)`
* **Transport:** Asynchronous JavaScript `fetch()` calls executed via Emscripten.

### API Routes

#### `POST /` (Submit Score)

Submits a player score on death. The worker stores the entry in KV, sorts all scores in descending order, trims the array to the top 10, and returns the updated leaderboard.

* **Headers:** `Content-Type: application/json`
* **Request Payload:**
```json
{
  "player": "AcePilot",
  "score": 1450
}

```


* **Success Response (200 OK):**
```json
{
  "success": true,
  "scores": [
    { "player": "AcePilot", "score": 1450, "date": 1713800000000 }
  ]
}

```



#### `GET /` (Fetch Top Scores)

Pulls the current top 10 ranking entries during game initialization and post-death display.

* **Success Response (200 OK):**
```json
[
  { "player": "AcePilot", "score": 1450, "date": 1713800000000 }
]

```



---

## Deployment to itch.io

1. Select `index.html`, `index.js`, and `index.wasm`.
2. Add them directly into the root of a ZIP file (e.g., `game.zip`). Do not place them inside a folder before zipping.
3. Log in to [itch.io](https://itch.io/) and create a new project.
4. Set **Kind of project** to **HTML (You have a ZIP or HTML file that will be played in the browser)**.
5. Under **Uploads**, upload `game.zip` and tick **This file will be played in the browser**.
6. Set **Embed options** viewport to **900 px width** and **900 px height**.
7. Tick **Automatically start on page load**.
8. Save and publish.