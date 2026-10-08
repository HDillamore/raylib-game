#include "raylib.h"
#include "raymath.h"

#define MAX_ENEMIES 100
//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------

typedef struct Enemy {
    Vector2 pos;
    Vector2 vel;
    Vector2 accel;
    float maxSpeed;
    float turningForce;
    bool active;
} Enemy;
    
Enemy CreateEnemy(Vector2 pos, float maxSpeed, float turningForce) {
    
    Enemy e = {0};
    e.pos = pos;
    e.vel = (Vector2){0.0f, 0.0f};
    e.accel = (Vector2){0.0f, 0.0f};
    e.maxSpeed = maxSpeed;
    e.turningForce = turningForce;
    e.active = true;
    return e;
}

bool SpawnEnemy(Enemy enemies[], int *count, Vector2 pos, float maxSpeed, float turningForce) {
    
    if (*count >= MAX_ENEMIES) return false;

    enemies[*count] = CreateEnemy(pos, maxSpeed, turningForce);
    (*count)++;
    return true;
}

Vector2 GetRandomScreenEdgePos(int screenWidth, int screenHeight, float padding) {
    int edge = GetRandomValue(0, 3); // 0: Top, 1: Right, 2: Bottom, 3: Left

    switch (edge) {
        case 0: // Top
            return (Vector2){ (float)GetRandomValue(0, screenWidth), -padding };
        case 1: // Right
            return (Vector2){ (float)screenWidth + padding, (float)GetRandomValue(0, screenHeight) };
        case 2: // Bottom
            return (Vector2){ (float)GetRandomValue(0, screenWidth), (float)screenHeight + padding };
        case 3: // Left
        default:
            return (Vector2){ -padding, (float)GetRandomValue(0, screenHeight) };
    }
}


int main(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 900;
    const int screenHeight = 900;

    InitWindow(screenWidth, screenHeight, "raylib [core] example - basic window");
    SetTargetFPS(60);
    
    // Variable Initialization
    
    Vector2 playerPos = {screenWidth/2, screenHeight/2};
    Vector2 playerVel = {0.0f, 0.0f};
    Vector2 playerAccel = {0.0f, 0.0f};
    Vector2 playerRotation = {0.0f, 0.0f};
    float playerMaxSpeed = 100.0f;
    float playerTurningForce = 0.25f;
    
    
    Enemy enemies[MAX_ENEMIES] = {0};
    int enemyCount = 0;
    
    int frameCount = 0;
    const int spawnInterval = 60;
    
    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        
        playerAccel = (Vector2){0.0f, 0.0f};
        
        if (IsKeyDown(KEY_W)) playerAccel.y -= 1.0f;
        if (IsKeyDown(KEY_S)) playerAccel.y += 1.0f;
        if (IsKeyDown(KEY_D)) playerAccel.x += 1.0f;
        if (IsKeyDown(KEY_A)) playerAccel.x -= 1.0f;
        
        playerAccel = Vector2Normalize(playerAccel);
        playerAccel = Vector2Scale(playerAccel, playerTurningForce);
        
        playerVel.x += playerAccel.x;
        playerVel.y += playerAccel.y;
        
        playerPos.x += playerVel.x;
        playerPos.y += playerVel.y;
        
        frameCount++;
        if (frameCount % spawnInterval == 0) {
            Vector2 spawnPos = GetRandomScreenEdgePos(screenWidth, screenHeight, 15.0f);
            SpawnEnemy(enemies, &enemyCount, spawnPos, 5.0f, 0.25f);
        }
        
        for(int i = 0; i < enemyCount; i++){
            if(enemies[i].active) {
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
        
        
        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(BLACK);
            
            DrawCircleV(playerPos, 10, RED);
            DrawText(TextFormat("Accel: (%.2f, %.2f)", playerAccel.x, playerAccel.y), 10, 10, 16, BLACK);

            for (int i = 0; i < enemyCount; i++) {
                if (enemies[i].active) {
                    DrawCircleV(enemies[i].pos, 8.0f, MAROON);
                }
            }


        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}