#pragma once

#include <stdint.h>
#include <string>
#include <vector>

enum VibrateResult
{
    VIBRATE_RESULT_OK = 0,
    VIBRATE_RESULT_UNSUPPORTED = 1,
    VIBRATE_RESULT_NOT_ALLOWED = 2,
    VIBRATE_RESULT_NOT_VISIBLE = 3,
    VIBRATE_RESULT_REJECTED = 4,
    VIBRATE_RESULT_PLATFORM_ERROR = 5,
    VIBRATE_RESULT_NO_HARDWARE = 6,
};

enum VibratePreset
{
    VIBRATE_PRESET_DEFAULT = 0,
    VIBRATE_PRESET_SELECTION,
    VIBRATE_PRESET_IMPACT_LIGHT,
    VIBRATE_PRESET_IMPACT_MEDIUM,
    VIBRATE_PRESET_IMPACT_HEAVY,
    VIBRATE_PRESET_IMPACT_SOFT,
    VIBRATE_PRESET_IMPACT_RIGID,
    VIBRATE_PRESET_SUCCESS,
    VIBRATE_PRESET_WARNING,
    VIBRATE_PRESET_ERROR,
    VIBRATE_PRESET_CLICK,
    VIBRATE_PRESET_DOUBLE_CLICK,
    VIBRATE_PRESET_TICK,
    VIBRATE_PRESET_HEAVY_CLICK,
};

enum VibrateAndroidMode
{
    VIBRATE_ANDROID_MODE_AUTO = 0,
    VIBRATE_ANDROID_MODE_ONE_SHOT,
    VIBRATE_ANDROID_MODE_WAVEFORM,
    VIBRATE_ANDROID_MODE_PREDEFINED,
    VIBRATE_ANDROID_MODE_COMPOSITION,
};

enum VibrateAndroidEffect
{
    VIBRATE_ANDROID_EFFECT_CLICK = 0,
    VIBRATE_ANDROID_EFFECT_DOUBLE_CLICK,
    VIBRATE_ANDROID_EFFECT_TICK,
    VIBRATE_ANDROID_EFFECT_HEAVY_CLICK,
};

enum VibrateAndroidUsage
{
    VIBRATE_ANDROID_USAGE_TOUCH = 0,
    VIBRATE_ANDROID_USAGE_GAME,
    VIBRATE_ANDROID_USAGE_NOTIFICATION,
    VIBRATE_ANDROID_USAGE_ALARM,
    VIBRATE_ANDROID_USAGE_MEDIA,
};

enum VibrateAndroidPrimitiveType
{
    VIBRATE_ANDROID_PRIMITIVE_CLICK = 0,
    VIBRATE_ANDROID_PRIMITIVE_THUD,
    VIBRATE_ANDROID_PRIMITIVE_SPIN,
    VIBRATE_ANDROID_PRIMITIVE_QUICK_RISE,
    VIBRATE_ANDROID_PRIMITIVE_SLOW_RISE,
    VIBRATE_ANDROID_PRIMITIVE_QUICK_FALL,
    VIBRATE_ANDROID_PRIMITIVE_TICK,
    VIBRATE_ANDROID_PRIMITIVE_LOW_TICK,
};

struct VibrateAndroidPrimitive
{
    int m_Type;
    float m_Scale;
    uint32_t m_DelayMs;

    VibrateAndroidPrimitive()
    : m_Type(VIBRATE_ANDROID_PRIMITIVE_CLICK)
    , m_Scale(1.0f)
    , m_DelayMs(0)
    {
    }
};

enum VibrateIOSMode
{
    VIBRATE_IOS_MODE_AUTO = 0,
    VIBRATE_IOS_MODE_SYSTEM,
    VIBRATE_IOS_MODE_SELECTION,
    VIBRATE_IOS_MODE_IMPACT,
    VIBRATE_IOS_MODE_NOTIFICATION,
    VIBRATE_IOS_MODE_CORE_HAPTICS,
};

enum VibrateIOSStyle
{
    VIBRATE_IOS_STYLE_LIGHT = 0,
    VIBRATE_IOS_STYLE_MEDIUM,
    VIBRATE_IOS_STYLE_HEAVY,
    VIBRATE_IOS_STYLE_SOFT,
    VIBRATE_IOS_STYLE_RIGID,
};

enum VibrateIOSNotification
{
    VIBRATE_IOS_NOTIFICATION_SUCCESS = 0,
    VIBRATE_IOS_NOTIFICATION_WARNING,
    VIBRATE_IOS_NOTIFICATION_ERROR,
};

enum VibrateIOSEventType
{
    VIBRATE_IOS_EVENT_TRANSIENT = 0,
    VIBRATE_IOS_EVENT_CONTINUOUS,
};

struct VibrateIOSEvent
{
    int m_Type;
    uint32_t m_TimeMs;
    uint32_t m_DurationMs;
    float m_Intensity;
    float m_Sharpness;

    VibrateIOSEvent()
    : m_Type(VIBRATE_IOS_EVENT_TRANSIENT)
    , m_TimeMs(0)
    , m_DurationMs(0)
    , m_Intensity(1.0f)
    , m_Sharpness(0.5f)
    {
    }
};

struct VibrateOptions
{
    uint32_t m_DurationMs;
    float m_Intensity;
    float m_Sharpness;
    int m_Preset;
    bool m_Fallback;
    bool m_HasDuration;
    bool m_HasIntensity;
    bool m_HasSharpness;
    std::vector<uint32_t> m_Pattern;
    std::vector<float> m_Intensities;
    std::vector<float> m_Sharpnesses;

    int m_AndroidMode;
    int m_AndroidAmplitude;
    int m_AndroidRepeatIndex;
    int m_AndroidEffect;
    int m_AndroidUsage;
    bool m_HasAndroidAmplitude;
    bool m_HasAndroidEffect;
    std::vector<uint32_t> m_AndroidTimings;
    std::vector<int32_t> m_AndroidAmplitudes;
    std::vector<VibrateAndroidPrimitive> m_AndroidPrimitives;

    int m_IOSMode;
    int m_IOSStyle;
    int m_IOSNotification;
    bool m_IOSRepeat;
    bool m_HasIOSStyle;
    bool m_HasIOSNotification;
    std::vector<VibrateIOSEvent> m_IOSEvents;
    std::string m_IOSAhapJson;

    uint32_t m_WebDurationMs;
    bool m_HasWebDuration;
    std::vector<uint32_t> m_WebPattern;

    VibrateOptions()
    : m_DurationMs(1000)
    , m_Intensity(1.0f)
    , m_Sharpness(0.5f)
    , m_Preset(VIBRATE_PRESET_DEFAULT)
    , m_Fallback(false)
    , m_HasDuration(false)
    , m_HasIntensity(false)
    , m_HasSharpness(false)
    , m_AndroidMode(VIBRATE_ANDROID_MODE_AUTO)
    , m_AndroidAmplitude(-1)
    , m_AndroidRepeatIndex(-1)
    , m_AndroidEffect(VIBRATE_ANDROID_EFFECT_CLICK)
    , m_AndroidUsage(VIBRATE_ANDROID_USAGE_GAME)
    , m_HasAndroidAmplitude(false)
    , m_HasAndroidEffect(false)
    , m_IOSMode(VIBRATE_IOS_MODE_AUTO)
    , m_IOSStyle(VIBRATE_IOS_STYLE_MEDIUM)
    , m_IOSNotification(VIBRATE_IOS_NOTIFICATION_SUCCESS)
    , m_IOSRepeat(false)
    , m_HasIOSStyle(false)
    , m_HasIOSNotification(false)
    , m_WebDurationMs(0)
    , m_HasWebDuration(false)
    {
    }
};

struct VibrateCapabilities
{
    const char* m_Platform;
    bool m_Supported;
    bool m_Pattern;
    bool m_Intensity;
    bool m_Sharpness;
    bool m_Cancel;
    bool m_Predefined;
    bool m_Composition;
    bool m_Repeat;
    bool m_CoreHaptics;
    bool m_UserActivationRequired;

    VibrateCapabilities()
    : m_Platform("unknown")
    , m_Supported(false)
    , m_Pattern(false)
    , m_Intensity(false)
    , m_Sharpness(false)
    , m_Cancel(false)
    , m_Predefined(false)
    , m_Composition(false)
    , m_Repeat(false)
    , m_CoreHaptics(false)
    , m_UserActivationRequired(false)
    {
    }
};

VibrateResult VibratePlatform_Trigger(const VibrateOptions& options);
VibrateResult VibratePlatform_Cancel();
void VibratePlatform_GetCapabilities(VibrateCapabilities* capabilities);
void VibratePlatform_Initialize();
void VibratePlatform_Finalize();
