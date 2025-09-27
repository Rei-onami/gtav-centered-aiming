#include "inc/main.h"
#include "inc/natives.h"
#include "inc/types.h"
#include "inc/enums.h"
#include <chrono>
#include <math.h>
#include <string>
#include <sstream>
#include <algorithm>
#include <cstdio>

// ---------- Глобалы ---------- ver 1



//bool showUI = true;
bool modEnabled = true;
int controlSwapCam = 51; // кнопка для спавна/удаления блока камеры
const char* camBlockModelName = "prop_tv_flat_01_screen";  //"prop_barrel_03a"; prop_fncwood_16c prop_box_tea01a prop_cs_rub_box_01
float camBlockOffsetX = 0.32f; // смещение справа от игрока
float camBlockOffsetY = 0.79f; //вверх
float camBlockOffsetZ = 0.0f;// смещение вперед
float offsetZ = 0.79f; //вверх
////кнопки
//int toggleKey = 0x23; // VK_END
//int insertKey = 0x2D; // VK_INSERT
//int leftKey = 0x25; // VK_LEFT
//int rightKey = 0x27; // VK_RIGHT
//int shiftKey = 0x10; // VK_SHIFT
//int upKey = 0x26; // VK_RIGHT
//int downKey = 0x28; // VK_SHIFT
//
//bool IsKeyJustPressed(int vkKey) {
//    static SHORT lastState[256] = { 0 };
//    SHORT state = GetAsyncKeyState(vkKey);
//    bool pressed = (state & 0x8000) != 0;
//    bool justPressed = pressed && !(lastState[vkKey] & 0x8000);
//    lastState[vkKey] = state;
//    return justPressed;
//}
//bool IsKeyDown(int key) {
//    return GetAsyncKeyState(key) & 0x8000;
//}
//bool IsKeyJustUp(int key) {
//    return GetAsyncKeyState(key) & 0x8000;
//}
////кнопки
// текущее смещение по X (будет анимироваться к camBlockOffsetX)
float currentOffsetX = 0.0f;
float startOffsetX = 0.0f;

int moveDurationTime = 800; // 1 секунда анимации
auto moveStartTime = std::chrono::steady_clock::now();
const auto moveDuration = std::chrono::milliseconds(moveDurationTime);
// стартовое смещение
float camBlockOffsetXdistance = 0.4f;
Entity camBlockEntity = 0;
bool camBlockActive = false;
float headingAngle = 90.0f;

bool RequestModelByName(const char* modelName, Hash* outHash) {
    Hash modelHash = GAMEPLAY::GET_HASH_KEY((char*)modelName);
    *outHash = modelHash;
    if (!STREAMING::HAS_MODEL_LOADED(modelHash)) {
        STREAMING::REQUEST_MODEL(modelHash);
        int tries = 0;
        while (!STREAMING::HAS_MODEL_LOADED(modelHash) && tries < 200) {
            WAIT(0);
            tries++;
        }
    }
    return STREAMING::HAS_MODEL_LOADED(modelHash);
}

Entity CreateCamBlock(Ped player) {
    Hash modelHash;
    if (!RequestModelByName(camBlockModelName, &modelHash)) return 0;

    // стартовое смещение: целевое + 2.0f
    startOffsetX = camBlockOffsetX + camBlockOffsetXdistance;
    currentOffsetX = startOffsetX;
    moveStartTime = std::chrono::steady_clock::now();

    Vector3 offset = { currentOffsetX, camBlockOffsetY, camBlockOffsetZ };
    Vector3 spawn = ENTITY::GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS(player, offset.x, offset.y, offset.z);

    Entity obj = OBJECT::CREATE_OBJECT(modelHash, spawn.x, spawn.y, spawn.z + offsetZ, TRUE, TRUE, FALSE);
    if (!ENTITY::DOES_ENTITY_EXIST(obj)) return 0;

    ENTITY::SET_ENTITY_INVINCIBLE(obj, TRUE);

    // 🔹 делаем объект невидимым
    ENTITY::SET_ENTITY_VISIBLE(obj, FALSE, FALSE);



    // 🔹 включаем коллизию только для камеры, но НЕ для физики педов/машин
    ENTITY::SET_ENTITY_COLLISION(obj, TRUE, FALSE);

    // 🔹 игнорируем столкновения с игроком
    ENTITY::SET_ENTITY_NO_COLLISION_ENTITY(obj, player, FALSE);

    ENTITY::FREEZE_ENTITY_POSITION(obj, TRUE);

    camBlockEntity = obj;
    camBlockActive = true;
    return obj;
}

void RemoveCamBlock() {
    if (camBlockEntity != 0 && ENTITY::DOES_ENTITY_EXIST(camBlockEntity)) {
        ENTITY::SET_ENTITY_INVINCIBLE(camBlockEntity, FALSE);
        ENTITY::SET_ENTITY_AS_MISSION_ENTITY(camBlockEntity, true, true);
        ENTITY::DELETE_ENTITY(&camBlockEntity);
    }
    camBlockEntity = 0;
    camBlockActive = false;
}


void UpdateCamBlockPosition(Ped player) {
    if (!camBlockActive || camBlockEntity == 0) return;

    auto now = std::chrono::steady_clock::now();
    float t = std::chrono::duration<float>(now - moveStartTime).count() /
        std::chrono::duration<float>(moveDuration).count();
    if (t > 1.0f) t = 1.0f;

    currentOffsetX = startOffsetX + (camBlockOffsetX - startOffsetX) * t;

    Vector3 offset = { currentOffsetX, camBlockOffsetY, camBlockOffsetZ };
    Vector3 pos = ENTITY::GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS(player, offset.x, offset.y, offset.z + offsetZ);

    ENTITY::SET_ENTITY_COORDS_NO_OFFSET(camBlockEntity, pos.x, pos.y, pos.z, TRUE, TRUE, TRUE);

    float heading = ENTITY::GET_ENTITY_HEADING(player);
    ENTITY::SET_ENTITY_HEADING(camBlockEntity, heading + headingAngle);
}


void ScriptMain() {
    while (true) {
        WAIT(0);
        if (!modEnabled) continue;

        Ped player = PLAYER::PLAYER_PED_ID();
        if (!ENTITY::DOES_ENTITY_EXIST(player)) continue;
        if (PED::IS_PED_IN_ANY_VEHICLE(player, FALSE) ||
            PED::IS_PED_RAGDOLL(player) ||
            PED::IS_PED_SWIMMING(player) ||
            PLAYER::IS_PLAYER_DEAD(PLAYER::PLAYER_ID())) continue;

        bool isAiming = PLAYER::IS_PLAYER_FREE_AIMING(PLAYER::PLAYER_ID());

        // кнопка спавна/удаления блока камеры
        if (isAiming && CONTROLS::IS_CONTROL_JUST_PRESSED(0, controlSwapCam)) {
            if (!camBlockActive) {
                CreateCamBlock(player);
            }
            else {
                RemoveCamBlock();
            }
        }

        // 🔹 если целиться перестали, убираем объект автоматически
        if (!isAiming && camBlockActive) {
            RemoveCamBlock();
        }

        // управление смещением блока камеры
        if (camBlockActive) {
            // вправо/влево
            //if (IsKeyDown(rightKey)) { // VK_RIGHT
            //    camBlockOffsetX += 0.01f;
            //}
            //if (IsKeyDown(leftKey)) { // VK_LEFT
            //    camBlockOffsetX -= 0.01f;
            //}

            //// вперед/назад
            //if (IsKeyDown(upKey)) { // VK_UP
            //    offsetZ += 0.01f;
            //}
            //if (IsKeyDown(downKey)) { // VK_DOWN
            //    offsetZ -= 0.01f;
            //}

            // обновляем позицию с учётом новых смещений
            UpdateCamBlockPosition(player);
        }
    }
}


// Стандартная точка входа ASI-плагина
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        scriptRegister(hModule, ScriptMain);
        break;
    case DLL_PROCESS_DETACH:
        scriptUnregister(hModule);
        break;
    }
    return TRUE;
}