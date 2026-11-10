// GamePad-Waechter (GamePad Guard) - Aroma plugin
//
// Two modes for the Wii U:
//   * "Mit GamePad"  : everything works as usual.
//   * "Ohne GamePad" : games from the block list (SD card) are not started. The console returns to the
//                      Wii U Menu and shows a message that the game needs the GamePad.
//   * "Automatisch"  : like "Ohne GamePad", but only while no GamePad is connected.
//
// Mode can be changed in the Aroma plugin menu (L + DPAD-Down + SELECT) or by holding L + R + MINUS.

#include "blocklist.h"

#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <coreinit/title.h>
#include <notifications/notifications.h>
#include <sysapp/launch.h>
#include <vpad/input.h>

#include <wups.h>
#include <wups/button_combo/api.h>
#include <wups/config/WUPSConfigCategory.h>
#include <wups/config/WUPSConfigItemBoolean.h>
#include <wups/config/WUPSConfigItemMultipleValues.h>
#include <wups/config/WUPSConfigItemStub.h>
#include <wups/config_api.h>

#include <optional>
#include <string>
#include <string_view>

WUPS_PLUGIN_NAME("GamePad-Waechter");
WUPS_PLUGIN_DESCRIPTION("Sperrt Spiele, die das Wii U GamePad benoetigen, wenn ohne GamePad gespielt wird.");
WUPS_PLUGIN_VERSION("v1.0.0");
WUPS_PLUGIN_AUTHOR("kuallieo");
WUPS_PLUGIN_LICENSE("MIT");

WUPS_USE_WUT_DEVOPTAB();
WUPS_USE_STORAGE("gamepad_guard");

namespace {

enum Mode : int32_t {
    MODE_AUTO            = 0, // block only when no GamePad is connected
    MODE_WITHOUT_GAMEPAD = 1, // always block GamePad-only games
    MODE_WITH_GAMEPAD    = 2, // never block anything
};

constexpr const char *kModeKey     = "mode";
constexpr const char *kBootHintKey = "showBootHint";

constexpr Mode kDefaultMode     = MODE_AUTO;
constexpr bool kDefaultBootHint = true;

constexpr WUPSButtonCombo_Buttons kSwitchCombo = WUPS_BUTTON_COMBO_BUTTON_L | WUPS_BUTTON_COMBO_BUTTON_R | WUPS_BUTTON_COMBO_BUTTON_MINUS;
constexpr uint32_t kSwitchHoldMs               = 1500;

Mode sMode          = kDefaultMode;
bool sShowBootHint  = kDefaultBootHint;
bool sBootHintShown = false;
bool sNotifyReady   = false;

std::optional<WUPSButtonComboAPI::ButtonCombo> sSwitchCombo;

constexpr NMColor kWhite    = {255, 255, 255, 255};
constexpr NMColor kErrorBg  = {150, 30, 30, 230};
constexpr NMColor kInfoBg   = {30, 30, 30, 220};
constexpr uint64_t kTypeApp = 0x00050000; // upper half of title ids of games / apps

const char *ModeName(Mode m) {
    switch (m) {
        case MODE_AUTO:
            return "Automatisch";
        case MODE_WITHOUT_GAMEPAD:
            return "Ohne GamePad";
        case MODE_WITH_GAMEPAD:
            return "Mit GamePad";
    }
    return "?";
}

void ShowInfo(const std::string &text, float seconds = 6.0f) {
    if (!sNotifyReady) return;
    NotificationModule_AddInfoNotificationEx(text.c_str(), seconds, kWhite, kInfoBg, nullptr, nullptr, true);
}

void ShowError(const std::string &text, float seconds = 10.0f) {
    if (!sNotifyReady) return;
    NotificationModule_AddErrorNotificationEx(text.c_str(), seconds, 1.0f, kWhite, kErrorBg, nullptr, nullptr, true);
}

void SaveSettings() {
    WUPSStorageAPI::Store(kModeKey, static_cast<int32_t>(sMode));
    WUPSStorageAPI::Store(kBootHintKey, sShowBootHint);
    WUPSStorageAPI::SaveStorage();
}

bool IsWiiUMenu(uint64_t titleId) {
    switch (titleId) {
        case 0x0005001010040000ULL: // JPN
        case 0x0005001010040100ULL: // USA
        case 0x0005001010040200ULL: // EUR
            return true;
        default:
            return false;
    }
}

// Returns false only if the GamePad is definitely not connected. Unknown states count as connected,
// so the plugin never blocks a game because of a glitch.
bool IsGamePadConnected() {
    VPADStatus status{};
    VPADReadError err = VPAD_READ_SUCCESS;
    for (int i = 0; i < 20; i++) {
        VPADRead(VPAD_CHAN_0, &status, 1, &err);
        if (err == VPAD_READ_SUCCESS) return true;
        if (err == VPAD_READ_INVALID_CONTROLLER) return false;
        OSSleepTicks(OSMillisecondsToTicks(5));
    }
    return true;
}

bool IsGamePadlessNow() {
    switch (sMode) {
        case MODE_WITHOUT_GAMEPAD:
            return true;
        case MODE_AUTO:
            return !IsGamePadConnected();
        case MODE_WITH_GAMEPAD:
        default:
            return false;
    }
}

std::string ModeSummary() {
    std::string s = std::string("GamePad-Waechter: Modus \"") + ModeName(sMode) + "\"";
    if (sMode == MODE_AUTO) {
        s += IsGamePadConnected() ? " (GamePad erkannt)" : " (kein GamePad erkannt)";
    }
    return s;
}

void OnSwitchCombo(WUPSButtonCombo_ControllerTypes, WUPSButtonCombo_ComboHandle, void *) {
    switch (sMode) {
        case MODE_AUTO:
            sMode = MODE_WITHOUT_GAMEPAD;
            break;
        case MODE_WITHOUT_GAMEPAD:
            sMode = MODE_WITH_GAMEPAD;
            break;
        default:
            sMode = MODE_AUTO;
            break;
    }
    SaveSettings();
    ShowInfo(ModeSummary(), 4.0f);
}

// ---------------------------------------------------------------------------------------------------------------------
// Config menu (L + DPAD-Down + SELECT)
// ---------------------------------------------------------------------------------------------------------------------

void OnModeChanged(ConfigItemMultipleValues *, uint32_t newValue) {
    sMode = static_cast<Mode>(newValue);
    WUPSStorageAPI::Store(kModeKey, static_cast<int32_t>(sMode));
}

void OnBootHintChanged(ConfigItemBoolean *, bool newValue) {
    sShowBootHint = newValue;
    WUPSStorageAPI::Store(kBootHintKey, sShowBootHint);
}

WUPSConfigAPICallbackStatus ConfigMenuOpened(WUPSConfigCategoryHandle rootHandle) {
    WUPSConfigCategory root(rootHandle);
    try {
        constexpr WUPSConfigItemMultipleValues::ValuePair modes[] = {
                {MODE_AUTO, "Automatisch (GamePad erkennen)"},
                {MODE_WITHOUT_GAMEPAD, "Ohne GamePad (Spiele sperren)"},
                {MODE_WITH_GAMEPAD, "Mit GamePad (nichts sperren)"},
        };
        root.add(WUPSConfigItemMultipleValues::CreateFromValue(kModeKey, "Modus", kDefaultMode, sMode, modes, OnModeChanged));
        root.add(WUPSConfigItemBoolean::Create(kBootHintKey, "Modus nach dem Einschalten anzeigen",
                                               kDefaultBootHint, sShowBootHint, OnBootHintChanged));
        root.add(WUPSConfigItemStub::Create("Schnellwechsel: L + R + MINUS 1,5 Sek. gedrueckt halten"));

        auto list     = blocklist::Load();
        auto category = WUPSConfigCategory::Create("Gesperrte Spiele (" + std::to_string(list.entries.size()) + ")");
        category.add(WUPSConfigItemStub::Create(list.fromFile ? "Quelle: SD:/wiiu/gamepad-guard/blocklist.txt"
                                                              : "Quelle: eingebaute Liste (keine Datei auf SD)"));
        size_t shown = 0;
        for (const auto &e : list.entries) {
            if (++shown > 100) {
                category.add(WUPSConfigItemStub::Create("..."));
                break;
            }
            category.add(WUPSConfigItemStub::Create(e.name));
        }
        root.add(std::move(category));
    } catch (std::exception &) {
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    }
    return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}

void ConfigMenuClosed() {
    WUPSStorageAPI::SaveStorage();
}

} // namespace

INITIALIZE_PLUGIN() {
    WUPSConfigAPIOptionsV1 configOptions = {.name = "GamePad-Waechter"};
    WUPSConfigAPI_Init(configOptions, ConfigMenuOpened, ConfigMenuClosed);

    int32_t mode = kDefaultMode;
    WUPSStorageAPI::GetOrStoreDefault(kModeKey, mode, static_cast<int32_t>(kDefaultMode));
    sMode = (mode >= MODE_AUTO && mode <= MODE_WITH_GAMEPAD) ? static_cast<Mode>(mode) : kDefaultMode;
    WUPSStorageAPI::GetOrStoreDefault(kBootHintKey, sShowBootHint, kDefaultBootHint);
    WUPSStorageAPI::SaveStorage();

    sNotifyReady = NotificationModule_InitLibrary() == NOTIFICATION_MODULE_RESULT_SUCCESS;

    WUPSButtonCombo_ComboStatus comboStatus;
    WUPSButtonCombo_Error comboError;
    sSwitchCombo = WUPSButtonComboAPI::CreateComboHold("GamePad-Waechter: Modus wechseln", kSwitchCombo, kSwitchHoldMs,
                                                       OnSwitchCombo, nullptr, comboStatus, comboError);
}

DEINITIALIZE_PLUGIN() {
    sSwitchCombo.reset();
    if (sNotifyReady) {
        NotificationModule_DeInitLibrary();
        sNotifyReady = false;
    }
}

ON_APPLICATION_START() {
    const uint64_t titleId = OSGetTitleID();

    if (IsWiiUMenu(titleId)) {
        blocklist::WriteDefaultFileIfMissing();
        if (!sBootHintShown) {
            sBootHintShown = true;
            if (sShowBootHint) {
                ShowInfo(ModeSummary() + " - Wechseln: L + R + MINUS halten", 8.0f);
            }
        }
        return;
    }

    // Only check real games/apps - not system titles.
    if ((titleId >> 32) != kTypeApp) return;
    if (!IsGamePadlessNow()) return;

    auto list = blocklist::Load();
    std::string gameName;
    if (!blocklist::Matches(list, titleId, gameName)) return;

    // The message is queued and shown once the Wii U Menu is back on screen.
    ShowError("\"" + gameName + "\" benoetigt das Wii U GamePad. Bitte verbinde dein GamePad, um fortzufahren.");
    SYSLaunchMenu();
}
