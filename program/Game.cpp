#include "Main.h"
#include "Game.h"
#include "DxLib.h"
#include <math.h>
#include <stdlib.h>
#include <vector>
#include <queue>

// --- シーン定義 ---
enum Scene {
    SCENE_TITLE,
    SCENE_PLAY,
    SCENE_CLEAR,
    SCENE_GAMEOVER
};

static Scene g_scene = SCENE_TITLE;

// --- 画面・レイアウト設定 ---
const int SCREEN_WIDTH = 960;
const int SCREEN_HEIGHT = 640;
const int GAME_AREA_WIDTH = 400;
const int GAME_LEFT_OFFSET = (SCREEN_WIDTH - GAME_AREA_WIDTH) / 2;

const float PI = 3.14159265f;

// --- マップ・球設定 ---
const int MAP_WIDTH = 8;
const int MAP_HEIGHT = 14;
const float BALL_SIZE = 50.0f;
const float BALL_RADIUS = BALL_SIZE / 2.0f;
const float ROW_SPACING = BALL_SIZE * 0.866025f;
const int DEAD_LINE_ROW = 10;

enum BallColor {
    COLOR_NONE = -1,
    COLOR_RED,
    COLOR_BLUE,
    COLOR_YELLOW,
    COLOR_GREEN,
    COLOR_WALL,
    COLOR_MAX = COLOR_WALL
};

static int g_map[MAP_HEIGHT][MAP_WIDTH];
static int g_wallLineCount = 0;

struct Ball {
    float x, y, vx, vy;
    bool isMoving;
    BallColor color;
};

static Ball g_ball;
static float g_angle;
const float SHOT_SPEED = 12.0f;
static BallColor g_nextColor;

static int g_score = 0;
static int g_shotCount = 0;
const float LAUNCHER_X = GAME_LEFT_OFFSET + (GAME_AREA_WIDTH / 2.0f);
const float LAUNCHER_Y = SCREEN_HEIGHT - 40.0f;
const float X_PADDING = (GAME_AREA_WIDTH - (BALL_SIZE * MAP_WIDTH)) / 2.0f;

// --- ユーティリティ関数群 ---

void GetGridCenterPos(int gx, int gy, float* outX, float* outY) {
    int visualY = gy + g_wallLineCount;
    if (visualY % 2 == 1) {
        *outX = GAME_LEFT_OFFSET + X_PADDING + (gx * BALL_SIZE + BALL_SIZE);
    }
    else {
        *outX = GAME_LEFT_OFFSET + X_PADDING + (gx * BALL_SIZE + BALL_RADIUS);
    }
    *outY = visualY * ROW_SPACING + BALL_RADIUS;
}

void FindBestGrid(float ballX, float ballY, int* outGx, int* outGy) {
    int visualY = (int)((ballY - BALL_RADIUS) / ROW_SPACING + 0.5f);
    int gy = visualY - g_wallLineCount;

    if (gy < 0) gy = 0;
    if (gy >= MAP_HEIGHT) gy = MAP_HEIGHT - 1;

    float relativeX = ballX - GAME_LEFT_OFFSET - X_PADDING;
    int gx = (visualY % 2 == 1) ? (int)((relativeX - BALL_SIZE) / BALL_SIZE + 0.5f) : (int)((relativeX - BALL_RADIUS) / BALL_SIZE + 0.5f);

    if (gx > MAP_WIDTH - 1) gx = MAP_WIDTH - 1;
    if (gx < 0) gx = 0;

    *outGx = gx; *outGy = gy;
}

void FindBestGridWithCollision(float ballX, float ballY, int* outGx, int* outGy) {
    FindBestGrid(ballX, ballY, outGx, outGy);
    int gx = *outGx, gy = *outGy;

    if (g_map[gy][gx] != COLOR_NONE) {
        float minDraw = 999999.0f;
        int backupX = gx, backupY = gy;
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                int cx = gx + dx, cy = gy + dy;
                if (cx >= 0 && cx < MAP_WIDTH && cy >= 0 && cy < MAP_HEIGHT) {
                    if (g_map[cy][cx] == COLOR_NONE) {
                        float tx, ty; GetGridCenterPos(cx, cy, &tx, &ty);
                        float dist = (ballX - tx) * (ballX - tx) + (ballY - ty) * (ballY - ty);
                        if (dist < minDraw) { minDraw = dist; backupX = cx; backupY = cy; }
                    }
                }
            }
        }
        gx = backupX; gy = backupY;
    }
    if (g_map[gy][gx] != COLOR_NONE && gy < MAP_HEIGHT - 1) gy++;
    *outGx = gx; *outGy = gy;
}

int GetBallColorValue(int color) {
    switch (color) {
    case COLOR_RED:     return GetColor(255, 60, 60);
    case COLOR_BLUE:    return GetColor(60, 120, 255);
    case COLOR_YELLOW:  return GetColor(255, 230, 40);
    case COLOR_GREEN:   return GetColor(40, 220, 80);
    default:            return GetColor(200, 200, 200);
    }
}

bool CheckCollision(float x1, float y1, float r1, float x2, float y2, float r2) {
    float dx = x1 - x2, dy = y1 - y2;
    return ((dx * dx) + (dy * dy)) < ((r1 + r2) * (r1 + r2));
}

std::vector<std::pair<int, int>> GetNeighbors(int cx, int cy) {
    std::vector<std::pair<int, int>> n;
    n.push_back({ cx - 1, cy }); n.push_back({ cx + 1, cy });

    int visualY = cy + g_wallLineCount;
    if (visualY % 2 == 0) {
        n.push_back({ cx - 1, cy - 1 }); n.push_back({ cx, cy - 1 });
        n.push_back({ cx - 1, cy + 1 }); n.push_back({ cx, cy + 1 });
    }
    else {
        n.push_back({ cx, cy - 1 }); n.push_back({ cx + 1, cy - 1 });
        n.push_back({ cx, cy + 1 }); n.push_back({ cx + 1, cy + 1 });
    }
    return n;
}

bool IsGameClear() {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (g_map[y][x] != COLOR_NONE && g_map[y][x] != COLOR_WALL) return false;
        }
    }
    return true;
}

void DropFloatingBalls() {
    bool connected[MAP_HEIGHT][MAP_WIDTH] = { false };
    std::queue<std::pair<int, int>> q;
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (g_map[y][x] != COLOR_NONE && (y == 0 || g_map[y][x] == COLOR_WALL)) {
                q.push({ x, y }); connected[y][x] = true;
            }
        }
    }
    while (!q.empty()) {
        auto c = q.front(); q.pop();
        for (auto next : GetNeighbors(c.first, c.second)) {
            if (next.first >= 0 && next.first < MAP_WIDTH && next.second >= 0 && next.second < MAP_HEIGHT) {
                if (!connected[next.second][next.first] && g_map[next.second][next.first] != COLOR_NONE) {
                    connected[next.second][next.first] = true; q.push(next);
                }
            }
        }
    }
    int dropIdx = 0;
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (g_map[y][x] != COLOR_NONE && g_map[y][x] != COLOR_WALL && !connected[y][x]) {
                g_map[y][x] = COLOR_NONE; dropIdx++; g_score += 10 * (1 << dropIdx);
            }
        }
    }
}

void EraseConnectedBalls(int sx, int sy, int color) {
    if (color == COLOR_NONE || color == COLOR_WALL) return;
    bool visited[MAP_HEIGHT][MAP_WIDTH] = { false };
    std::vector<std::pair<int, int>> found;
    std::queue<std::pair<int, int>> q;
    q.push({ sx, sy }); visited[sy][sx] = true;
    while (!q.empty()) {
        auto c = q.front(); q.pop(); found.push_back(c);
        for (auto next : GetNeighbors(c.first, c.second)) {
            if (next.first >= 0 && next.first < MAP_WIDTH && next.second >= 0 && next.second < MAP_HEIGHT) {
                if (!visited[next.second][next.first] && g_map[next.second][next.first] == color) {
                    visited[next.second][next.first] = true; q.push(next);
                }
            }
        }
    }
    if (found.size() >= 3) {
        for (auto p : found) g_map[p.second][p.first] = COLOR_NONE;
        g_score += found.size() * 10;
        DropFloatingBalls();
        if (IsGameClear()) g_scene = SCENE_CLEAR;
    }
}

void AdvanceCeiling() {
    g_wallLineCount += 2;
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (y + g_wallLineCount < g_wallLineCount) {
                g_map[y][x] = COLOR_WALL;
            }
        }
    }
    DropFloatingBalls();
}

void CheckDeadLineViolation() {
    float dummyX, deadLineY;
    int prevCount = g_wallLineCount; g_wallLineCount = 0;
    GetGridCenterPos(0, DEAD_LINE_ROW, &dummyX, &deadLineY);
    g_wallLineCount = prevCount;

    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (g_map[y][x] != COLOR_NONE && g_map[y][x] != COLOR_WALL) {
                float bx, by;
                GetGridCenterPos(x, y, &bx, &by);
                if (by + BALL_RADIUS > deadLineY) {
                    g_scene = SCENE_GAMEOVER;
                    return;
                }
            }
        }
    }
}

// --- ゲームメイン処理 ---

// 【修正】少し難易度を上げ、パズル要素を強化した初期配置
void GameInit() {
    g_angle = -PI / 2.0f; g_score = 0; g_shotCount = 0; g_wallLineCount = 0;

    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            g_map[y][x] = COLOR_NONE;
        }
    }

    // 4色が確実に使われるようにベースの色順を作成
    int colorOrder[4] = { COLOR_RED, COLOR_BLUE, COLOR_YELLOW, COLOR_GREEN };
    for (int i = 0; i < 4; i++) {
        int r = rand() % 4;
        int temp = colorOrder[i];
        colorOrder[i] = colorOrder[r];
        colorOrder[r] = temp;
    }

    int colorIdx = 0;
    for (int x = 0; x < MAP_WIDTH; x++) {
        if (x % 2 == 0) {
            // 列ごとにメインカラーとサブカラーの2色を選び、シマシマを作る
            BallColor colorA = (BallColor)colorOrder[colorIdx % 4];
            BallColor colorB = (BallColor)colorOrder[(colorIdx + 1) % 4];
            colorIdx++;

            // 行数を少し増やして「6行」の深さに（少しデッドラインに近づけて緊張感を出す）
            int columnLength = 6;
            for (int y = 0; y < columnLength; y++) {
                if (y < MAP_HEIGHT) {
                    // 2マスずつ交互に色を変えることで、パズルらしい構造にする
                    if ((y / 2) % 2 == 0) {
                        g_map[y][x] = colorA;
                    }
                    else {
                        g_map[y][x] = colorB;
                    }
                }
            }
        }
    }

    g_ball.isMoving = false; g_ball.x = LAUNCHER_X; g_ball.y = LAUNCHER_Y;
    g_ball.color = (BallColor)(rand() % COLOR_MAX); g_nextColor = (BallColor)(rand() % COLOR_MAX);
}

void GameUpdate() {
    switch (g_scene) {
    case SCENE_TITLE:
        if (PushHitKey(KEY_INPUT_SPACE)) { GameInit(); g_scene = SCENE_PLAY; }
        break;

    case SCENE_PLAY:
        CheckDeadLineViolation();
        if (g_scene == SCENE_GAMEOVER) break;

        if (!g_ball.isMoving) {
            if (CheckHitKey(KEY_INPUT_LEFT)) g_angle -= 0.03f;
            if (CheckHitKey(KEY_INPUT_RIGHT)) g_angle += 0.03f;
            if (g_angle < -PI + 0.2f) g_angle = -PI + 0.2f;
            if (g_angle > -0.2f) g_angle = -0.2f;
            if (PushHitKey(KEY_INPUT_SPACE)) {
                g_ball.vx = cosf(g_angle) * SHOT_SPEED; g_ball.vy = sinf(g_angle) * SHOT_SPEED;
                g_ball.isMoving = true;
            }
        }
        else {
            g_ball.x += g_ball.vx; g_ball.y += g_ball.vy;

            if (g_ball.x - BALL_RADIUS < GAME_LEFT_OFFSET + X_PADDING) { g_ball.x = GAME_LEFT_OFFSET + X_PADDING + BALL_RADIUS; g_ball.vx *= -1.0f; }
            if (g_ball.x + BALL_RADIUS > GAME_LEFT_OFFSET + GAME_AREA_WIDTH - X_PADDING) { g_ball.x = GAME_LEFT_OFFSET + GAME_AREA_WIDTH - X_PADDING - BALL_RADIUS; g_ball.vx *= -1.0f; }

            bool hit = false; int hgx, hgy;

            float ceilingY = g_wallLineCount * ROW_SPACING;
            if (g_ball.y - BALL_RADIUS <= ceilingY) {
                hit = true;
                FindBestGridWithCollision(g_ball.x, ceilingY + BALL_RADIUS, &hgx, &hgy);
            }
            else {
                for (int y = 0; y < MAP_HEIGHT; y++) {
                    for (int x = 0; x < MAP_WIDTH; x++) {
                        if (g_map[y][x] != COLOR_NONE) {
                            float mx, my; GetGridCenterPos(x, y, &mx, &my);
                            if (CheckCollision(g_ball.x, g_ball.y, BALL_RADIUS - 2.0f, mx, my, BALL_RADIUS - 2.0f)) {
                                FindBestGridWithCollision(g_ball.x, g_ball.y, &hgx, &hgy);
                                hit = true; break;
                            }
                        }
                    }
                    if (hit) break;
                }
            }

            if (hit) {
                if (hgy < MAP_HEIGHT && hgx < MAP_WIDTH) {
                    g_map[hgy][hgx] = g_ball.color;
                    EraseConnectedBalls(hgx, hgy, g_ball.color);
                }
                g_ball.isMoving = false; g_ball.x = LAUNCHER_X; g_ball.y = LAUNCHER_Y;
                g_ball.color = g_nextColor; g_nextColor = (BallColor)(rand() % COLOR_MAX);

                g_shotCount++;
                if (g_shotCount >= 6) { AdvanceCeiling(); g_shotCount = 0; }

                CheckDeadLineViolation();
                if (IsGameClear() && g_scene != SCENE_GAMEOVER) g_scene = SCENE_CLEAR;
            }
        }
        break;

    case SCENE_CLEAR:
    case SCENE_GAMEOVER:
        if (PushHitKey(KEY_INPUT_R)) { GameInit(); g_scene = SCENE_PLAY; }
        if (PushHitKey(KEY_INPUT_SPACE)) { g_scene = SCENE_TITLE; }
        break;
    }
}

void GameRender() {
    DrawBox(0, 0, GAME_LEFT_OFFSET, SCREEN_HEIGHT, GetColor(30, 30, 35), TRUE);
    DrawBox(GAME_LEFT_OFFSET, 0, GAME_LEFT_OFFSET + GAME_AREA_WIDTH, SCREEN_HEIGHT, GetColor(10, 10, 15), TRUE);
    DrawBox(GAME_LEFT_OFFSET + GAME_AREA_WIDTH, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(30, 30, 35), TRUE);
    DrawLine(GAME_LEFT_OFFSET + X_PADDING, 0, GAME_LEFT_OFFSET + X_PADDING, SCREEN_HEIGHT, GetColor(100, 100, 100), 2);
    DrawLine(GAME_LEFT_OFFSET + GAME_AREA_WIDTH - X_PADDING, 0, GAME_LEFT_OFFSET + GAME_AREA_WIDTH - X_PADDING, SCREEN_HEIGHT, GetColor(100, 100, 100), 2);

    float lx, ly;
    int prevCount = g_wallLineCount; g_wallLineCount = 0;
    GetGridCenterPos(0, DEAD_LINE_ROW, &lx, &ly);
    g_wallLineCount = prevCount;

    for (int i = X_PADDING; i < GAME_AREA_WIDTH - X_PADDING; i += 10) {
        DrawLine(GAME_LEFT_OFFSET + i, (int)ly, GAME_LEFT_OFFSET + i + 5, (int)ly, GetColor(255, 50, 50), 2);
    }

    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (g_map[y][x] != COLOR_NONE && g_map[y][x] != COLOR_WALL) {
                float mx, my; GetGridCenterPos(x, y, &mx, &my);
                DrawCircle((int)mx, (int)my, (int)BALL_RADIUS, GetBallColorValue(g_map[y][x]), TRUE);
                DrawCircle((int)mx, (int)my, (int)BALL_RADIUS, GetColor(0, 0, 0), FALSE);
            }
        }
    }

    if (g_wallLineCount > 0) {
        float wy = g_wallLineCount * ROW_SPACING;
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 80);
        DrawBox(GAME_LEFT_OFFSET + X_PADDING, 0, GAME_LEFT_OFFSET + GAME_AREA_WIDTH - X_PADDING, (int)wy, GetColor(200, 50, 50), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 180);
        DrawBox(GAME_LEFT_OFFSET + X_PADDING, (int)wy - 4, GAME_LEFT_OFFSET + GAME_AREA_WIDTH - X_PADDING, (int)wy, GetColor(255, 100, 100), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    if (g_scene == SCENE_PLAY) {
        if (!g_ball.isMoving) {
            float tx = LAUNCHER_X + cosf(g_angle) * 60, ty = LAUNCHER_Y + sinf(g_angle) * 60;
            DrawLine((int)LAUNCHER_X, (int)LAUNCHER_Y, (int)tx, (int)ty, GetBallColorValue(g_ball.color), 3);
        }
        DrawCircle((int)g_ball.x, (int)g_ball.y, (int)BALL_RADIUS, GetBallColorValue(g_ball.color), TRUE);
        DrawCircle((int)g_ball.x, (int)g_ball.y, (int)BALL_RADIUS, GetColor(0, 0, 0), FALSE);
    }

    DrawString(30, 50, "SCORE", GetColor(150, 150, 150));
    DrawFormatString(30, 80, GetColor(255, 255, 255), "%06d", g_score);
    int nx = LAUNCHER_X + 70; DrawString(nx - 15, LAUNCHER_Y - 20, "NEXT", GetColor(200, 200, 200));
    DrawCircle(nx, LAUNCHER_Y, 12, GetBallColorValue(g_nextColor), TRUE);

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 180);
    if (g_scene == SCENE_TITLE) {
        DrawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(0, 0, 0), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        DrawString(SCREEN_WIDTH / 2 - 120, SCREEN_HEIGHT / 2 - 60, "BUBBLE SHOOTER", GetColor(100, 255, 100), TRUE);
        DrawString(SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2 + 20, "PRESS SPACE TO START", GetColor(255, 255, 255));
    }
    else if (g_scene == SCENE_CLEAR) {
        DrawBox(GAME_LEFT_OFFSET, 0, GAME_LEFT_OFFSET + GAME_AREA_WIDTH, SCREEN_HEIGHT, GetColor(0, 50, 0), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        DrawString(SCREEN_WIDTH / 2 - 55, SCREEN_HEIGHT / 2 - 40, "GAME CLEAR!", GetColor(255, 255, 100));
        DrawString(SCREEN_WIDTH / 2 - 90, SCREEN_HEIGHT / 2, "R: Retry / Space: Title", GetColor(255, 255, 255));
    }
    else if (g_scene == SCENE_GAMEOVER) {
        DrawBox(GAME_LEFT_OFFSET, 0, GAME_LEFT_OFFSET + GAME_AREA_WIDTH, SCREEN_HEIGHT, GetColor(50, 0, 0), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        DrawString(SCREEN_WIDTH / 2 - 50, SCREEN_HEIGHT / 2 - 40, "GAME OVER", GetColor(255, 50, 50));
        DrawString(SCREEN_WIDTH / 2 - 90, SCREEN_HEIGHT / 2, "R: Retry / Space: Title", GetColor(255, 255, 255));
    }
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

void GameExit() {}