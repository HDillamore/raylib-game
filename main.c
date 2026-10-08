#include "raylib.h"
#include "raymath.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

#define MAX_ENEMIES 100
#define MAX_PARTICLES 128
#define MAX_FLOATING_SCORES 64
#define MAX_NAME_LENGTH 12
#define MAX_LEADERBOARD_ENTRIES 5

typedef enum GameState {
    STATE_START = 0,
    STATE_GAMEPLAY,
    STATE_GAMEOVER
} GameState;

typedef struct LeaderboardEntry {
    char name[16];
    int score;
} LeaderboardEntry;

typedef struct Enemy {
    Vector2 pos;
    Vector2 vel;
    Vector2 accel;
    float maxSpeed;
    float turningForce;
    bool active;
    bool nearMissAwarded;
} Enemy;

typedef struct Particle {
    Vector2 pos;
    float radius;
    float maxRadius;
    float alpha;
    bool active;
} Particle;

typedef struct FloatingScore {
    Vector2 pos;
    int value;
    float alpha;
    float lifetime;
    Color color;
    bool active;
} FloatingScore;

// Global state
const int screenWidth = 900;
const int screenHeight = 900;

GameState gameState = STATE_START;
char playerName[MAX_NAME_LENGTH + 1] = "\0";
int nameLetterCount = 0;

LeaderboardEntry topScores[MAX_LEADERBOARD_ENTRIES] = {0};
int topScoreCount = 0;
bool leaderboardLoaded = false;

Vector2 playerPos;
Vector2 playerVel = {0.0f, 0.0f};
Vector2 playerAccel = {0.0f, 0.0f};
float playerRotation = -PI / 2.0f;
float playerTurningForce = 0.25f;
float rotationSpeed = 0.08f;

Enemy enemies[MAX_ENEMIES] = {0};
int enemyCount = 0;
int frameCount = 0;
const int spawnInterval = 60;

const float enemyRadius = 8.0f;
const float collisionDistSqr = (8.0f * 2.0f) * (8.0f * 2.0f);
const float playerRadius = 15.0f;
const float bounceRestitution = 0.8f;
const float playerHitDistSqr = (15.0f + 8.0f) * (15.0f + 8.0f);
const float nearMissDistSqr = (15.0f + 8.0f + 22.0f) * (15.0f + 8.0f + 22.0f);

int score = 0;
float screenShake = 0.0f;
bool scoreSent = false;
Particle particles[MAX_PARTICLES] = {0};
FloatingScore floatingScores[MAX_FLOATING_SCORES] = {0};
Camera2D camera = {0};

#if defined(PLATFORM_WEB)
EMSCRIPTEN_KEEPALIVE
void ClearLeaderboard(void) {
    topScoreCount = 0;
    leaderboardLoaded = true;
}

EMSCRIPTEN_KEEPALIVE
void AddLeaderboardEntry(const char *name, int entryScore) {
    if (topScoreCount < MAX_LEADERBOARD_ENTRIES) {
        snprintf(topScores[topScoreCount].name, sizeof(topScores[topScoreCount].name), "%s", name);
        topScores[topScoreCount].score = entryScore;
        topScoreCount++;
        leaderboardLoaded = true;
    }
}
#endif

void FetchLeaderboard(void) {
#if defined(PLATFORM_WEB)
    leaderboardLoaded = false;
    topScoreCount = 0;
    const char *endpoint = "https://asteroids-leaderboard.2402521.workers.dev";

    char script[1024];
    snprintf(script, sizeof(script),
        "fetch('%s')"
        ".then(res => res.json())"
        ".then(data => {"
        "  _ClearLeaderboard();"
        "  const list = Array.isArray(data) ? data : (data.scores || []);"
        "  list.slice(0, 5).forEach(item => {"
        "    const nameStr = (item.player || 'Unknown').substring(0, 15);"
        "    const scoreVal = parseInt(item.score, 10) || 0;"
        "    const lengthBytes = lengthBytesUTF8(nameStr) + 1;"
        "    const namePtr = _malloc(lengthBytes);"
        "    stringToUTF8(nameStr, namePtr, lengthBytes);"
        "    _AddLeaderboardEntry(namePtr, scoreVal);"
        "    _free(namePtr);"
        "  });"
        "})"
        ".catch(err => console.error('Error fetching leaderboard:', err));",
        endpoint);

    emscripten_run_script(script);
#endif
}

void SendScoreWebhook(int finalScore, const char *name) {
    const char *finalName = (name && name[0] != '\0') ? name : "Guest";

#if defined(PLATFORM_WEB)
    const char *endpoint = "https://asteroids-leaderboard.2402521.workers.dev";
    
    char script[1024];
    snprintf(script, sizeof(script),
        "fetch('%s', {"
        "  method: 'POST',"
        "  headers: { 'Content-Type': 'application/json' },"
        "  body: JSON.stringify({ score: %d, player: '%s' })"
        "}).then(res => res.json())"
        "  .then(data => {"
        "    console.log('Score saved:', data);"
        "    _ClearLeaderboard();"
        "    const list = Array.isArray(data) ? data : (data.scores || []);"
        "    list.slice(0, 5).forEach(item => {"
        "      const nameStr = (item.player || 'Unknown').substring(0, 15);"
        "      const scoreVal = parseInt(item.score, 10) || 0;"
        "      const lengthBytes = lengthBytesUTF8(nameStr) + 1;"
        "      const namePtr = _malloc(lengthBytes);"
        "      stringToUTF8(nameStr, namePtr, lengthBytes);"
        "      _AddLeaderboardEntry(namePtr, scoreVal);"
        "      _free(namePtr);"
        "    });"
        "  })"
        "  .catch(err => console.error('Error:', err));",
        endpoint, finalScore, finalName);

    emscripten_run_script(script);
#else
    TraceLog(LOG_INFO, "DESKTOP RUN: Webhook would post: Player: %s, Score: %d", finalName, finalScore);
#endif
}

Enemy CreateEnemy(Vector2 pos, float maxSpeed, float turningForce) {
    Enemy e = {0};
    e.pos = pos;
    e.vel = (Vector2){0.0f, 0.0f};
    e.accel = (Vector2){0.0f, 0.0f};
    e.maxSpeed = (float)GetRandomValue(1.0f, maxSpeed);
    e.turningForce = (float)GetRandomValue(10, turningForce);
    e.turningForce = e.turningForce / 100.0f;
    e.active = true;
    e.nearMissAwarded = false;
    return e;
}

bool SpawnEnemy(Enemy enemies[], int *count, Vector2 pos, float maxSpeed, float turningForce) {
    if (*count >= MAX_ENEMIES) return false;
    enemies[*count] = CreateEnemy(pos, maxSpeed, turningForce);
    (*count)++;
    return true;
}

Vector2 GetRandomScreenEdgePos(int screenWidth, int screenHeight, float padding) {
    int edge = GetRandomValue(0, 3);
    switch (edge) {
        case 0: return (Vector2){ (float)GetRandomValue(0, screenWidth), -padding };
        case 1: return (Vector2){ (float)screenWidth + padding, (float)GetRandomValue(0, screenHeight) };
        case 2: return (Vector2){ (float)GetRandomValue(0, screenWidth), (float)screenHeight + padding };
        case 3:
        default: return (Vector2){ -padding, (float)GetRandomValue(0, screenHeight) };
    }
}

void SpawnPopEffect(Particle particles[], Vector2 pos) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].active) {
            particles[i].pos = pos;
            particles[i].radius = 4.0f;
            particles[i].maxRadius = 24.0f;
            particles[i].alpha = 1.0f;
            particles[i].active = true;
            break;
        }
    }
}

void SpawnFloatingScore(FloatingScore scores[], Vector2 pos, int value, Color color) {
    for (int i = 0; i < MAX_FLOATING_SCORES; i++) {
        if (!scores[i].active) {
            scores[i].pos = pos;
            scores[i].value = value;
            scores[i].alpha = 1.0f;
            scores[i].lifetime = 1.0f;
            scores[i].color = color;
            scores[i].active = true;
            break;
        }
    }
}

void ResetGame(void) {
    gameState = STATE_GAMEPLAY;
    score = 0;
    enemyCount = 0;
    frameCount = 0;
    screenShake = 0.0f;
    scoreSent = false;
    playerPos = (Vector2){ (float)screenWidth / 2.0f, (float)screenHeight / 2.0f };
    playerVel = (Vector2){ 0.0f, 0.0f };
    playerRotation = -PI / 2.0f;
    for (int i = 0; i < MAX_PARTICLES; i++) particles[i].active = false;
    for (int i = 0; i < MAX_FLOATING_SCORES; i++) floatingScores[i].active = false;
}

void UpdateDrawFrame(void) {
    // Update
    if (gameState == STATE_START) {
        int key = GetCharPressed();
        while (key > 0) {
            if ((key >= 32) && (key <= 125) && (nameLetterCount < MAX_NAME_LENGTH)) {
                playerName[nameLetterCount] = (char)key;
                playerName[nameLetterCount + 1] = '\0';
                nameLetterCount++;
            }
            key = GetCharPressed();
        }

        if (IsKeyPressed(KEY_BACKSPACE)) {
            nameLetterCount--;
            if (nameLetterCount < 0) nameLetterCount = 0;
            playerName[nameLetterCount] = '\0';
        }

        if (IsKeyPressed(KEY_ENTER)) {
            if (nameLetterCount == 0) {
                strcpy(playerName, "Pilot");
                nameLetterCount = 5;
            }
            ResetGame();
        }
    }
    else if (gameState == STATE_GAMEPLAY) {
        playerAccel = (Vector2){0.0f, 0.0f};
        
        if (IsKeyDown(KEY_A)) playerRotation -= rotationSpeed;
        if (IsKeyDown(KEY_D)) playerRotation += rotationSpeed;
        
        if (IsKeyDown(KEY_W)) {
            playerAccel.x = cosf(playerRotation);
            playerAccel.y = sinf(playerRotation);
            playerAccel = Vector2Normalize(playerAccel);
            playerAccel = Vector2Scale(playerAccel, playerTurningForce);
        }
        
        playerVel.x += playerAccel.x;
        playerVel.y += playerAccel.y;
        playerPos.x += playerVel.x;
        playerPos.y += playerVel.y;
        
        if (playerPos.x - playerRadius < 0.0f) {
            playerPos.x = playerRadius;
            playerVel.x = -playerVel.x * bounceRestitution;
        } else if (playerPos.x + playerRadius > screenWidth) {
            playerPos.x = screenWidth - playerRadius;
            playerVel.x = -playerVel.x * bounceRestitution;
        }

        if (playerPos.y - playerRadius < 0.0f) {
            playerPos.y = playerRadius;
            playerVel.y = -playerVel.y * bounceRestitution;
        } else if (playerPos.y + playerRadius > screenHeight) {
            playerPos.y = screenHeight - playerRadius;
            playerVel.y = -playerVel.y * bounceRestitution;
        }
        
        frameCount++;
        if (frameCount % spawnInterval == 0) {
            Vector2 spawnPos = GetRandomScreenEdgePos(screenWidth, screenHeight, 15.0f);
            SpawnEnemy(enemies, &enemyCount, spawnPos, 5.0f, 25.0f);
        }
        
        for (int i = 0; i < enemyCount; i++) {
            if (enemies[i].active) {
                Enemy e = enemies[i];
                e.accel = (Vector2){(playerPos.x - e.pos.x),(playerPos.y - e.pos.y)};
                e.accel = Vector2Normalize(e.accel);
                e.accel = Vector2Scale(e.accel, e.turningForce);
                e.vel = Vector2Add(e.vel, e.accel);
                e.pos = Vector2Add(e.pos, e.vel);
                e.vel = Vector2ClampValue(e.vel, -1.0f * e.maxSpeed, e.maxSpeed);
                enemies[i] = e;
            }
        }

        for (int i = 0; i < enemyCount; i++) {
            if (!enemies[i].active) continue;

            float distSqr = Vector2DistanceSqr(playerPos, enemies[i].pos);

            if (distSqr <= playerHitDistSqr) {
                SpawnPopEffect(particles, playerPos);
                screenShake = 22.0f;
                gameState = STATE_GAMEOVER;
                
                if (!scoreSent) {
                    SendScoreWebhook(score, playerName);
                    scoreSent = true;
                }
                break;
            }

            if (distSqr <= nearMissDistSqr) {
                if (!enemies[i].nearMissAwarded) {
                    enemies[i].nearMissAwarded = true;
                    score += 50;
                    SpawnFloatingScore(floatingScores, enemies[i].pos, 50, SKYBLUE);
                }
            } else {
                enemies[i].nearMissAwarded = false;
            }
        }
        
        for (int i = 0; i < enemyCount; i++) {
            if (!enemies[i].active) continue;

            for (int j = i + 1; j < enemyCount; j++) {
                if (!enemies[j].active) continue;

                if (Vector2DistanceSqr(enemies[i].pos, enemies[j].pos) <= collisionDistSqr) {
                    Vector2 deathPos = Vector2Scale(Vector2Add(enemies[i].pos, enemies[j].pos), 0.5f);
                    SpawnPopEffect(particles, deathPos);
                    SpawnFloatingScore(floatingScores, deathPos, 200, YELLOW);
                    screenShake = 10.0f;
                    score += 200;

                    enemies[i].active = false;
                    enemies[j].active = false;
                    break;
                }
            }
        }
        
        for (int i = 0; i < enemyCount; i++) {
            if (!enemies[i].active) {
                enemies[i] = enemies[enemyCount - 1];
                enemyCount--;
                i--;
            }
        }

        for (int i = 0; i < MAX_PARTICLES; i++) {
            if (particles[i].active) {
                particles[i].radius += 1.2f;
                particles[i].alpha -= 0.05f;
                if (particles[i].alpha <= 0.0f || particles[i].radius >= particles[i].maxRadius) {
                    particles[i].active = false;
                }
            }
        }

        for (int i = 0; i < MAX_FLOATING_SCORES; i++) {
            if (floatingScores[i].active) {
                floatingScores[i].pos.y -= 0.9f;
                floatingScores[i].lifetime -= 0.02f;
                floatingScores[i].alpha = floatingScores[i].lifetime;
                if (floatingScores[i].lifetime <= 0.0f) {
                    floatingScores[i].active = false;
                }
            }
        }

        if (screenShake > 0.0f) {
            camera.offset.x = (float)GetRandomValue(-screenShake, screenShake);
            camera.offset.y = (float)GetRandomValue(-screenShake, screenShake);
            screenShake -= 0.6f;
            if (screenShake < 0.0f) screenShake = 0.0f;
        } else {
            camera.offset = (Vector2){0.0f, 0.0f};
        }
    }
    else if (gameState == STATE_GAMEOVER) {
        if (IsKeyPressed(KEY_R) || IsKeyPressed(KEY_ENTER)) {
            ResetGame();
        }

        if (screenShake > 0.0f) {
            camera.offset.x = (float)GetRandomValue(-screenShake, screenShake);
            camera.offset.y = (float)GetRandomValue(-screenShake, screenShake);
            screenShake -= 0.6f;
            if (screenShake < 0.0f) screenShake = 0.0f;
        } else {
            camera.offset = (Vector2){0.0f, 0.0f};
        }
    }
    
    // Draw
    BeginDrawing();
        ClearBackground(BLACK);
        
        if (gameState == STATE_START) {
            const char *titleText = "ASTEROIDS SURVIVAL";
            const char *enterPrompt = "TYPE YOUR NAME:";
            const char *startPrompt = "PRESS [ENTER] TO START";
            const char *controlsText = "W: Thrust | A/D: Rotate";

            DrawText(titleText, screenWidth / 2 - MeasureText(titleText, 40) / 2, screenHeight / 2 - 160, 40, RED);
            DrawText(enterPrompt, screenWidth / 2 - MeasureText(enterPrompt, 20) / 2, screenHeight / 2 - 70, 20, RAYWHITE);

            // Name input box
            Rectangle textBox = { (float)screenWidth / 2 - 150, (float)screenHeight / 2 - 30, 300, 50 };
            DrawRectangleRec(textBox, Fade(DARKGRAY, 0.3f));
            DrawRectangleLinesEx(textBox, 2, RED);

            const char *renderedName = (nameLetterCount > 0) ? playerName : "PILOT";
            Color nameColor = (nameLetterCount > 0) ? YELLOW : GRAY;
            DrawText(renderedName, (int)(textBox.x + textBox.width / 2 - MeasureText(renderedName, 26) / 2), (int)(textBox.y + 12), 26, nameColor);

            DrawText(startPrompt, screenWidth / 2 - MeasureText(startPrompt, 20) / 2, screenHeight / 2 + 50, 20, RAYWHITE);
            DrawText(controlsText, screenWidth / 2 - MeasureText(controlsText, 18) / 2, screenHeight / 2 + 90, 18, GRAY);
        }
        else if (gameState == STATE_GAMEPLAY || gameState == STATE_GAMEOVER) {
            BeginMode2D(camera);

            if (gameState == STATE_GAMEPLAY) {
                Vector2 v1 = Vector2Add(playerPos, Vector2Rotate((Vector2){ 15.0f, 0.0f }, playerRotation));
                Vector2 v2 = Vector2Add(playerPos, Vector2Rotate((Vector2){ -10.0f, -8.0f }, playerRotation));
                Vector2 v3 = Vector2Add(playerPos, Vector2Rotate((Vector2){ -10.0f, 8.0f }, playerRotation));
                DrawTriangle(v1, v2, v3, RED);
            }

            for (int i = 0; i < enemyCount; i++) {
                if (enemies[i].active) {
                    DrawCircleV(enemies[i].pos, 8.0f, MAROON);
                }
            }

            for (int i = 0; i < MAX_PARTICLES; i++) {
                if (particles[i].active) {
                    Color popColor = Fade(GOLD, particles[i].alpha);
                    DrawCircleLines((int)particles[i].pos.x, (int)particles[i].pos.y, particles[i].radius, popColor);
                    DrawCircleLines((int)particles[i].pos.x, (int)particles[i].pos.y, particles[i].radius * 0.6f, Fade(ORANGE, particles[i].alpha));
                }
            }

            for (int i = 0; i < MAX_FLOATING_SCORES; i++) {
                if (floatingScores[i].active) {
                    const char *scoreText = TextFormat("+%d", floatingScores[i].value);
                    DrawText(scoreText, (int)floatingScores[i].pos.x, (int)floatingScores[i].pos.y, 16, Fade(floatingScores[i].color, floatingScores[i].alpha));
                }
            }

            EndMode2D();

            DrawText(TextFormat("Name: %s", playerName), 20, 20, 20, RAYWHITE);
            DrawText(TextFormat("Score: %06d", score), 20, 48, 24, YELLOW);
            DrawText(TextFormat("Vel: %.2f, %.2f", playerVel.x, playerVel.y), 20, 78, 16, GRAY);

            if (gameState == STATE_GAMEOVER) {
                DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.85f));

                const char *overText = "GAME OVER";
                const char *finalScoreText = TextFormat("%s: %d", playerName, score);
                const char *boardTitle = "-- TOP SCORES --";
                const char *restartText = "PRESS [R] OR [ENTER] TO RETRY";

                DrawText(overText, screenWidth / 2 - MeasureText(overText, 46) / 2, 120, 46, RED);
                DrawText(finalScoreText, screenWidth / 2 - MeasureText(finalScoreText, 24) / 2, 180, 24, YELLOW);
                DrawText(boardTitle, screenWidth / 2 - MeasureText(boardTitle, 22) / 2, 240, 22, GOLD);

                if (!leaderboardLoaded) {
                    const char *loadingText = "Fetching Leaderboard...";
                    DrawText(loadingText, screenWidth / 2 - MeasureText(loadingText, 18) / 2, 330, 18, GRAY);
                } else if (topScoreCount == 0) {
                    const char *emptyText = "No entries logged yet.";
                    DrawText(emptyText, screenWidth / 2 - MeasureText(emptyText, 18) / 2, 330, 18, GRAY);
                } else {
                    int startY = 280;
                    for (int i = 0; i < topScoreCount; i++) {
                        const char *rankText = TextFormat("%d. %-15s %6d", i + 1, topScores[i].name, topScores[i].score);
                        Color entryColor = (i == 0) ? GOLD : RAYWHITE;
                        DrawText(rankText, screenWidth / 2 - MeasureText(rankText, 20) / 2, startY + (i * 36), 20, entryColor);
                    }
                }

                DrawText(restartText, screenWidth / 2 - MeasureText(restartText, 20) / 2, screenHeight - 160, 20, RAYWHITE);
            }
        }
    EndDrawing();
}

int main(void)
{
    InitWindow(screenWidth, screenHeight, "raylib - Asteroids Web");
    playerPos = (Vector2){ (float)screenWidth / 2.0f, (float)screenHeight / 2.0f };
    camera.zoom = 1.0f;

    FetchLeaderboard();

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 60, 1);
#else
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        UpdateDrawFrame();
    }
#endif

    CloseWindow();
    return 0;
}