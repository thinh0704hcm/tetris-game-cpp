#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

using namespace std;

const int H = 20;
const int W = 15;
const int cellSize = 30;
const int screenWidth = 650;
const int screenHeight = 600;
const int MAX_PARTICLES = 600;

Color palette[] = {
    {0x00, 0xF0, 0xF0, 0xFF}, {0x00, 0x00, 0xF0, 0xFF},
    {0xF0, 0xA0, 0x00, 0xFF}, {0xF0, 0xF0, 0x00, 0xFF},
    {0x00, 0xF0, 0x00, 0xFF}, {0xA0, 0x00, 0xF0, 0xFF},
    {0xF0, 0x00, 0x00, 0xFF},
};
Color blockColors[7];

char board[H][W] = {};
char blocks[][4][4] = {{{' ', 'I', ' ', ' '},
                        {' ', 'I', ' ', ' '},
                        {' ', 'I', ' ', ' '},
                        {' ', 'I', ' ', ' '}},
                       {{' ', ' ', ' ', ' '},
                        {' ', 'O', 'O', ' '},
                        {' ', 'O', 'O', ' '},
                        {' ', ' ', ' ', ' '}},
                       {{' ', ' ', ' ', ' '},
                        {' ', 'T', ' ', ' '},
                        {'T', 'T', 'T', ' '},
                        {' ', ' ', ' ', ' '}},
                       {{' ', ' ', ' ', ' '},
                        {' ', 'S', 'S', ' '},
                        {'S', 'S', ' ', ' '},
                        {' ', ' ', ' ', ' '}},
                       {{' ', ' ', ' ', ' '},
                        {'Z', 'Z', ' ', ' '},
                        {' ', 'Z', 'Z', ' '},
                        {' ', ' ', ' ', ' '}},
                       {{' ', ' ', ' ', ' '},
                        {'J', ' ', ' ', ' '},
                        {'J', 'J', 'J', ' '},
                        {' ', ' ', ' ', ' '}},
                       {{' ', ' ', ' ', ' '},
                        {' ', ' ', 'L', ' '},
                        {'L', 'L', 'L', ' '},
                        {' ', ' ', ' ', ' '}}};

int gameState = 0;
const int MAX_ENTRIES = 10;
const char *LEADERBOARD_FILE = "leaderboard.txt";
char playerNames[5][MAX_ENTRIES][32];
int playerScores[5][MAX_ENTRIES];
int entryCount[5] = {0, 0, 0, 0, 0};
int leaderboardDiff = 0;
char currentName[32] = "";
int nameCharCount = 0;
int pendingScore = 0;
int pendingPlayer = 0;

struct Player {
    char board[H][W];
    char currentBlock[4][4];
    int posX, posY;
    int blockType, nextBlockType;
    int score;
    float gameTimer, dropTimer, moveSpeed;
    bool isClearing;
    float clearTimer;
    int clearingRows[4], clearingRowCount;
    bool isDropping;
    float dropStartY, dropEndY, dropAnimTimer;
    int scorePopupValue;
    float scorePopupTimer, acePopupTimer;
};

Player p1, p2;
int posX = (W / 2) - 2, posY = 0, blockType = 1, nextBlockType = 0;
int score = 0;
float gameTimer = 0;
float dropTimer = 0;
float moveSpeed = 0.5f;
int difficulty = 0;
const char *diffNames[] = {"Noob", "Pro", "Hacker", "God", "Extredemon"};
bool isClearing = false;
float clearTimer = 0;
int clearingRows[4];
int clearingRowCount = 0;
char currentBlock[4][4];

void sortLeaderboard(int d) {
    for (int i = 0; i < entryCount[d] - 1; i++) {
        for (int j = 0; j < entryCount[d] - 1 - i; j++) {
            if (playerScores[d][j] < playerScores[d][j + 1]) {
                int ts = playerScores[d][j];
                playerScores[d][j] = playerScores[d][j + 1];
                playerScores[d][j + 1] = ts;

                char tn[32];
                strcpy(tn, playerNames[d][j]);
                strcpy(playerNames[d][j], playerNames[d][j + 1]);
                strcpy(playerNames[d][j + 1], tn);
            }
        }
    }
}

void addLeaderboardEntry(int d, const char *name, int savedScore) {
    if (d < 0 || d >= 5 || name[0] == '\0')
        return;

    sortLeaderboard(d);
    if (entryCount[d] >= MAX_ENTRIES &&
        savedScore <= playerScores[d][MAX_ENTRIES - 1])
        return;

    int index = entryCount[d];
    if (index < MAX_ENTRIES)
        entryCount[d]++;
    else
        index = MAX_ENTRIES - 1;

    strncpy(playerNames[d][index], name, sizeof(playerNames[d][index]) - 1);
    playerNames[d][index][sizeof(playerNames[d][index]) - 1] = '\0';
    playerScores[d][index] = savedScore;
    sortLeaderboard(d);
}

void loadLeaderboard() {
    FILE *file = fopen(LEADERBOARD_FILE, "r");
    if (!file)
        return;

    for (int d = 0; d < 5; d++)
        entryCount[d] = 0;

    char line[128];
    while (fgets(line, sizeof(line), file)) {
        char *p = line;
        char *end = NULL;
        int d = (int)strtol(p, &end, 10);
        if (*end != '\t')
            continue;

        p = end + 1;
        int savedScore = (int)strtol(p, &end, 10);
        if (*end != '\t')
            continue;

        p = end + 1;
        p[strcspn(p, "\r\n")] = '\0';
        addLeaderboardEntry(d, p, savedScore);
    }

    fclose(file);
}

void saveLeaderboard() {
    FILE *file = fopen(LEADERBOARD_FILE, "w");
    if (!file)
        return;

    for (int d = 0; d < 5; d++)
        for (int i = 0; i < entryCount[d]; i++)
            fprintf(file, "%d\t%d\t%s\n", d, playerScores[d][i],
                    playerNames[d][i]);

    fclose(file);
}

void spawnBlock(Player &p) {
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            p.currentBlock[i][j] = blocks[p.blockType][i][j];
}

void spawnBlock() {
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            currentBlock[i][j] = blocks[blockType][i][j];
}

bool canMove(Player &p, int dx, int dy) {
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            if (p.currentBlock[i][j] != ' ') {
                int tx = p.posX + j + dx;
                int ty = p.posY + i + dy;
                if (tx <= 0 || tx >= W - 1)
                    return false;
                if (ty >= H - 1)
                    return false;
                if (ty >= 0 && p.board[ty][tx] != ' ')
                    return false;
            }
    return true;
}

bool canMove(int dx, int dy) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (currentBlock[i][j] != ' ') {
                int tx = posX + j + dx;
                int ty = posY + i + dy;
                if (tx <= 0 || tx >= W - 1)
                    return false;
                if (ty >= H - 1)
                    return false;
                if (ty >= 0 && board[ty][tx] != ' ')
                    return false;
            }
        }
    }
    return true;
}

void block2Board(Player &p) {
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            if (p.currentBlock[i][j] != ' ')
                p.board[p.posY + i][p.posX + j] = '0' + p.blockType;
}

void block2Board() {
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            if (currentBlock[i][j] != ' ')
                board[posY + i][posX + j] = '0' + blockType;
}

void rotateBlock(Player &p) {
    char rotated[4][4];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            rotated[j][3 - i] = p.currentBlock[i][j];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (rotated[i][j] != ' ') {
                int tx = p.posX + j;
                int ty = p.posY + i;
                if (tx <= 0 || tx >= W - 1 || ty >= H - 1)
                    return;
                if (ty >= 0 && p.board[ty][tx] != ' ')
                    return;
            }
        }
    }
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            p.currentBlock[i][j] = rotated[i][j];
}

void rotateBlock() {
    char rotated[4][4];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            rotated[j][3 - i] = currentBlock[i][j];

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (rotated[i][j] != ' ') {
                int tx = posX + j;
                int ty = posY + i;
                if (tx <= 0 || tx >= W - 1 || ty >= H - 1)
                    return;
                if (ty >= 0 && board[ty][tx] != ' ')
                    return;
            }
        }
    }

    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            currentBlock[i][j] = rotated[i][j];
}

void initBoard(Player &p) {
    for (int i = 0; i < H; i++)
        for (int j = 0; j < W; j++)
            if ((i == H - 1) || (j == 0) || (j == W - 1))
                p.board[i][j] = '#';
            else
                p.board[i][j] = ' ';
}

bool isDropping = false;
float dropStartY = 0, dropEndY = 0, dropAnimTimer = 0;

int scorePopupValue = 0;
float scorePopupTimer = 0;
float acePopupTimer = 0;

float pX[MAX_PARTICLES], pY[MAX_PARTICLES];
float pVX[MAX_PARTICLES], pVY[MAX_PARTICLES];
float pLife[MAX_PARTICLES];
Color pColor[MAX_PARTICLES];
int particleCount = 0;

void spawnParticles() {
    particleCount = 0;
    for (int k = 0; k < clearingRowCount && particleCount < MAX_PARTICLES;
         k++) {
        int row = clearingRows[k];
        for (int j = 1; j < W - 1 && particleCount < MAX_PARTICLES; j++) {
            int n = 4 + rand() % 4;
            for (int p = 0; p < n && particleCount < MAX_PARTICLES; p++) {
                pX[particleCount] = (j + 0.5f) * cellSize + (rand() % 20 - 10);
                pY[particleCount] =
                    (row + 0.5f) * cellSize + (rand() % 20 - 10);
                pVX[particleCount] = (rand() % 400 - 200) * 1.2f;
                pVY[particleCount] = -(100 + rand() % 400) * 1.2f;
                pLife[particleCount] = 0.5f + (rand() % 100) * 0.003f;
                pColor[particleCount] = ColorFromHSV(rand() % 360, 0.9f, 1);
                particleCount++;
            }
        }
    }
}

void updateParticles(float dt) {
    for (int i = 0; i < particleCount; i++) {
        pX[i] += pVX[i] * dt;
        pY[i] += pVY[i] * dt;
        pVY[i] += 400 * dt;
        pLife[i] -= dt;
    }
}

void drawParticles() {
    for (int i = 0; i < particleCount; i++) {
        if (pLife[i] > 0) {
            Color c = pColor[i];
            c.a = (unsigned char)((pLife[i] / 0.8f) * 255);
            float size = pLife[i] * 5 + 1;
            DrawCircle((int)pX[i], (int)pY[i], size, c);
            DrawCircle((int)pX[i], (int)pY[i], size * 0.4f,
                       Fade(WHITE, c.a / 255.0f * 0.6f));
        }
    }
}

void initBoard() {
    for (int i = 0; i < H; i++)
        for (int j = 0; j < W; j++)
            if ((i == H - 1) || (j == 0) || (j == W - 1))
                board[i][j] = '#';
            else
                board[i][j] = ' ';
}

void initColors() {
    for (int i = 0; i < 7; i++)
        blockColors[i] = palette[i];
}

void drawPlayerBoard(Player &p, int boardX, int boardY, int cellS,
                     float shakeX, float shakeY) {
    for (int i = 0; i < H; i++) {
        for (int j = 0; j < W; j++) {
            int rx = boardX + j * cellS + (int)shakeX;
            int ry = boardY + i * cellS + (int)shakeY;
            if (p.board[i][j] == '#')
                DrawRectangle(rx, ry, cellS - 1, cellS - 1, DARKGRAY);
            else if (p.board[i][j] != ' ') {
                Color blockColor;
                if (p.board[i][j] >= '0' && p.board[i][j] <= '6')
                    blockColor = blockColors[p.board[i][j] - '0'];
                else
                    blockColor = BLUE;
                DrawRectangleRounded((Rectangle){(float)rx, (float)ry,
                                                 (float)cellS - 1,
                                                 (float)cellS - 1},
                                     0.25f, 4, blockColor);
                DrawRectangleRounded(
                    (Rectangle){(float)rx + 2, (float)ry + 2,
                                (float)cellS - 5, (float)cellS - 5},
                    0.2f, 4, Fade(WHITE, 0.12f));
            }
        }
    }

    if (!p.isDropping) {
        int ghostY = p.posY;
        int g = ghostY;
        while (canMove(p, 0, g - p.posY + 1)) g++;
        ghostY = g;
        if (ghostY > p.posY) {
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    if (p.currentBlock[i][j] != ' ') {
                        float gx = (float)(boardX + (p.posX + j) * cellS + (int)shakeX);
                        float gy = (float)(boardY + (ghostY + i) * cellS + (int)shakeY);
                        Color gc = blockColors[p.blockType];
                        gc.a = 60;
                        DrawRectangleRounded(
                            (Rectangle){gx, gy, (float)cellS - 1, (float)cellS - 1},
                            0.25f, 4, gc);
                    }
                }
            }
        }
    }

    if (p.isDropping) {
        float progress = 1.0f - p.dropAnimTimer / 0.19f;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (blocks[p.blockType][i][j] != ' ') {
                    float gx = (float)(boardX + (p.posX + j) * cellS + (int)shakeX);
                    float gy = (float)(boardY + ((int)p.dropEndY + i) * cellS + (int)shakeY);
                    Color gc = blockColors[p.blockType];
                    gc.a = (unsigned char)(80 * (1.0f - progress));
                    DrawRectangleRounded(
                        (Rectangle){gx, gy, (float)cellS - 1, (float)cellS - 1},
                        0.25f, 4, gc);
                }
            }
        }
        for (int trail = 1; trail < (int)(p.dropEndY - p.dropStartY); trail++) {
            float trailAlpha = (1.0f - progress) * (1.0f - trail / (p.dropEndY - p.dropStartY)) * 100;
            if (trailAlpha < 5) continue;
            int trailY = (int)p.dropStartY + trail - 1;
            if (trailY >= p.posY) continue;
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    if (blocks[p.blockType][i][j] != ' ') {
                        Color tc = blockColors[p.blockType];
                        tc.a = (unsigned char)trailAlpha;
                        DrawRectangleRounded(
                            (Rectangle){(float)(boardX + (p.posX + j) * cellS + (int)shakeX),
                                        (float)(boardY + (trailY + i) * cellS + (int)shakeY),
                                        (float)cellS - 1, (float)cellS - 1},
                            0.25f, 4, tc);
                    }
                }
            }
        }
    }

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (p.currentBlock[i][j] != ' ') {
                float bx = (float)(boardX + (p.posX + j) * cellS + (int)shakeX);
                float by = (float)(boardY + (p.posY + i) * cellS + (int)shakeY);
                Color c = blockColors[p.blockType];
                DrawRectangleRounded(
                    (Rectangle){bx, by, (float)cellS - 1, (float)cellS - 1},
                    0.25f, 4, c);
                DrawRectangleRounded(
                    (Rectangle){bx + 2, by + 2, (float)cellS - 5, (float)cellS - 5},
                    0.2f, 4, Fade(WHITE, 0.15f));
            }
        }
    }

    for (int i = 0; i <= H; i++)
        DrawLine(boardX, boardY + i * cellS + (int)shakeY,
                 boardX + W * cellS, boardY + i * cellS + (int)shakeY,
                 Fade(DARKGRAY, 0.5f));
    for (int j = 0; j <= W; j++)
        DrawLine(boardX + j * cellS + (int)shakeX, boardY,
                 boardX + j * cellS + (int)shakeX, boardY + H * cellS,
                 Fade(DARKGRAY, 0.5f));
}

int main() {
    InitWindow(screenWidth, screenHeight, "Tetris Raylib - Square GUI");
    InitAudioDevice();
    Sound milestoneSounds[5];
    milestoneSounds[0] = LoadSound("audio/500diem_A01_R.wav");
    milestoneSounds[1] = LoadSound("audio/100diem_A01_R.wav");
    milestoneSounds[2] = LoadSound("audio/200diem_A01_R.wav");
    milestoneSounds[3] = LoadSound("audio/300diem_A01_R.wav");
    milestoneSounds[4] = LoadSound("audio/400diem_A01_R.wav");
    Music menuMusic = LoadMusicStream("audio/anhdochauphi.mp3");
    PlayMusicStream(menuMusic);
    Music gameMusic = LoadMusicStream("audio/tidalwave.mp3");
    float masterVolume = 1.0f;
    float baseMusicVol = 0.07f;
    float baseSoundVol = 3.0f;
    SetMusicVolume(menuMusic, masterVolume);
    SetMusicVolume(gameMusic, baseMusicVol * masterVolume);
    for (int i = 0; i < 5; i++)
        SetSoundVolume(milestoneSounds[i], baseSoundVol * masterVolume);
    SetTargetFPS(60);
    srand(time(0) + clock());

    initColors();
    initBoard();
    initBoard(p1);
    initBoard(p2);
    loadLeaderboard();
    for (int i = 0; i < 4; i++)
        rand();
    blockType = rand() % 7;
    nextBlockType = rand() % 7;
    spawnBlock();

    while (!WindowShouldClose()) {
        UpdateMusicStream(menuMusic);
        if (gameState == 2)
            UpdateMusicStream(gameMusic);
        if (gameState == 0) {
            Vector2 mp = GetMousePosition();
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                float bx = screenWidth / 2.0f - 130;
                if (mp.x >= bx && mp.x <= bx + 260) {
                    for (int i = 0; i < 4; i++) {
                        float by = 180.0f + i * 90.0f;
                        if (mp.y >= by && mp.y <= by + 60) {
                            if (i == 0) {
                                gameState = 2;
                                StopMusicStream(menuMusic);
                                PlayMusicStream(gameMusic);
                                initBoard();
                                score = 0;
                                gameTimer = 0;
                                nameCharCount = 0;
                                currentName[0] = '\0';
                                moveSpeed = 0.5f / pow(1.5f, difficulty);
                            }
                            if (i == 1) {
                                gameState = 6;
                                StopMusicStream(menuMusic);
                                PlayMusicStream(gameMusic);
                                initBoard(p1);
                                initBoard(p2);
                                p1.score = 0; p2.score = 0;
                                p1.gameTimer = 0; p2.gameTimer = 0;
                                p1.dropTimer = 0; p2.dropTimer = 0;
                                p1.moveSpeed = 0.5f / pow(1.5f, difficulty);
                                p2.moveSpeed = 0.5f / pow(1.5f, difficulty);
                                p1.isClearing = false; p2.isClearing = false;
                                p1.isDropping = false; p2.isDropping = false;
                                p1.blockType = rand() % 7;
                                p1.nextBlockType = rand() % 7;
                                p1.posX = (W / 2) - 2; p1.posY = 0;
                                spawnBlock(p1);
                                p2.blockType = rand() % 7;
                                p2.nextBlockType = rand() % 7;
                                p2.posX = (W / 2) - 2; p2.posY = 0;
                                spawnBlock(p2);
                            }
                            if (i == 2)
                                gameState = 4;
                            if (i == 3)
                                gameState = 5;
                        }
                    }
                }
            }
        }

        if (gameState == 1 || gameState == 3) {
            int key = GetCharPressed();
            while (key > 0) {
                if (key >= 32 && key <= 125 && nameCharCount < 30) {
                    currentName[nameCharCount] = (char)key;
                    nameCharCount++;
                    currentName[nameCharCount] = '\0';
                }
                key = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && nameCharCount > 0) {
                nameCharCount--;
                currentName[nameCharCount] = '\0';
            }
            if (IsKeyPressed(KEY_ENTER) && nameCharCount > 0) {
                if (gameState == 3) {
                    addLeaderboardEntry(difficulty, currentName, pendingScore);
                    saveLeaderboard();
                    gameState = 4;
                }
            }
            if (IsKeyPressed(KEY_Q)) {
                gameState = 0;
                PlayMusicStream(menuMusic);
            }
        }

        if (gameState == 4 && IsKeyPressed(KEY_Q)) {
            gameState = 0;
            PlayMusicStream(menuMusic);
        }

        if (gameState == 2) {
            if (IsKeyPressed(KEY_Q)) {
                gameState = 0;
                initBoard();
                score = 0;
                gameTimer = 0;
                PlayMusicStream(menuMusic);
                StopMusicStream(gameMusic);
            }
            if (!isClearing && !isDropping) {
                if (IsKeyPressed(KEY_A) && canMove(-1, 0))
                    posX--;
                if (IsKeyPressed(KEY_D) && canMove(1, 0))
                    posX++;
                if (IsKeyPressed(KEY_R))
                    rotateBlock();
                if (IsKeyPressed(KEY_X) && canMove(0, 1)) {
                    int target = posY;
                    while (canMove(0, target - posY + 1))
                        target++;
                    if (target > posY) {
                        isDropping = true;
                        dropStartY = posY;
                        dropEndY = target;
                        dropAnimTimer = 0.19f;
                    }
                }
            }

            float dt = GetFrameTime();
            gameTimer += dt;
            if (scorePopupTimer > 0)
                scorePopupTimer -= dt;
            if (acePopupTimer > 0)
                acePopupTimer -= dt;

            if (isDropping) {
                dropAnimTimer -= dt;
                float t = 1.0f - dropAnimTimer / 0.19f;
                t = t < 0 ? 0 : (t > 1 ? 1 : t);
                posY = (int)(dropStartY + (dropEndY - dropStartY) * t);
                if (dropAnimTimer <= 0) {
                    posY = (int)dropEndY;
                    isDropping = false;
                    block2Board();
                    clearingRowCount = 0;
                    for (int i = H - 2; i > 0; i--) {
                        int filledCount = 0;
                        for (int j = 1; j < W - 1; j++)
                            if (board[i][j] != ' ')
                                filledCount++;
                        if (filledCount == W - 2)
                            clearingRows[clearingRowCount++] = i;
                    }
                    if (clearingRowCount > 0) {
                        int ps = score;
                        int ns[] = {0, 100, 200, 300, 400};
                        for (int m = 0; m < 5; m++) {
                            int t =
                                ((ps + clearingRowCount * 100) / 500) * 500 +
                                ns[m];
                            if (t > 0 && ps < t &&
                                ps + clearingRowCount * 100 >= t)
                                PlaySound(milestoneSounds[m]);
                        }
                        isClearing = true;
                        clearTimer = 1.0f;
                        spawnParticles();
                    } else {
                        posX = (W / 2) - 2;
                        posY = 0;
                        blockType = nextBlockType;
                        nextBlockType = rand() % 7;
                        spawnBlock();
                        if (!canMove(0, 0)) {
                            while (GetCharPressed() > 0)
                                ;
                            pendingScore = score;
                            gameState = 3;
                            nameCharCount = 0;
                            currentName[0] = '\0';
                        }
                    }
                    dropTimer = 0;
                }
            }

            if (isClearing) {
                clearTimer -= dt;
                updateParticles(dt);
                if (clearTimer <= 0) {
                    for (int k = 0; k < clearingRowCount; k++)
                        for (int r = clearingRows[k]; r > 0; r--)
                            for (int j = 1; j < W - 1; j++)
                                board[r][j] = board[r - 1][j];
                    score += clearingRowCount * 100;
                    scorePopupValue = clearingRowCount * 100;
                    scorePopupTimer = 1.5f;
                    if (score > 0 &&
                        score / 500 > (score - clearingRowCount * 100) / 500)
                        acePopupTimer = 2.0f;
                    isClearing = false;
                    moveSpeed = max(0.05f, moveSpeed * 0.95f);
                    posX = (W / 2) - 2;
                    posY = 0;
                    blockType = nextBlockType;
                    nextBlockType = rand() % 7;
                    spawnBlock();
                    if (!canMove(0, 0)) {
                        pendingScore = score;
                        gameState = 3;
                        nameCharCount = 0;
                        currentName[0] = '\0';
                    }
                }
            } else {
                dropTimer += dt;
                if (dropTimer >= moveSpeed) {
                    if (canMove(0, 1))
                        posY++;
                    else {
                        block2Board();
                        clearingRowCount = 0;
                        for (int i = H - 2; i > 0; i--) {
                            int filledCount = 0;
                            for (int j = 1; j < W - 1; j++)
                                if (board[i][j] != ' ')
                                    filledCount++;
                            if (filledCount == W - 2)
                                clearingRows[clearingRowCount++] = i;
                        }
                        if (clearingRowCount > 0) {
                            int ps = score;
                            int ns[] = {0, 100, 200, 300, 400};
                            for (int m = 0; m < 5; m++) {
                                int t = ((ps + clearingRowCount * 100) / 500) *
                                            500 +
                                        ns[m];
                                if (t > 0 && ps < t &&
                                    ps + clearingRowCount * 100 >= t)
                                    PlaySound(milestoneSounds[m]);
                            }
                            isClearing = true;
                            clearTimer = 1.0f;
                            spawnParticles();
                        } else {
                            posX = (W / 2) - 2;
                            posY = 0;
                            blockType = nextBlockType;
                            nextBlockType = rand() % 7;
                            spawnBlock();
                            if (!canMove(0, 0)) {
                                pendingScore = score;
                                gameState = 3;
                                nameCharCount = 0;
                                currentName[0] = '\0';
                            }
                        }
                    }
                    dropTimer = 0;
                }
            }
        }

        if (gameState == 6) {
            if (IsKeyPressed(KEY_Q)) {
                gameState = 0;
                PlayMusicStream(menuMusic);
                StopMusicStream(gameMusic);
            }
            float dt = GetFrameTime();

            for (int side = 0; side < 2; side++) {
                Player &p = (side == 0) ? p1 : p2;
                if (p.isClearing || p.isDropping) continue;
                if (side == 0) {
                    if (IsKeyPressed(KEY_A) && canMove(p, -1, 0)) p.posX--;
                    if (IsKeyPressed(KEY_D) && canMove(p, 1, 0)) p.posX++;
                    if (IsKeyPressed(KEY_R)) rotateBlock(p);
                    if (IsKeyPressed(KEY_X) && canMove(p, 0, 1)) {
                        int target = p.posY;
                        while (canMove(p, 0, target - p.posY + 1)) target++;
                        if (target > p.posY) {
                            p.isDropping = true;
                            p.dropStartY = p.posY;
                            p.dropEndY = target;
                            p.dropAnimTimer = 0.19f;
                        }
                    }
                } else {
                    if (IsKeyPressed(KEY_LEFT) && canMove(p, -1, 0)) p.posX--;
                    if (IsKeyPressed(KEY_RIGHT) && canMove(p, 1, 0)) p.posX++;
                    if (IsKeyPressed(KEY_COMMA)) rotateBlock(p);
                    if (IsKeyPressed(KEY_PERIOD) && canMove(p, 0, 1)) {
                        int target = p.posY;
                        while (canMove(p, 0, target - p.posY + 1)) target++;
                        if (target > p.posY) {
                            p.isDropping = true;
                            p.dropStartY = p.posY;
                            p.dropEndY = target;
                            p.dropAnimTimer = 0.19f;
                        }
                    }
                }
            }

            p1.gameTimer += dt;
            p2.gameTimer += dt;
            p1.dropTimer += dt;
            p2.dropTimer += dt;
            if (p1.scorePopupTimer > 0) p1.scorePopupTimer -= dt;
            if (p2.scorePopupTimer > 0) p2.scorePopupTimer -= dt;
            if (p1.acePopupTimer > 0) p1.acePopupTimer -= dt;
            if (p2.acePopupTimer > 0) p2.acePopupTimer -= dt;

            for (int side = 0; side < 2; side++) {
                Player &p = (side == 0) ? p1 : p2;
                if (p.isDropping) {
                    p.dropAnimTimer -= dt;
                    float t = 1.0f - p.dropAnimTimer / 0.19f;
                    t = t < 0 ? 0 : (t > 1 ? 1 : t);
                    p.posY = (int)(p.dropStartY + (p.dropEndY - p.dropStartY) * t);
                    if (p.dropAnimTimer <= 0) {
                        p.posY = (int)p.dropEndY;
                        p.isDropping = false;
                        block2Board(p);
                        p.clearingRowCount = 0;
                        for (int i = H - 2; i > 0; i--) {
                            int filledCount = 0;
                            for (int j = 1; j < W - 1; j++)
                                if (p.board[i][j] != ' ') filledCount++;
                            if (filledCount == W - 2)
                                p.clearingRows[p.clearingRowCount++] = i;
                        }
                        if (p.clearingRowCount > 0) {
                            int ps = p.score;
                            int ns[] = {0, 100, 200, 300, 400};
                            for (int m = 0; m < 5; m++) {
                                int t = ((ps + p.clearingRowCount * 100) / 500) * 500 + ns[m];
                                if (t > 0 && ps < t && ps + p.clearingRowCount * 100 >= t)
                                    PlaySound(milestoneSounds[m]);
                            }
                            p.isClearing = true;
                            p.clearTimer = 1.0f;
                        } else {
                            p.posX = (W / 2) - 2; p.posY = 0;
                            p.blockType = p.nextBlockType;
                            p.nextBlockType = rand() % 7;
                            spawnBlock(p);
                            if (!canMove(p, 0, 0)) {
                                pendingScore = p.score;
                                gameState = 3;
                                nameCharCount = 0;
                                currentName[0] = '\0';
                            }
                        }
                        p.dropTimer = 0;
                    }
                }
            }

            for (int side = 0; side < 2; side++) {
                Player &p = (side == 0) ? p1 : p2;
                if (p.isClearing) {
                    p.clearTimer -= dt;
                    if (p.clearTimer <= 0) {
                        for (int k = 0; k < p.clearingRowCount; k++)
                            for (int r = p.clearingRows[k]; r > 0; r--)
                                for (int j = 1; j < W - 1; j++)
                                    p.board[r][j] = p.board[r - 1][j];
                        p.score += p.clearingRowCount * 100;
                        p.scorePopupValue = p.clearingRowCount * 100;
                        p.scorePopupTimer = 1.5f;
                        if (p.score > 0 && p.score / 500 > (p.score - p.clearingRowCount * 100) / 500)
                            p.acePopupTimer = 2.0f;
                        p.isClearing = false;
                        p.moveSpeed = max(0.05f, p.moveSpeed * 0.95f);
                        p.posX = (W / 2) - 2; p.posY = 0;
                        p.blockType = p.nextBlockType;
                        p.nextBlockType = rand() % 7;
                        spawnBlock(p);
                        if (!canMove(p, 0, 0)) {
                            pendingScore = p.score;
                            gameState = 3;
                            nameCharCount = 0;
                            currentName[0] = '\0';
                        }
                    }
                } else {
                    if (p.dropTimer >= p.moveSpeed) {
                        if (canMove(p, 0, 1)) p.posY++;
                        else {
                            block2Board(p);
                            p.clearingRowCount = 0;
                            for (int i = H - 2; i > 0; i--) {
                                int filledCount = 0;
                                for (int j = 1; j < W - 1; j++)
                                    if (p.board[i][j] != ' ') filledCount++;
                                if (filledCount == W - 2)
                                    p.clearingRows[p.clearingRowCount++] = i;
                            }
                            if (p.clearingRowCount > 0) {
                                int ps = p.score;
                                int ns[] = {0, 100, 200, 300, 400};
                                for (int m = 0; m < 5; m++) {
                                    int t = ((ps + p.clearingRowCount * 100) / 500) * 500 + ns[m];
                                    if (t > 0 && ps < t && ps + p.clearingRowCount * 100 >= t)
                                        PlaySound(milestoneSounds[m]);
                                }
                                p.isClearing = true;
                                p.clearTimer = 1.0f;
                            } else {
                                p.posX = (W / 2) - 2; p.posY = 0;
                                p.blockType = p.nextBlockType;
                                p.nextBlockType = rand() % 7;
                                spawnBlock(p);
                                if (!canMove(p, 0, 0)) {
                                    pendingScore = p.score;
                                    gameState = 3;
                                    nameCharCount = 0;
                                    currentName[0] = '\0';
                                }
                            }
                        }
                        p.dropTimer = 0;
                    }
                }
            }
        }

        float shakeX = 0, shakeY = 0;
        if (isClearing) {
            float intensity = clearTimer * 4;
            shakeX = (rand() % 200 - 100) * intensity / 100.0f;
            shakeY = (rand() % 200 - 100) * intensity / 100.0f;
        }

        BeginDrawing();
        ClearBackground(BLACK);

        if (gameState == 0) {
            float bx = screenWidth / 2.0f - 130;
            int tw = MeasureText("TETRIS", 70);
            DrawText("TETRIS", (screenWidth - tw) / 2, 70, 70, RAYWHITE);
            Color titleShadow = Fade(RAYWHITE, 0.15f);
            DrawText("TETRIS", (screenWidth - tw) / 2 + 3, 73, 70, titleShadow);
            const char *items[] = {"Single Player", "2-Players", "Leaderboard",
                                   "Settings"};
            for (int i = 0; i < 4; i++) {
                float by = 180.0f + i * 90.0f;
                Rectangle rec = {bx, by, 260, 60};
                bool hover = CheckCollisionPointRec(GetMousePosition(), rec);
                Color bg, border;
                if (hover) {
                    bg = (Color){0x4A, 0x6F, 0xE0, 0xFF};
                    border = (Color){0x6C, 0x8E, 0xF0, 0xFF};
                } else {
                    bg = (Color){0x2C, 0x2C, 0x3E, 0xCC};
                    border = (Color){0x4A, 0x4A, 0x5E, 0xAA};
                }
                DrawRectangleRounded(rec, 0.3f, 6, bg);
                DrawRectangleRoundedLines(rec, 0.3f, 6, border);
                int tw2 = MeasureText(items[i], 22);
                Color textColor =
                    hover ? RAYWHITE : (Color){0xCC, 0xCC, 0xDD, 0xFF};
                DrawText(items[i], (int)(bx + (260 - tw2) / 2), (int)by + 17,
                         22, textColor);
            }
            EndDrawing();
            continue;
        }

        if (gameState == 4) {
            DrawText("LEADERBOARD",
                     (screenWidth - MeasureText("LEADERBOARD", 50)) / 2, 40, 50,
                     RAYWHITE);

            int tabY = 100;
            int tabW = 110;
            int tabH = 30;
            int totalTabW = 5 * tabW + 4 * 6;
            int tabStartX = (screenWidth - totalTabW) / 2;
            for (int d = 0; d < 5; d++) {
                int tx = tabStartX + d * (tabW + 6);
                Rectangle tr = {(float)tx, (float)tabY, (float)tabW,
                                (float)tabH};
                bool hover = CheckCollisionPointRec(GetMousePosition(), tr);
                if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                    leaderboardDiff = d;
                Color tbg = (d == leaderboardDiff)
                                ? (Color){0x4A, 0x6F, 0xE0, 0xFF}
                                : (Color){0x2C, 0x2C, 0x3E, 0xCC};
                Color tbrd = (d == leaderboardDiff)
                                 ? (Color){0x6C, 0x8E, 0xF0, 0xFF}
                                 : (Color){0x4A, 0x4A, 0x5E, 0xAA};
                DrawRectangleRounded(tr, 0.3f, 4, tbg);
                DrawRectangleRoundedLines(tr, 0.3f, 4, tbrd);
                int dtw = MeasureText(diffNames[d], 16);
                DrawText(diffNames[d], tx + (tabW - dtw) / 2, tabY + 7, 16,
                         RAYWHITE);
            }

            int startY = 150;
            int nameX = screenWidth / 2 - 60;
            int ec = entryCount[leaderboardDiff];
            for (int i = 0; i < ec && i < 10; i++) {
                Color c = (i == 0) ? GOLD
                                   : ((i == 1) ? LIGHTGRAY
                                               : ((i == 2) ? ORANGE : GRAY));
                int y = startY + i * 40;
                int rankX = screenWidth / 2 - 180;
                if (i == 0) {
                    DrawCircle(rankX + 12, y + 12, 12, GOLD);
                    DrawText("1", rankX + 9, y + 5, 14, BLACK);
                } else if (i == 1) {
                    DrawCircle(rankX + 12, y + 12, 12, LIGHTGRAY);
                    DrawText("2", rankX + 9, y + 5, 14, BLACK);
                } else if (i == 2) {
                    DrawCircle(rankX + 12, y + 12, 12, ORANGE);
                    DrawText("3", rankX + 9, y + 5, 14, BLACK);
                } else {
                    char rankStr[8];
                    snprintf(rankStr, sizeof(rankStr), "%d.", i + 1);
                    DrawText(rankStr, rankX, y, 24, c);
                }
                char line[64];
                snprintf(line, sizeof(line), "%s - %d",
                         playerNames[leaderboardDiff][i],
                         playerScores[leaderboardDiff][i]);
                DrawText(line, nameX, y, 24, c);
            }
            if (ec == 0) {
                int lw = MeasureText("No scores yet!", 24);
                DrawText("No scores yet!", (screenWidth - lw) / 2, startY, 24,
                         GRAY);
            }
            if (IsKeyPressed(KEY_Q)) {
                gameState = 0;
                PlayMusicStream(menuMusic);
            }
            const char *hint2 = "Press Q to return to menu";
            int hw2 = MeasureText(hint2, 16);
            DrawText(hint2, (screenWidth - hw2) / 2, startY + 10 * 40 + 20, 16,
                     GRAY);
            EndDrawing();
            continue;
        }

        if (gameState == 5) {
            DrawText("SETTINGS",
                     (screenWidth - MeasureText("SETTINGS", 50)) / 2, 50, 50,
                     RAYWHITE);

            int sliderY = 200;
            int sliderW = 300;
            int sliderH = 12;
            int sliderX = (screenWidth - sliderW) / 2;

            Rectangle track = {(float)sliderX, (float)sliderY, (float)sliderW,
                               (float)sliderH};
            Vector2 mp = GetMousePosition();
            bool onSlider = CheckCollisionPointRec(mp, track);
            if (onSlider && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                masterVolume = (mp.x - sliderX) / sliderW;
                if (masterVolume < 0)
                    masterVolume = 0;
                if (masterVolume > 1)
                    masterVolume = 1;
                SetMusicVolume(menuMusic, masterVolume);
                SetMusicVolume(gameMusic, baseMusicVol * masterVolume);
                for (int i = 0; i < 5; i++)
                    SetSoundVolume(milestoneSounds[i],
                                   baseSoundVol * masterVolume);
            }

            DrawRectangleRounded(track, 0.3f, 4,
                                 (Color){0x4A, 0x4A, 0x5E, 0xAA});
            float fillW = masterVolume * sliderW;
            if (fillW > 0) {
                Rectangle fill = {(float)sliderX, (float)sliderY, fillW,
                                  (float)sliderH};
                DrawRectangleRounded(fill, 0.3f, 4,
                                     (Color){0x4A, 0x6F, 0xE0, 0xFF});
            }

            int knobX = sliderX + (int)(masterVolume * sliderW);
            DrawCircle(knobX, sliderY + sliderH / 2, 10, RAYWHITE);

            const char *volLabel = "Volume";
            int vlw = MeasureText(volLabel, 20);
            DrawText(volLabel, (screenWidth - vlw) / 2, sliderY - 40, 20,
                     RAYWHITE);

            char volPct[8];
            snprintf(volPct, sizeof(volPct), "%d%%", (int)(masterVolume * 100));
            int vpw = MeasureText(volPct, 16);
            DrawText(volPct, (screenWidth - vpw) / 2, sliderY + 30, 16, GRAY);

            int diffY = 300;
            DrawText("Difficulty",
                     (screenWidth - MeasureText("Difficulty", 20)) / 2,
                     diffY - 30, 20, RAYWHITE);
            int diffBtnW = 110;
            int diffBtnH = 35;
            int totalW = 5 * diffBtnW + 4 * 8;
            int diffStartX = (screenWidth - totalW) / 2;
            for (int d = 0; d < 5; d++) {
                int dx = diffStartX + d * (diffBtnW + 8);
                Rectangle dr = {(float)dx, (float)diffY, (float)diffBtnW,
                                (float)diffBtnH};
                bool hover = CheckCollisionPointRec(GetMousePosition(), dr);
                if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                    difficulty = d;
                Color dbg = (d == difficulty) ? (Color){0x4A, 0x6F, 0xE0, 0xFF}
                                              : (Color){0x2C, 0x2C, 0x3E, 0xCC};
                Color dbrd = (d == difficulty)
                                 ? (Color){0x6C, 0x8E, 0xF0, 0xFF}
                                 : (Color){0x4A, 0x4A, 0x5E, 0xAA};
                DrawRectangleRounded(dr, 0.3f, 4, dbg);
                DrawRectangleRoundedLines(dr, 0.3f, 4, dbrd);
                int dtw = MeasureText(diffNames[d], 16);
                DrawText(diffNames[d], dx + (diffBtnW - dtw) / 2, diffY + 9, 16,
                         RAYWHITE);
            }

            const char *credit = "This game made by Thinh, Minh, Son";
            int cw = MeasureText(credit, 14);
            DrawText(credit, (screenWidth - cw) / 2, 430, 14,
                     (Color){0x88, 0x88, 0xAA, 0xFF});

            if (IsKeyPressed(KEY_Q)) {
                gameState = 0;
                PlayMusicStream(menuMusic);
            }
            const char *hint2 = "Press Q to return to menu";
            int hw2 = MeasureText(hint2, 16);
            DrawText(hint2, (screenWidth - hw2) / 2, 500, 16, GRAY);
            EndDrawing();
            continue;
        }

        if (gameState == 3) {
            DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.6f));
            const char *prompt = "Game Over! Enter your name:";
            int pw = MeasureText(prompt, 30);
            DrawText(prompt, (screenWidth - pw) / 2, 200, 30, RAYWHITE);
            Rectangle inputBox = {(float)(screenWidth / 2 - 150), 260, 300, 50};
            DrawRectangleRounded(inputBox, 0.2f, 4,
                                 (Color){0x4A, 0x6F, 0xE0, 0xAA});
            DrawRectangleRoundedLines(inputBox, 0.2f, 4,
                                      (Color){0x6C, 0x8E, 0xF0, 0xFF});
            int cw = MeasureText(currentName, 28);
            DrawText(currentName, screenWidth / 2 - cw / 2, 270, 28, RAYWHITE);
            if ((int)(GetTime() * 2) % 2 == 0) {
                int cursorX = screenWidth / 2 + cw / 2 + 2;
                DrawText("|", cursorX, 270, 28, RAYWHITE);
            }
            const char *hint = "Press ENTER to confirm, Q to go back";
            int hw = MeasureText(hint, 16);
            DrawText(hint, (screenWidth - hw) / 2, 330, 16, GRAY);
            EndDrawing();
            continue;
        }

        if (gameState == 6) {
            int cellS2 = 13;
            int bw = W * cellS2;
            int bh = H * cellS2;
            int gap = 20;
            int totalW = 2 * bw + gap;
            int board1X = (screenWidth - totalW) / 2;
            int board2X = board1X + bw + gap;
            int boardY = 60;

            int shake1X = 0, shake1Y = 0, shake2X = 0, shake2Y = 0;
            if (p1.isClearing) {
                float intensity = p1.clearTimer * 4;
                shake1X = (rand() % 200 - 100) * intensity / 100.0f;
                shake1Y = (rand() % 200 - 100) * intensity / 100.0f;
            }
            if (p2.isClearing) {
                float intensity = p2.clearTimer * 4;
                shake2X = (rand() % 200 - 100) * intensity / 100.0f;
                shake2Y = (rand() % 200 - 100) * intensity / 100.0f;
            }

            DrawText("Player 1", board1X + bw / 2 - MeasureText("Player 1", 16) / 2,
                     boardY - 25, 16, RAYWHITE);
            DrawText("Player 2", board2X + bw / 2 - MeasureText("Player 2", 16) / 2,
                     boardY - 25, 16, RAYWHITE);

            drawPlayerBoard(p1, board1X, boardY, cellS2, shake1X, shake1Y);
            drawPlayerBoard(p2, board2X, boardY, cellS2, shake2X, shake2Y);

            int infoY = boardY + bh + 10;
            DrawText(TextFormat("Score: %d", p1.score), board1X, infoY, 16, YELLOW);
            DrawText(TextFormat("Time: %02d:%02d", (int)p1.gameTimer / 60, (int)p1.gameTimer % 60),
                     board1X, infoY + 20, 14, GREEN);
            DrawText(TextFormat("Score: %d", p2.score), board2X, infoY, 16, YELLOW);
            DrawText(TextFormat("Time: %02d:%02d", (int)p2.gameTimer / 60, (int)p2.gameTimer % 60),
                     board2X, infoY + 20, 14, GREEN);

            DrawText("P1: A/D R X", board1X, infoY + 45, 12, GRAY);
            DrawText("P2: < > , .", board2X, infoY + 45, 12, GRAY);

            EndDrawing();
            continue;
        }

        float hue = fmod(clearTimer * 480, 360);

        for (int i = 0; i < H; i++) {
            for (int j = 0; j < W; j++) {
                int rx = j * cellSize + (int)shakeX;
                int ry = i * cellSize + (int)shakeY;
                if (board[i][j] == '#')
                    DrawRectangle(rx, ry, cellSize - 1, cellSize - 1, DARKGRAY);
                else if (board[i][j] != ' ') {
                    bool isFlashRow = false;
                    for (int k = 0; k < clearingRowCount; k++)
                        if (clearingRows[k] == i) {
                            isFlashRow = true;
                            break;
                        }
                    Color blockColor;
                    if (isFlashRow && isClearing)
                        blockColor = ColorFromHSV(hue + i * 30, 1, 1);
                    else if (board[i][j] >= '0' && board[i][j] <= '6')
                        blockColor = blockColors[board[i][j] - '0'];
                    else
                        blockColor = BLUE;
                    DrawRectangleRounded((Rectangle){(float)rx, (float)ry,
                                                     cellSize - 1,
                                                     cellSize - 1},
                                         0.25f, 4, blockColor);
                    DrawRectangleRounded(
                        (Rectangle){(float)rx + 2, (float)ry + 2, cellSize - 5,
                                    cellSize - 5},
                        0.2f, 4, Fade(WHITE, 0.12f));
                }
            }
        }

        if (isClearing) {
            for (int k = 0; k < clearingRowCount; k++) {
                int row = clearingRows[k];
                float glowSize = (1.0f - clearTimer) * 20;
                for (int g = 0; g < 3; g++) {
                    float s = glowSize + g * 4;
                    Color glow = ColorFromHSV(hue + row * 30, 1, 1);
                    glow.a =
                        (unsigned char)((60 - g * 15) * (1.0f - clearTimer));
                    DrawRectangle((int)(1 * cellSize - s + shakeX),
                                  (int)(row * cellSize - s + shakeY),
                                  (int)((W - 2) * cellSize + s * 2),
                                  (int)(cellSize + s * 2), glow);
                }
            }
        }

        if (isDropping) {
            float progress = 1.0f - dropAnimTimer / 0.19f;
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    if (blocks[blockType][i][j] != ' ') {
                        float gx = (float)((posX + j) * cellSize + (int)shakeX);
                        float gy = (float)(((int)dropEndY + i) * cellSize +
                                        (int)shakeY);
                        Color gc = blockColors[blockType];
                        gc.a = (unsigned char)(80 * (1.0f - progress));
                        DrawRectangleRounded(
                            (Rectangle){gx, gy, cellSize - 1, cellSize - 1},
                            0.25f, 4, gc);
                    }
                }
            }
            for (int trail = 1; trail < (int)(dropEndY - dropStartY); trail++) {
                float trailAlpha = (1.0f - progress) *
                                   (1.0f - trail / (dropEndY - dropStartY)) *100;
                if (trailAlpha < 5)
                    continue;
                int trailY = (int)dropStartY + trail - 1;
                if (trailY >= posY)
                    continue;
                for (int i = 0; i < 4; i++) {
                    for (int j = 0; j < 4; j++) {
                        if (blocks[blockType][i][j] != ' ') {
                            Color tc = blockColors[blockType];
                            tc.a = (unsigned char)trailAlpha;
                            DrawRectangleRounded(
                                (Rectangle){(float)((posX + j) * cellSize +
                                                    (int)shakeX),
                                            (float)((trailY + i) * cellSize +
                                                    (int)shakeY),
                                            cellSize - 1, cellSize - 1},
                                0.25f, 4, tc);
                        }
                    }
                }
            }
        }

        if (!isDropping) {
            int ghostY = posY;
            while (canMove(0, ghostY - posY + 1))
                ghostY++;
            if (ghostY > posY) {
                for (int i = 0; i < 4; i++) {
                    for (int j = 0; j < 4; j++) {
                        if (currentBlock[i][j] != ' ') {
                            float gx =
                                (float)((posX + j) * cellSize + (int)shakeX);
                            float gy =
                                (float)((ghostY + i) * cellSize + (int)shakeY);
                            Color gc = blockColors[blockType];
                            gc.a = 60;
                            DrawRectangleRounded(
                                (Rectangle){gx, gy, cellSize - 1, cellSize - 1},
                                0.25f, 4, gc);
                        }
                    }
                }
            }
        }

        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (currentBlock[i][j] != ' ') {
                    float bx = (float)((posX + j) * cellSize + (int)shakeX);
                    float by = (float)((posY + i) * cellSize + (int)shakeY);
                    Color c = blockColors[blockType];
                    DrawRectangleRounded(
                        (Rectangle){bx, by, cellSize - 1, cellSize - 1}, 0.25f,
                        4, c);
                    DrawRectangleRounded(
                        (Rectangle){bx + 2, by + 2, cellSize - 5, cellSize - 5},
                        0.2f, 4, Fade(WHITE, 0.15f));
                }
            }
        }

        for (int i = 0; i <= H; i++)
            DrawLine(0, i * cellSize + (int)shakeY, W * cellSize,
                     i * cellSize + (int)shakeY, Fade(DARKGRAY, 0.5f));
        for (int j = 0; j <= W; j++)
            DrawLine(j * cellSize + (int)shakeX, 0, j * cellSize + (int)shakeX,
                     H * cellSize, Fade(DARKGRAY, 0.5f));

        drawParticles();

        if (isClearing) {
            float progress = 1.0f - clearTimer;
            float alpha = progress < 0.2f ? progress / 0.2f : 1.0f;
            if (progress > 0.7f)
                alpha = 1.0f - (progress - 0.7f) / 0.3f;
            int fontSize = 40 + (int)(progress * 60);
            const char *label = TextFormat("+%d", clearingRowCount * 100);
            int tw = MeasureText(label, fontSize);
            int cx = (W * cellSize - tw) / 2;
            int cy = (H * cellSize - fontSize) / 2;
            Color tc = ColorFromHSV(hue, 1, 1);
            tc.a = (unsigned char)(alpha * 255);
            DrawText(label, cx + (int)shakeX, cy + (int)shakeY - 30, fontSize,
                     tc);

            if (clearingRowCount >= 3) {
                Color flash = Fade(WHITE, 0.3f * alpha);
                DrawRectangle(0, 0, W * cellSize, H * cellSize, flash);
            }
        }

        if (acePopupTimer > 0) {
            float progress = 1.0f - acePopupTimer / 2.0f;
            float alpha = progress < 0.15f ? progress / 0.15f : 1.0f;
            if (progress > 0.8f)
                alpha = 1.0f - (progress - 0.8f) / 0.2f;
            int fontSize = 50 + (int)(progress * 80);
            const char *aceText = "ACE";
            int tw = MeasureText(aceText, fontSize);
            int cx = (W * cellSize - tw) / 2;
            int cy = (H * cellSize - fontSize) / 2;
            Color ac = ColorFromHSV(progress * 720, 1, 1);
            ac.a = (unsigned char)(alpha * 255);
            DrawText(aceText, cx + (int)shakeX, cy + (int)shakeY - 30, fontSize,
                     ac);
        }

        int sidebarX = W * cellSize + 20;
        DrawText("TETRIS", sidebarX, 20, 40, RAYWHITE);

        DrawText("SCORE", sidebarX, 100, 20, LIGHTGRAY);
        DrawText(TextFormat("%06d", score), sidebarX, 125, 30, YELLOW);

        if (scorePopupTimer > 0) {
            float a = scorePopupTimer / 1.5f;
            int offsetY = (int)((1.0f - a) * 40);
            Color c = YELLOW;
            c.a = (unsigned char)(a * 255);
            DrawText(TextFormat("+%d", scorePopupValue), sidebarX,
                     125 - offsetY, 25, c);
        }

        DrawText("TIME", sidebarX, 200, 20, LIGHTGRAY);
        DrawText(
            TextFormat("%02d:%02d", (int)gameTimer / 60, (int)gameTimer % 60),
            sidebarX, 225, 30, GREEN);

        DrawText("NEXT", sidebarX, 300, 20, LIGHTGRAY);
        DrawRectangleRounded((Rectangle){(float)sidebarX, 330, 120, 120}, 0.15f,
                             4, Fade(DARKGRAY, 0.3f));
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (blocks[nextBlockType][i][j] != ' ') {
                    float nb = (float)(sidebarX + j * 25 + 10);
                    float nby = (float)(330 + i * 25 + 10);
                    Color c = blockColors[nextBlockType];
                    DrawRectangleRounded((Rectangle){nb, nby, 23, 23}, 0.25f, 4,
                                         c);
                    DrawRectangleRounded(
                        (Rectangle){nb + 1.5f, nby + 1.5f, 20, 20}, 0.2f, 4,
                        Fade(WHITE, 0.12f));
                }
            }
        }

        DrawText("A/D: Move", sidebarX, 480, 15, GRAY);
        DrawText("X: Drop", sidebarX, 500, 15, GRAY);
        DrawText("R: Rotate", sidebarX, 520, 15, GRAY);

        EndDrawing();
    }

    for (int i = 0; i < 5; i++)
        UnloadSound(milestoneSounds[i]);
    UnloadMusicStream(menuMusic);
    UnloadMusicStream(gameMusic);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
