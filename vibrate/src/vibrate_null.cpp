#if !defined(DM_PLATFORM_IOS) && !defined(DM_PLATFORM_ANDROID) && !defined(DM_PLATFORM_HTML5)

#include "vibrate_private.h"

VibrateResult VibratePlatform_Trigger(const VibrateOptions& options)
{
    (void)options;
    return VIBRATE_RESULT_UNSUPPORTED;
}

VibrateResult VibratePlatform_Cancel()
{
    return VIBRATE_RESULT_UNSUPPORTED;
}

void VibratePlatform_GetCapabilities(VibrateCapabilities* capabilities)
{
#if defined(DM_PLATFORM_OSX)
    capabilities->m_Platform = "macos";
#elif defined(DM_PLATFORM_WINDOWS)
    capabilities->m_Platform = "windows";
#elif defined(DM_PLATFORM_LINUX)
    capabilities->m_Platform = "linux";
#else
    capabilities->m_Platform = "unsupported";
#endif
}

void VibratePlatform_Initialize()
{
}

void VibratePlatform_Finalize()
{
}

#endif
