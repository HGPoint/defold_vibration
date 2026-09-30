#include "vibrate_private.h"

#if defined(DM_PLATFORM_HTML5)

extern "C"
{
    int VibrateWeb_Trigger(const uint32_t* pattern, uint32_t count);
    int VibrateWeb_Cancel();
    int VibrateWeb_IsSupported();
    void VibrateWeb_Initialize();
    void VibrateWeb_Finalize();
}

static VibrateResult WebResult(int result)
{
    switch (result)
    {
        case VIBRATE_RESULT_OK:
        case VIBRATE_RESULT_UNSUPPORTED:
        case VIBRATE_RESULT_NOT_ALLOWED:
        case VIBRATE_RESULT_NOT_VISIBLE:
        case VIBRATE_RESULT_REJECTED:
        case VIBRATE_RESULT_PLATFORM_ERROR:
        case VIBRATE_RESULT_NO_HARDWARE:
            return (VibrateResult)result;
        default:
            return VIBRATE_RESULT_PLATFORM_ERROR;
    }
}

static void BuildPresetPattern(int preset, std::vector<uint32_t>* pattern)
{
    switch (preset)
    {
        case VIBRATE_PRESET_SELECTION:
        case VIBRATE_PRESET_TICK:
            pattern->push_back(10);
            break;
        case VIBRATE_PRESET_IMPACT_LIGHT:
        case VIBRATE_PRESET_CLICK:
            pattern->push_back(20);
            break;
        case VIBRATE_PRESET_IMPACT_MEDIUM:
            pattern->push_back(35);
            break;
        case VIBRATE_PRESET_IMPACT_HEAVY:
        case VIBRATE_PRESET_HEAVY_CLICK:
            pattern->push_back(50);
            break;
        case VIBRATE_PRESET_IMPACT_SOFT:
            pattern->push_back(25);
            break;
        case VIBRATE_PRESET_IMPACT_RIGID:
            pattern->push_back(40);
            break;
        case VIBRATE_PRESET_SUCCESS:
            pattern->push_back(20);
            pattern->push_back(50);
            pattern->push_back(35);
            break;
        case VIBRATE_PRESET_WARNING:
            pattern->push_back(30);
            pattern->push_back(40);
            pattern->push_back(30);
            break;
        case VIBRATE_PRESET_ERROR:
            pattern->push_back(40);
            pattern->push_back(30);
            pattern->push_back(40);
            pattern->push_back(30);
            pattern->push_back(60);
            break;
        case VIBRATE_PRESET_DOUBLE_CLICK:
            pattern->push_back(20);
            pattern->push_back(40);
            pattern->push_back(20);
            break;
        default:
            break;
    }
}

static bool BuildWebPattern(const VibrateOptions& options, std::vector<uint32_t>* pattern)
{
    if (!options.m_Fallback &&
        (options.m_HasIntensity || options.m_HasSharpness ||
         !options.m_Intensities.empty() || !options.m_Sharpnesses.empty()))
    {
        // The Vibration API only accepts timings. Do not silently discard
        // explicitly requested haptic parameters in strict mode.
        return false;
    }

    // Platform-specific values have precedence over their common equivalents.
    if (!options.m_WebPattern.empty())
    {
        *pattern = options.m_WebPattern;
        return true;
    }

    if (options.m_HasWebDuration)
    {
        pattern->push_back(options.m_WebDurationMs);
        return true;
    }

    if (!options.m_Pattern.empty())
    {
        *pattern = options.m_Pattern;
        return true;
    }

    if (options.m_Preset != VIBRATE_PRESET_DEFAULT)
    {
        if (!options.m_Fallback)
            return false;

        BuildPresetPattern(options.m_Preset, pattern);
        return !pattern->empty();
    }

    pattern->push_back(options.m_DurationMs);
    return true;
}

VibrateResult VibratePlatform_Trigger(const VibrateOptions& options)
{
    std::vector<uint32_t> pattern;
    if (!BuildWebPattern(options, &pattern))
        return VIBRATE_RESULT_UNSUPPORTED;

    return WebResult(VibrateWeb_Trigger(&pattern[0], (uint32_t)pattern.size()));
}

VibrateResult VibratePlatform_Cancel()
{
    return WebResult(VibrateWeb_Cancel());
}

void VibratePlatform_GetCapabilities(VibrateCapabilities* capabilities)
{
    if (!capabilities)
        return;

    const bool supported = VibrateWeb_IsSupported() != 0;
    capabilities->m_Platform = "web";
    capabilities->m_Supported = supported;
    capabilities->m_Pattern = supported;
    capabilities->m_Intensity = false;
    capabilities->m_Sharpness = false;
    capabilities->m_Cancel = supported;
    capabilities->m_Predefined = false;
    capabilities->m_Composition = false;
    capabilities->m_Repeat = false;
    capabilities->m_CoreHaptics = false;
    capabilities->m_UserActivationRequired = true;
}

void VibratePlatform_Initialize()
{
    VibrateWeb_Initialize();
}

void VibratePlatform_Finalize()
{
    VibrateWeb_Finalize();
}

#endif // DM_PLATFORM_HTML5
