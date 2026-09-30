#include <assert.h>
#include <cmath>
#include <new>
#include <string.h>

#define EXTENSION_NAME Vibrate
#define LIB_NAME "Vibrate"
#define MODULE_NAME "vibrate"
#define DLIB_LOG_DOMAIN LIB_NAME

#include <dmsdk/sdk.h>

#include "vibrate_private.h"

static const uint32_t MAX_PATTERN_ITEMS = 256;
static const uint32_t MAX_PLATFORM_ITEMS = 256;
static const double MAX_DURATION_MS = 3600000.0;
static const char* OPTIONS_METATABLE = "vibrate.options";

struct NamedValue
{
    const char* m_Name;
    int m_Value;
};

static const NamedValue PRESETS[] = {
    {"default", VIBRATE_PRESET_DEFAULT},
    {"selection", VIBRATE_PRESET_SELECTION},
    {"impact_light", VIBRATE_PRESET_IMPACT_LIGHT},
    {"impact_medium", VIBRATE_PRESET_IMPACT_MEDIUM},
    {"impact_heavy", VIBRATE_PRESET_IMPACT_HEAVY},
    {"impact_soft", VIBRATE_PRESET_IMPACT_SOFT},
    {"impact_rigid", VIBRATE_PRESET_IMPACT_RIGID},
    {"success", VIBRATE_PRESET_SUCCESS},
    {"warning", VIBRATE_PRESET_WARNING},
    {"error", VIBRATE_PRESET_ERROR},
    {"click", VIBRATE_PRESET_CLICK},
    {"double_click", VIBRATE_PRESET_DOUBLE_CLICK},
    {"tick", VIBRATE_PRESET_TICK},
    {"heavy_click", VIBRATE_PRESET_HEAVY_CLICK},
};

static const NamedValue ANDROID_MODES[] = {
    {"auto", VIBRATE_ANDROID_MODE_AUTO},
    {"one_shot", VIBRATE_ANDROID_MODE_ONE_SHOT},
    {"waveform", VIBRATE_ANDROID_MODE_WAVEFORM},
    {"predefined", VIBRATE_ANDROID_MODE_PREDEFINED},
    {"composition", VIBRATE_ANDROID_MODE_COMPOSITION},
};

static const NamedValue ANDROID_EFFECTS[] = {
    {"click", VIBRATE_ANDROID_EFFECT_CLICK},
    {"double_click", VIBRATE_ANDROID_EFFECT_DOUBLE_CLICK},
    {"tick", VIBRATE_ANDROID_EFFECT_TICK},
    {"heavy_click", VIBRATE_ANDROID_EFFECT_HEAVY_CLICK},
};

static const NamedValue ANDROID_USAGES[] = {
    {"touch", VIBRATE_ANDROID_USAGE_TOUCH},
    {"game", VIBRATE_ANDROID_USAGE_GAME},
    {"notification", VIBRATE_ANDROID_USAGE_NOTIFICATION},
    {"alarm", VIBRATE_ANDROID_USAGE_ALARM},
    {"media", VIBRATE_ANDROID_USAGE_MEDIA},
};

static const NamedValue ANDROID_PRIMITIVES[] = {
    {"click", VIBRATE_ANDROID_PRIMITIVE_CLICK},
    {"thud", VIBRATE_ANDROID_PRIMITIVE_THUD},
    {"spin", VIBRATE_ANDROID_PRIMITIVE_SPIN},
    {"quick_rise", VIBRATE_ANDROID_PRIMITIVE_QUICK_RISE},
    {"slow_rise", VIBRATE_ANDROID_PRIMITIVE_SLOW_RISE},
    {"quick_fall", VIBRATE_ANDROID_PRIMITIVE_QUICK_FALL},
    {"tick", VIBRATE_ANDROID_PRIMITIVE_TICK},
    {"low_tick", VIBRATE_ANDROID_PRIMITIVE_LOW_TICK},
};

static const NamedValue IOS_MODES[] = {
    {"auto", VIBRATE_IOS_MODE_AUTO},
    {"system", VIBRATE_IOS_MODE_SYSTEM},
    {"selection", VIBRATE_IOS_MODE_SELECTION},
    {"impact", VIBRATE_IOS_MODE_IMPACT},
    {"notification", VIBRATE_IOS_MODE_NOTIFICATION},
    {"core_haptics", VIBRATE_IOS_MODE_CORE_HAPTICS},
};

static const NamedValue IOS_STYLES[] = {
    {"light", VIBRATE_IOS_STYLE_LIGHT},
    {"medium", VIBRATE_IOS_STYLE_MEDIUM},
    {"heavy", VIBRATE_IOS_STYLE_HEAVY},
    {"soft", VIBRATE_IOS_STYLE_SOFT},
    {"rigid", VIBRATE_IOS_STYLE_RIGID},
};

static const NamedValue IOS_NOTIFICATIONS[] = {
    {"success", VIBRATE_IOS_NOTIFICATION_SUCCESS},
    {"warning", VIBRATE_IOS_NOTIFICATION_WARNING},
    {"error", VIBRATE_IOS_NOTIFICATION_ERROR},
};

static const NamedValue IOS_EVENT_TYPES[] = {
    {"transient", VIBRATE_IOS_EVENT_TRANSIENT},
    {"continuous", VIBRATE_IOS_EVENT_CONTINUOUS},
};

static const char* ResultToString(VibrateResult result)
{
    switch (result)
    {
        case VIBRATE_RESULT_OK: return 0;
        case VIBRATE_RESULT_UNSUPPORTED: return "unsupported";
        case VIBRATE_RESULT_NOT_ALLOWED: return "not_allowed";
        case VIBRATE_RESULT_NOT_VISIBLE: return "not_visible";
        case VIBRATE_RESULT_REJECTED: return "rejected";
        case VIBRATE_RESULT_NO_HARDWARE: return "no_hardware";
        default: return "platform_error";
    }
}

static double CheckFiniteNumber(lua_State* L, int index, const char* name)
{
    if (lua_type(L, index) != LUA_TNUMBER)
        luaL_error(L, "%s must be a finite number", name);
    const double value = lua_tonumber(L, index);
    if (!std::isfinite(value))
        luaL_error(L, "%s must be a finite number", name);
    return value;
}

static uint32_t CheckDuration(lua_State* L, int index, const char* name)
{
    const double value = CheckFiniteNumber(L, index, name);
    if (value < 0.0 || value > MAX_DURATION_MS || std::floor(value) != value)
        luaL_error(L, "%s must be an integer between 0 and %.0f milliseconds", name, MAX_DURATION_MS);
    return (uint32_t)value;
}

static float CheckUnitFloat(lua_State* L, int index, const char* name)
{
    const double value = CheckFiniteNumber(L, index, name);
    if (value < 0.0 || value > 1.0)
        luaL_error(L, "%s must be between 0 and 1", name);
    return (float)value;
}

static int CheckNamedValue(lua_State* L, int index, const char* name, const NamedValue* values, uint32_t count)
{
    const char* value = luaL_checkstring(L, index);
    for (uint32_t i = 0; i < count; ++i)
    {
        if (strcmp(value, values[i].m_Name) == 0)
            return values[i].m_Value;
    }
    return luaL_error(L, "unknown %s '%s'", name, value);
}

static bool PushField(lua_State* L, int table_index, const char* name)
{
    lua_getfield(L, table_index, name);
    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        return false;
    }
    return true;
}

static void ReadDurationField(lua_State* L, int table_index, const char* field, uint32_t* value)
{
    if (!PushField(L, table_index, field))
        return;
    *value = CheckDuration(L, -1, field);
    lua_pop(L, 1);
}

static void ReadUnitFloatField(lua_State* L, int table_index, const char* field, float* value)
{
    if (!PushField(L, table_index, field))
        return;
    *value = CheckUnitFloat(L, -1, field);
    lua_pop(L, 1);
}

static void ReadBooleanField(lua_State* L, int table_index, const char* field, bool* value)
{
    if (!PushField(L, table_index, field))
        return;
    if (!lua_isboolean(L, -1))
        luaL_error(L, "%s must be a boolean", field);
    *value = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
}

static void ReadNamedField(lua_State* L, int table_index, const char* field, int* value, const NamedValue* values, uint32_t count)
{
    if (!PushField(L, table_index, field))
        return;
    *value = CheckNamedValue(L, -1, field, values, count);
    lua_pop(L, 1);
}

static void ReadDurationArray(lua_State* L, int table_index, const char* field, std::vector<uint32_t>* output, uint32_t max_items)
{
    if (!PushField(L, table_index, field))
        return;
    if (!lua_istable(L, -1))
        luaL_error(L, "%s must be an array", field);

    const uint32_t count = (uint32_t)lua_objlen(L, -1);
    if (count == 0 || count > max_items)
        luaL_error(L, "%s must contain between 1 and %u items", field, max_items);

    output->reserve(count);
    for (uint32_t i = 1; i <= count; ++i)
    {
        lua_rawgeti(L, -1, i);
        output->push_back(CheckDuration(L, -1, field));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

static void ReadUnitFloatArray(lua_State* L, int table_index, const char* field, std::vector<float>* output, uint32_t max_items)
{
    if (!PushField(L, table_index, field))
        return;
    if (!lua_istable(L, -1))
        luaL_error(L, "%s must be an array", field);

    const uint32_t count = (uint32_t)lua_objlen(L, -1);
    if (count == 0 || count > max_items)
        luaL_error(L, "%s must contain between 1 and %u items", field, max_items);

    output->reserve(count);
    for (uint32_t i = 1; i <= count; ++i)
    {
        lua_rawgeti(L, -1, i);
        output->push_back(CheckUnitFloat(L, -1, field));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

static void ReadAndroidAmplitudes(lua_State* L, int table_index, std::vector<int32_t>* output)
{
    if (!PushField(L, table_index, "amplitudes"))
        return;
    if (!lua_istable(L, -1))
        luaL_error(L, "amplitudes must be an array");

    const uint32_t count = (uint32_t)lua_objlen(L, -1);
    if (count == 0 || count > MAX_PLATFORM_ITEMS)
        luaL_error(L, "amplitudes must contain between 1 and %u items", MAX_PLATFORM_ITEMS);

    output->reserve(count);
    for (uint32_t i = 1; i <= count; ++i)
    {
        lua_rawgeti(L, -1, i);
        const double value = CheckFiniteNumber(L, -1, "amplitudes");
        if (std::floor(value) != value || value < -1.0 || value > 255.0)
            luaL_error(L, "amplitudes values must be -1 or integers between 0 and 255");
        output->push_back((int32_t)value);
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

static void ReadAndroidPrimitives(lua_State* L, int table_index, std::vector<VibrateAndroidPrimitive>* output)
{
    if (!PushField(L, table_index, "primitives"))
        return;
    if (!lua_istable(L, -1))
        luaL_error(L, "primitives must be an array");

    const uint32_t count = (uint32_t)lua_objlen(L, -1);
    if (count == 0 || count > MAX_PLATFORM_ITEMS)
        luaL_error(L, "primitives must contain between 1 and %u items", MAX_PLATFORM_ITEMS);

    output->reserve(count);
    for (uint32_t i = 1; i <= count; ++i)
    {
        lua_rawgeti(L, -1, i);
        if (!lua_istable(L, -1))
            luaL_error(L, "primitives[%u] must be a table", i);
        const int primitive_index = lua_gettop(L);

        VibrateAndroidPrimitive primitive;
        if (!PushField(L, primitive_index, "name"))
            luaL_error(L, "primitives[%u].name is required", i);
        primitive.m_Type = CheckNamedValue(L, -1, "name", ANDROID_PRIMITIVES, sizeof(ANDROID_PRIMITIVES) / sizeof(ANDROID_PRIMITIVES[0]));
        lua_pop(L, 1);
        ReadUnitFloatField(L, primitive_index, "scale", &primitive.m_Scale);
        ReadDurationField(L, primitive_index, "delay_ms", &primitive.m_DelayMs);
        output->push_back(primitive);
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

static void ReadIOSEvents(lua_State* L, int table_index, std::vector<VibrateIOSEvent>* output)
{
    if (!PushField(L, table_index, "events"))
        return;
    if (!lua_istable(L, -1))
        luaL_error(L, "events must be an array");

    const uint32_t count = (uint32_t)lua_objlen(L, -1);
    if (count == 0 || count > MAX_PLATFORM_ITEMS)
        luaL_error(L, "events must contain between 1 and %u items", MAX_PLATFORM_ITEMS);

    output->reserve(count);
    for (uint32_t i = 1; i <= count; ++i)
    {
        lua_rawgeti(L, -1, i);
        if (!lua_istable(L, -1))
            luaL_error(L, "events[%u] must be a table", i);
        const int event_index = lua_gettop(L);

        VibrateIOSEvent event;
        if (!PushField(L, event_index, "type"))
            luaL_error(L, "events[%u].type is required", i);
        event.m_Type = CheckNamedValue(L, -1, "type", IOS_EVENT_TYPES, sizeof(IOS_EVENT_TYPES) / sizeof(IOS_EVENT_TYPES[0]));
        lua_pop(L, 1);
        ReadDurationField(L, event_index, "time_ms", &event.m_TimeMs);
        ReadDurationField(L, event_index, "duration_ms", &event.m_DurationMs);
        ReadUnitFloatField(L, event_index, "intensity", &event.m_Intensity);
        ReadUnitFloatField(L, event_index, "sharpness", &event.m_Sharpness);
        if (event.m_Type == VIBRATE_IOS_EVENT_CONTINUOUS && event.m_DurationMs == 0)
            luaL_error(L, "continuous events require duration_ms > 0");
        output->push_back(event);
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

static void ReadAndroidOptions(lua_State* L, int options_index, VibrateOptions* options)
{
    if (!PushField(L, options_index, "android"))
        return;
    if (!lua_istable(L, -1))
        luaL_error(L, "android must be a table");
    const int table_index = lua_gettop(L);

    ReadNamedField(L, table_index, "mode", &options->m_AndroidMode, ANDROID_MODES, sizeof(ANDROID_MODES) / sizeof(ANDROID_MODES[0]));
    if (PushField(L, table_index, "effect"))
    {
        options->m_AndroidEffect = CheckNamedValue(L, -1, "effect", ANDROID_EFFECTS, sizeof(ANDROID_EFFECTS) / sizeof(ANDROID_EFFECTS[0]));
        options->m_HasAndroidEffect = true;
        lua_pop(L, 1);
    }
    ReadNamedField(L, table_index, "usage", &options->m_AndroidUsage, ANDROID_USAGES, sizeof(ANDROID_USAGES) / sizeof(ANDROID_USAGES[0]));
    ReadDurationArray(L, table_index, "timings", &options->m_AndroidTimings, MAX_PLATFORM_ITEMS);
    ReadAndroidAmplitudes(L, table_index, &options->m_AndroidAmplitudes);
    ReadAndroidPrimitives(L, table_index, &options->m_AndroidPrimitives);

    if (PushField(L, table_index, "amplitude"))
    {
        const double amplitude = CheckFiniteNumber(L, -1, "amplitude");
        if (std::floor(amplitude) != amplitude || (amplitude != -1.0 && (amplitude < 0.0 || amplitude > 255.0)))
            luaL_error(L, "amplitude must be -1 or an integer between 0 and 255");
        options->m_AndroidAmplitude = (int)amplitude;
        options->m_HasAndroidAmplitude = true;
        lua_pop(L, 1);
    }

    if (PushField(L, table_index, "repeat_index"))
    {
        const double repeat_index = CheckFiniteNumber(L, -1, "repeat_index");
        if (std::floor(repeat_index) != repeat_index || repeat_index < -1.0 || repeat_index > (double)MAX_PLATFORM_ITEMS || repeat_index == 0.0)
            luaL_error(L, "repeat_index must be -1 or a positive Lua array index");
        options->m_AndroidRepeatIndex = repeat_index < 0.0 ? -1 : (int)repeat_index - 1;
        lua_pop(L, 1);
    }

    lua_pop(L, 1);
}

static void ReadIOSOptions(lua_State* L, int options_index, VibrateOptions* options)
{
    if (!PushField(L, options_index, "ios"))
        return;
    if (!lua_istable(L, -1))
        luaL_error(L, "ios must be a table");
    const int table_index = lua_gettop(L);

    ReadNamedField(L, table_index, "mode", &options->m_IOSMode, IOS_MODES, sizeof(IOS_MODES) / sizeof(IOS_MODES[0]));
    if (PushField(L, table_index, "style"))
    {
        options->m_IOSStyle = CheckNamedValue(L, -1, "style", IOS_STYLES, sizeof(IOS_STYLES) / sizeof(IOS_STYLES[0]));
        options->m_HasIOSStyle = true;
        lua_pop(L, 1);
    }
    if (PushField(L, table_index, "notification"))
    {
        options->m_IOSNotification = CheckNamedValue(L, -1, "notification", IOS_NOTIFICATIONS, sizeof(IOS_NOTIFICATIONS) / sizeof(IOS_NOTIFICATIONS[0]));
        options->m_HasIOSNotification = true;
        lua_pop(L, 1);
    }
    ReadBooleanField(L, table_index, "repeat", &options->m_IOSRepeat);
    ReadIOSEvents(L, table_index, &options->m_IOSEvents);

    if (PushField(L, table_index, "ahap_json"))
    {
        size_t length = 0;
        const char* json = luaL_checklstring(L, -1, &length);
        if (length == 0)
            luaL_error(L, "ahap_json must not be empty");
        options->m_IOSAhapJson.assign(json, length);
        lua_pop(L, 1);
    }

    lua_pop(L, 1);
}

static void ReadWebOptions(lua_State* L, int options_index, VibrateOptions* options)
{
    if (!PushField(L, options_index, "web"))
        return;
    if (!lua_istable(L, -1))
        luaL_error(L, "web must be a table");
    const int table_index = lua_gettop(L);

    ReadDurationArray(L, table_index, "pattern", &options->m_WebPattern, MAX_PATTERN_ITEMS);
    if (PushField(L, table_index, "duration_ms"))
    {
        options->m_WebDurationMs = CheckDuration(L, -1, "duration_ms");
        options->m_HasWebDuration = true;
        lua_pop(L, 1);
    }

    lua_pop(L, 1);
}

static void ReadOptions(lua_State* L, int argument_count, VibrateOptions* options)
{
    if (argument_count == 0 || lua_isnil(L, 1))
        return;
    luaL_checktype(L, 1, LUA_TTABLE);

    if (PushField(L, 1, "duration_ms"))
    {
        options->m_DurationMs = CheckDuration(L, -1, "duration_ms");
        options->m_HasDuration = true;
        lua_pop(L, 1);
    }
    if (PushField(L, 1, "intensity"))
    {
        options->m_Intensity = CheckUnitFloat(L, -1, "intensity");
        options->m_HasIntensity = true;
        lua_pop(L, 1);
    }
    if (PushField(L, 1, "sharpness"))
    {
        options->m_Sharpness = CheckUnitFloat(L, -1, "sharpness");
        options->m_HasSharpness = true;
        lua_pop(L, 1);
    }
    ReadBooleanField(L, 1, "fallback", &options->m_Fallback);
    ReadNamedField(L, 1, "preset", &options->m_Preset, PRESETS, sizeof(PRESETS) / sizeof(PRESETS[0]));
    ReadDurationArray(L, 1, "pattern", &options->m_Pattern, MAX_PATTERN_ITEMS);
    ReadUnitFloatArray(L, 1, "intensities", &options->m_Intensities, MAX_PATTERN_ITEMS);
    ReadUnitFloatArray(L, 1, "sharpnesses", &options->m_Sharpnesses, MAX_PATTERN_ITEMS);

    if (!options->m_Intensities.empty() && options->m_Intensities.size() != options->m_Pattern.size())
        luaL_error(L, "intensities must have the same number of items as pattern");
    if (!options->m_Sharpnesses.empty() && options->m_Sharpnesses.size() != options->m_Pattern.size())
        luaL_error(L, "sharpnesses must have the same number of items as pattern");

    ReadAndroidOptions(L, 1, options);
    ReadIOSOptions(L, 1, options);
    ReadWebOptions(L, 1, options);

    if (!options->m_AndroidAmplitudes.empty() && !options->m_AndroidTimings.empty() &&
        options->m_AndroidAmplitudes.size() != options->m_AndroidTimings.size())
        luaL_error(L, "android.amplitudes must have the same number of items as android.timings");
    if (!options->m_AndroidAmplitudes.empty() && options->m_AndroidTimings.empty())
        luaL_error(L, "android.amplitudes requires android.timings");
    if (options->m_AndroidRepeatIndex >= 0 && !options->m_AndroidTimings.empty() &&
        options->m_AndroidRepeatIndex >= (int)options->m_AndroidTimings.size())
        luaL_error(L, "android.repeat_index is outside android.timings");
    if (options->m_AndroidRepeatIndex >= 0 && options->m_AndroidTimings.empty() && options->m_Pattern.empty())
        luaL_error(L, "android.repeat_index requires android.timings or a common pattern");
    if (options->m_AndroidRepeatIndex >= 0 && options->m_AndroidTimings.empty() &&
        options->m_AndroidRepeatIndex >= (int)options->m_Pattern.size())
        luaL_error(L, "android.repeat_index is outside the common pattern");
    if (options->m_AndroidMode == VIBRATE_ANDROID_MODE_COMPOSITION && options->m_AndroidPrimitives.empty())
        luaL_error(L, "android.mode 'composition' requires android.primitives");
    if (!options->m_IOSAhapJson.empty() && !options->m_IOSEvents.empty())
        luaL_error(L, "ios.ahap_json and ios.events are mutually exclusive");
}

static int PushResult(lua_State* L, VibrateResult result)
{
    lua_pushboolean(L, result == VIBRATE_RESULT_OK);
    const char* reason = ResultToString(result);
    if (reason)
        lua_pushstring(L, reason);
    else
        lua_pushnil(L);
    return 2;
}

static int DestroyOptions(lua_State* L)
{
    VibrateOptions* options = (VibrateOptions*)lua_touserdata(L, 1);
    if (options)
        options->~VibrateOptions();
    return 0;
}

static int Trigger(lua_State* L)
{
    // Lua reports argument errors with longjmp, which skips normal C++ stack
    // unwinding. Keep the STL-backed options in finalizable userdata so malformed
    // calls still release any vectors/strings already parsed before the error.
    const int argument_count = lua_gettop(L);
    const int options_index = argument_count + 1;
    VibrateOptions* options = new (lua_newuserdata(L, sizeof(VibrateOptions))) VibrateOptions();
    luaL_getmetatable(L, OPTIONS_METATABLE);
    lua_setmetatable(L, options_index);

    ReadOptions(L, argument_count, options);
    const VibrateResult result = VibratePlatform_Trigger(*options);

    options->~VibrateOptions();
    lua_pushnil(L);
    lua_setmetatable(L, options_index);
    lua_remove(L, options_index);
    return PushResult(L, result);
}

static int Cancel(lua_State* L)
{
    return PushResult(L, VibratePlatform_Cancel());
}

static int IsSupported(lua_State* L)
{
    VibrateCapabilities capabilities;
    VibratePlatform_GetCapabilities(&capabilities);
    lua_pushboolean(L, capabilities.m_Supported);
    return 1;
}

static void SetBooleanField(lua_State* L, const char* name, bool value)
{
    lua_pushboolean(L, value);
    lua_setfield(L, -2, name);
}

static int GetCapabilities(lua_State* L)
{
    VibrateCapabilities capabilities;
    VibratePlatform_GetCapabilities(&capabilities);

    lua_newtable(L);
    lua_pushstring(L, capabilities.m_Platform);
    lua_setfield(L, -2, "platform");
    SetBooleanField(L, "supported", capabilities.m_Supported);
    SetBooleanField(L, "pattern", capabilities.m_Pattern);
    SetBooleanField(L, "intensity", capabilities.m_Intensity);
    SetBooleanField(L, "sharpness", capabilities.m_Sharpness);
    SetBooleanField(L, "cancel", capabilities.m_Cancel);
    SetBooleanField(L, "predefined", capabilities.m_Predefined);
    SetBooleanField(L, "composition", capabilities.m_Composition);
    SetBooleanField(L, "repeat", capabilities.m_Repeat);
    SetBooleanField(L, "core_haptics", capabilities.m_CoreHaptics);
    SetBooleanField(L, "user_activation_required", capabilities.m_UserActivationRequired);
    return 1;
}

static const luaL_reg ModuleMethods[] = {
    {"trigger", Trigger},
    {"cancel", Cancel},
    {"is_supported", IsSupported},
    {"get_capabilities", GetCapabilities},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
    const int top = lua_gettop(L);
    luaL_newmetatable(L, OPTIONS_METATABLE);
    lua_pushcfunction(L, DestroyOptions);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);

    luaL_register(L, MODULE_NAME, ModuleMethods);
    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

static dmExtension::Result AppInitializeVibrate(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result InitializeVibrate(dmExtension::Params* params)
{
    LuaInit(params->m_L);
    VibratePlatform_Initialize();
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeVibrate(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeVibrate(dmExtension::Params* params)
{
    VibratePlatform_Finalize();
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(EXTENSION_NAME, LIB_NAME, AppInitializeVibrate, AppFinalizeVibrate, InitializeVibrate, 0, 0, FinalizeVibrate)
