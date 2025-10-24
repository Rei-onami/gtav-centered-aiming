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
static std::string iniPath; // <- полный путь к INI



bool modEnabled = true;
int controlSwapCam = 45; // reload кнопка для спавна/удаления блока камеры 51 174
float headingAngle = 55.0f; // 90 поворот вокруг Z (горизонтальный)74 100 много 65 ок
float pitchAngle = 80.0f; // поворот вокруг X (наклон вертикальный)90залезает 75незалезает

const char* camBlockModelName = "p_cs_laptop_02_w";  //"prop_barrel_03a"; prop_fncwood_16c prop_box_tea01a prop_cs_rub_box_01
float camBlockOffsetX = 0.32f; // смещение справа от игрока 0,4
float camBlockOffsetY = 0.75f; //вверх
float camBlockOffsetZ = -0.045f;// смещение вперед 00 0.5много 0,04 мало
float offsetZ = 0.66f; //вверх0.79f ,68 69 67
//float currentOffsetX = 0.0f;
//float startOffsetX = 0.0f;

//int moveDurationTime = 300; // 1 секунда анимации
//auto moveStartTime = std::chrono::steady_clock::now();
//const auto moveDuration = std::chrono::milliseconds(moveDurationTime);
// стартовое смещение
float camBlockOffsetXdistance = 0.4f;
Entity camBlockEntity = 0;
bool camBlockActive = false;

////кнопки
//int toggleKey = 0x23; // VK_END
//int insertKey = 0x2D; // VK_INSERT
//int leftKey = 0x25; // VK_LEFT
//int rightKey = 0x27; // VK_RIGHT
//int shiftKey = 0x10; // VK_SHIFT
//int upKey = 0x26; // VK_RIGHT
//int downKey = 0x28; // VK_SHIFT

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
//кнопки
// 
// 
// текущее смещение по X (будет анимироваться к camBlockOffsetX)

// --- Служебные утилиты для INI ---
static inline void trim(std::string& s) {
    auto notspace = [](int ch) { return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
}
static inline void strip_comment(std::string& s) {
    size_t p = s.find_first_of(";#");
    if (p != std::string::npos) s.erase(p);
}
static inline void comma_to_dot(std::string& s) {
    std::replace(s.begin(), s.end(), ',', '.');
}
static float ReadFloat(const char* section, const char* key, float def) {
    char buf[256] = { 0 };
    GetPrivateProfileStringA(section, key, "", buf, sizeof(buf), iniPath.c_str());
    if (buf[0] == '\0') return def;
    std::string v(buf);
    strip_comment(v);
    trim(v);
    comma_to_dot(v); // локали RU/PL
    if (v.empty()) return def;
    char* endp = nullptr;
    float f = strtof(v.c_str(), &endp);
    if (!endp || endp == v.c_str()) return def; // не распарсилось
    return f;
}
static int ReadInt(const char* section, const char* key, int def) {
    return GetPrivateProfileIntA(section, key, def, iniPath.c_str());
}
static bool ReadBool(const char* section, const char* key, bool def) {
    // допускаем 0/1, true/false, on/off (регистронезависимо)
    char buf[64] = { 0 };
    GetPrivateProfileStringA(section, key, "", buf, sizeof(buf), iniPath.c_str());
    if (buf[0] == '\0') return def;
    std::string v(buf);
    strip_comment(v); trim(v);
    std::transform(v.begin(), v.end(), v.begin(), ::tolower);
    if (v == "1" || v == "true" || v == "on" || v == "yes") return true;
    if (v == "0" || v == "false" || v == "off" || v == "no")  return false;
    // если написали странность — вернём дефолт
    return def;
}

// --- Загрузка настроек из INI ---
void LoadSettings() {
    // Булевые/инт

    // Float’ы
    camBlockOffsetX = ReadFloat("OFFSET", "camBlockOffsetX", camBlockOffsetX);
    camBlockOffsetY = ReadFloat("OFFSET", "camBlockOffsetY", camBlockOffsetY);
    camBlockOffsetZ = ReadFloat("OFFSET", "camBlockOffsetZ", camBlockOffsetZ);
    offsetZ = ReadFloat("OFFSET", "offsetZ", offsetZ);
    camBlockOffsetXdistance = ReadFloat("OFFSET", "camBlockOffsetXdistance", camBlockOffsetXdistance);
    //moveDurationTime = ReadInt("OFFSET", "moveDurationTime", moveDurationTime);
    headingAngle = ReadFloat("OFFSET", "headingAngle", headingAngle);

}
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
    //startOffsetX = camBlockOffsetX + camBlockOffsetXdistance;
    //currentOffsetX = startOffsetX;
    //moveStartTime = std::chrono::steady_clock::now();

    Vector3 offset = { camBlockOffsetX, camBlockOffsetY, camBlockOffsetZ };
    Vector3 spawn = ENTITY::GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS(player, offset.x, offset.y, offset.z);

    Entity obj = OBJECT::CREATE_OBJECT(modelHash, spawn.x, spawn.y, spawn.z + offsetZ, TRUE, TRUE, FALSE);
    if (!ENTITY::DOES_ENTITY_EXIST(obj)) return 0;

    ENTITY::SET_ENTITY_INVINCIBLE(obj, TRUE);
    // 🔹 делаем объект невидимым
    ENTITY::SET_ENTITY_VISIBLE(obj, FALSE, FALSE);
    // 🔹 включаем коллизию только для камеры, но НЕ для физики педов/машин
    //ENTITY::SET_ENTITY_COLLISION(obj, TRUE, FALSE камера блокается, стены клипуют); (FALSE FALSE камера клипует все клипует)
    ENTITY::SET_ENTITY_COLLISION(obj, TRUE, TRUE);
    // 🔹 игнорируем столкновения с игроком
    ENTITY::SET_ENTITY_NO_COLLISION_ENTITY(obj, player, TRUE);
    ENTITY::SET_ENTITY_NO_COLLISION_ENTITY(player, obj, TRUE);
    ENTITY::FREEZE_ENTITY_POSITION(obj, TRUE);



    //устанавливаем начальное вращение (pitch + heading)
    float initialHeading = ENTITY::GET_ENTITY_HEADING(player) + headingAngle;
    Vector3 rotation = { pitchAngle, 0.0f, initialHeading }; // pitch(X), roll(Y), yaw(Z)
    ENTITY::SET_ENTITY_ROTATION(obj, rotation.x, rotation.y, rotation.z, 2, TRUE); // 2 = world coords

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


    Vector3 offset = { camBlockOffsetX, camBlockOffsetY, camBlockOffsetZ };
    Vector3 pos = ENTITY::GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS(player, offset.x, offset.y, offset.z + offsetZ);

    ENTITY::SET_ENTITY_COORDS_NO_OFFSET(camBlockEntity, pos.x, pos.y, pos.z, TRUE, TRUE, TRUE);

    float playerHeading = ENTITY::GET_ENTITY_HEADING(player);
    Vector3 rotation = { pitchAngle, 0.0f, playerHeading + headingAngle };
    ENTITY::SET_ENTITY_ROTATION(camBlockEntity, rotation.x, rotation.y, rotation.z, 2, TRUE);
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


            // обновляем позицию с учётом новых смещений
            UpdateCamBlockPosition(player);
        }
    }
}


// Стандартная точка входа ASI-плагина
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        // Получаем путь к нашему .asi и собираем рядом EnhancedCamera.ini
        char modulePath[MAX_PATH] = { 0 };
        GetModuleFileNameA(hModule, modulePath, MAX_PATH);
        std::string dir(modulePath);
        size_t p = dir.find_last_of("\\/");
        if (p != std::string::npos) dir.erase(p);
        iniPath = dir + "\\gtav_centerend_aim.ini"; // <-- полный путь
        LoadSettings();
        scriptRegister(hModule, ScriptMain);
        break;
    }
    case DLL_PROCESS_DETACH:
        scriptUnregister(hModule);
        break;
    }
    return TRUE;
}
