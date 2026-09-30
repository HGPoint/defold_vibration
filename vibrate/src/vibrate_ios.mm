#if defined(DM_PLATFORM_IOS)

#include <algorithm>

#include <dmsdk/sdk.h>
#include "vibrate_private.h"

#import <AudioToolbox/AudioServices.h>
#import <CoreHaptics/CoreHaptics.h>
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

API_AVAILABLE(ios(13.0))
static CHHapticEngine* g_HapticEngine = nil;
API_AVAILABLE(ios(13.0))
static id<CHHapticAdvancedPatternPlayer> g_HapticPlayer = nil;

API_AVAILABLE(ios(13.0))
static void ReleasePlayer()
{
    if (g_HapticPlayer)
    {
        [g_HapticPlayer release];
        g_HapticPlayer = nil;
    }
}

static VibrateResult StopPlayer()
{
    if (@available(iOS 13.0, *))
    {
        if (g_HapticPlayer)
        {
            NSError* error = nil;
            const BOOL stopped = [g_HapticPlayer stopAtTime:CHHapticTimeImmediate error:&error];
            if (!stopped)
            {
                dmLogWarning("Unable to stop Core Haptics player: %s", error ? [[error localizedDescription] UTF8String] : "unknown error");
                return VIBRATE_RESULT_PLATFORM_ERROR;
            }
            ReleasePlayer();
        }
    }
    return VIBRATE_RESULT_OK;
}

static bool SupportsCoreHaptics()
{
    if (@available(iOS 13.0, *))
        return [CHHapticEngine capabilitiesForHardware].supportsHaptics == YES;
    return false;
}

static VibrateResult PlaySystemVibration()
{
    AudioServicesPlaySystemSound(kSystemSoundID_Vibrate);
    return VIBRATE_RESULT_OK;
}

static UIImpactFeedbackStyle GetImpactStyle(int style)
{
    switch (style)
    {
        case VIBRATE_IOS_STYLE_LIGHT:
            return UIImpactFeedbackStyleLight;
        case VIBRATE_IOS_STYLE_HEAVY:
            return UIImpactFeedbackStyleHeavy;
        case VIBRATE_IOS_STYLE_SOFT:
            if (@available(iOS 13.0, *))
                return UIImpactFeedbackStyleSoft;
            return UIImpactFeedbackStyleLight;
        case VIBRATE_IOS_STYLE_RIGID:
            if (@available(iOS 13.0, *))
                return UIImpactFeedbackStyleRigid;
            return UIImpactFeedbackStyleHeavy;
        default:
            return UIImpactFeedbackStyleMedium;
    }
}

static VibrateResult PlayImpact(int style, float intensity)
{
    UIImpactFeedbackGenerator* generator = [[UIImpactFeedbackGenerator alloc] initWithStyle:GetImpactStyle(style)];
    [generator prepare];
    if (@available(iOS 13.0, *))
        [generator impactOccurredWithIntensity:intensity];
    else
        [generator impactOccurred];
    [generator release];
    return VIBRATE_RESULT_OK;
}

static VibrateResult PlaySelection()
{
    UISelectionFeedbackGenerator* generator = [[UISelectionFeedbackGenerator alloc] init];
    [generator prepare];
    [generator selectionChanged];
    [generator release];
    return VIBRATE_RESULT_OK;
}

static VibrateResult PlayNotification(int notification)
{
    UINotificationFeedbackType type = UINotificationFeedbackTypeSuccess;
    if (notification == VIBRATE_IOS_NOTIFICATION_WARNING)
        type = UINotificationFeedbackTypeWarning;
    else if (notification == VIBRATE_IOS_NOTIFICATION_ERROR)
        type = UINotificationFeedbackTypeError;

    UINotificationFeedbackGenerator* generator = [[UINotificationFeedbackGenerator alloc] init];
    [generator prepare];
    [generator notificationOccurred:type];
    [generator release];
    return VIBRATE_RESULT_OK;
}

API_AVAILABLE(ios(13.0))
static void AddEvent(NSMutableArray* events, int type, double time, double duration, float intensity, float sharpness)
{
    CHHapticEventParameter* intensity_parameter = [[[CHHapticEventParameter alloc]
        initWithParameterID:CHHapticEventParameterIDHapticIntensity value:intensity] autorelease];
    CHHapticEventParameter* sharpness_parameter = [[[CHHapticEventParameter alloc]
        initWithParameterID:CHHapticEventParameterIDHapticSharpness value:sharpness] autorelease];
    NSArray* parameters = [NSArray arrayWithObjects:intensity_parameter, sharpness_parameter, nil];

    if (type == VIBRATE_IOS_EVENT_CONTINUOUS)
    {
        // Core Haptics limits one continuous event to 30 seconds. Split longer
        // Lua events without changing their perceived timeline.
        double remaining = duration;
        double offset = time;
        while (remaining > 0.0)
        {
            const double chunk = std::min(remaining, 30.0);
            CHHapticEvent* event = [[[CHHapticEvent alloc]
                initWithEventType:CHHapticEventTypeHapticContinuous
                parameters:parameters
                relativeTime:offset
                duration:chunk] autorelease];
            [events addObject:event];
            remaining -= chunk;
            offset += chunk;
        }
    }
    else
    {
        CHHapticEvent* event = [[[CHHapticEvent alloc]
            initWithEventType:CHHapticEventTypeHapticTransient
            parameters:parameters
            relativeTime:time] autorelease];
        [events addObject:event];
    }
}

API_AVAILABLE(ios(13.0))
static CHHapticPattern* CreatePattern(const VibrateOptions& options, NSError** error)
{
    if (!options.m_IOSAhapJson.empty())
    {
        NSData* data = [NSData dataWithBytes:options.m_IOSAhapJson.data() length:options.m_IOSAhapJson.size()];
        id object = [NSJSONSerialization JSONObjectWithData:data options:0 error:error];
        if (!object || ![object isKindOfClass:[NSDictionary class]])
            return nil;
        return [[CHHapticPattern alloc] initWithDictionary:(NSDictionary*)object error:error];
    }

    NSMutableArray* events = [NSMutableArray array];
    if (!options.m_IOSEvents.empty())
    {
        for (size_t i = 0; i < options.m_IOSEvents.size(); ++i)
        {
            const VibrateIOSEvent& event = options.m_IOSEvents[i];
            AddEvent(events,
                event.m_Type,
                event.m_TimeMs / 1000.0,
                event.m_DurationMs / 1000.0,
                event.m_Intensity,
                event.m_Sharpness);
        }
    }
    else if (!options.m_Pattern.empty())
    {
        double time = 0.0;
        for (size_t i = 0; i < options.m_Pattern.size(); ++i)
        {
            const double duration = options.m_Pattern[i] / 1000.0;
            if ((i % 2) == 0 && duration > 0.0)
            {
                const float intensity = options.m_Intensities.empty() ? options.m_Intensity : options.m_Intensities[i];
                const float sharpness = options.m_Sharpnesses.empty() ? options.m_Sharpness : options.m_Sharpnesses[i];
                AddEvent(events, VIBRATE_IOS_EVENT_CONTINUOUS, time, duration, intensity, sharpness);
            }
            time += duration;
        }
    }
    else if (options.m_DurationMs > 0)
    {
        AddEvent(events,
            VIBRATE_IOS_EVENT_CONTINUOUS,
            0.0,
            options.m_DurationMs / 1000.0,
            options.m_Intensity,
            options.m_Sharpness);
    }

    if ([events count] == 0)
        return nil;
    return [[CHHapticPattern alloc] initWithEvents:events parameters:[NSArray array] error:error];
}

API_AVAILABLE(ios(13.0))
static VibrateResult PlayCoreHapticsAvailable(const VibrateOptions& options)
{
    @autoreleasepool
    {
        NSError* error = nil;
        if (!g_HapticEngine)
        {
            g_HapticEngine = [[CHHapticEngine alloc] initAndReturnError:&error];
            if (!g_HapticEngine || error)
            {
                dmLogWarning("Unable to create Core Haptics engine: %s", error ? [[error localizedDescription] UTF8String] : "unknown error");
                return VIBRATE_RESULT_PLATFORM_ERROR;
            }
            g_HapticEngine.playsHapticsOnly = YES;
            g_HapticEngine.autoShutdownEnabled = YES;
        }

        if (![g_HapticEngine startAndReturnError:&error])
        {
            dmLogWarning("Unable to start Core Haptics engine: %s", error ? [[error localizedDescription] UTF8String] : "unknown error");
            return VIBRATE_RESULT_PLATFORM_ERROR;
        }

        CHHapticPattern* pattern = CreatePattern(options, &error);
        if (!pattern)
        {
            dmLogWarning("Unable to create Core Haptics pattern: %s", error ? [[error localizedDescription] UTF8String] : "empty pattern");
            return VIBRATE_RESULT_PLATFORM_ERROR;
        }

        const VibrateResult stop_result = StopPlayer();
        if (stop_result != VIBRATE_RESULT_OK)
        {
            [pattern release];
            return stop_result;
        }

        id<CHHapticAdvancedPatternPlayer> player = [g_HapticEngine createAdvancedPlayerWithPattern:pattern error:&error];
        if (!player || error)
        {
            dmLogWarning("Unable to create Core Haptics player: %s", error ? [[error localizedDescription] UTF8String] : "unknown error");
            [pattern release];
            return VIBRATE_RESULT_PLATFORM_ERROR;
        }

        g_HapticPlayer = [player retain];
        if (options.m_IOSRepeat)
        {
            g_HapticPlayer.loopEnd = pattern.duration;
            g_HapticPlayer.loopEnabled = YES;
        }

        const BOOL started = [g_HapticPlayer startAtTime:CHHapticTimeImmediate error:&error];
        [pattern release];
        if (!started)
        {
            dmLogWarning("Unable to play Core Haptics pattern: %s", error ? [[error localizedDescription] UTF8String] : "unknown error");
            ReleasePlayer();
            return VIBRATE_RESULT_PLATFORM_ERROR;
        }
    }
    return VIBRATE_RESULT_OK;
}

static VibrateResult PlayCoreHaptics(const VibrateOptions& options)
{
    if (@available(iOS 13.0, *))
    {
        if (!SupportsCoreHaptics())
            return VIBRATE_RESULT_NO_HARDWARE;
        return PlayCoreHapticsAvailable(options);
    }
    return VIBRATE_RESULT_NO_HARDWARE;
}

static int StyleForPreset(int preset)
{
    switch (preset)
    {
        case VIBRATE_PRESET_IMPACT_LIGHT:
        case VIBRATE_PRESET_TICK:
            return VIBRATE_IOS_STYLE_LIGHT;
        case VIBRATE_PRESET_IMPACT_HEAVY:
        case VIBRATE_PRESET_HEAVY_CLICK:
            return VIBRATE_IOS_STYLE_HEAVY;
        case VIBRATE_PRESET_IMPACT_SOFT:
            return VIBRATE_IOS_STYLE_SOFT;
        case VIBRATE_PRESET_IMPACT_RIGID:
            return VIBRATE_IOS_STYLE_RIGID;
        default:
            return VIBRATE_IOS_STYLE_MEDIUM;
    }
}

VibrateResult VibratePlatform_Trigger(const VibrateOptions& options)
{
    int mode = options.m_IOSMode;
    if (mode == VIBRATE_IOS_MODE_AUTO)
    {
        if (!options.m_IOSAhapJson.empty() || !options.m_IOSEvents.empty() || options.m_IOSRepeat)
            mode = VIBRATE_IOS_MODE_CORE_HAPTICS;
        else if (options.m_HasIOSNotification)
            mode = VIBRATE_IOS_MODE_NOTIFICATION;
        else if (options.m_HasIOSStyle)
            mode = VIBRATE_IOS_MODE_IMPACT;
        else if (!options.m_Pattern.empty())
            mode = VIBRATE_IOS_MODE_CORE_HAPTICS;
        else if (options.m_Preset == VIBRATE_PRESET_SUCCESS ||
                 options.m_Preset == VIBRATE_PRESET_WARNING || options.m_Preset == VIBRATE_PRESET_ERROR)
            mode = VIBRATE_IOS_MODE_NOTIFICATION;
        else if (options.m_Preset == VIBRATE_PRESET_SELECTION)
            mode = VIBRATE_IOS_MODE_SELECTION;
        else if ((options.m_Preset >= VIBRATE_PRESET_IMPACT_LIGHT && options.m_Preset <= VIBRATE_PRESET_IMPACT_RIGID) ||
                 options.m_Preset == VIBRATE_PRESET_CLICK || options.m_Preset == VIBRATE_PRESET_DOUBLE_CLICK ||
                 options.m_Preset == VIBRATE_PRESET_TICK || options.m_Preset == VIBRATE_PRESET_HEAVY_CLICK)
            mode = VIBRATE_IOS_MODE_IMPACT;
        else if (options.m_HasDuration || options.m_HasIntensity || options.m_HasSharpness)
            mode = VIBRATE_IOS_MODE_CORE_HAPTICS;
        else
            mode = VIBRATE_IOS_MODE_SYSTEM;
    }

    switch (mode)
    {
        case VIBRATE_IOS_MODE_SELECTION:
            return PlaySelection();
        case VIBRATE_IOS_MODE_IMPACT:
            return PlayImpact(options.m_HasIOSStyle ? options.m_IOSStyle : StyleForPreset(options.m_Preset), options.m_Intensity);
        case VIBRATE_IOS_MODE_NOTIFICATION:
        {
            int notification = options.m_IOSNotification;
            if (!options.m_HasIOSNotification)
            {
                if (options.m_Preset == VIBRATE_PRESET_WARNING)
                    notification = VIBRATE_IOS_NOTIFICATION_WARNING;
                else if (options.m_Preset == VIBRATE_PRESET_ERROR)
                    notification = VIBRATE_IOS_NOTIFICATION_ERROR;
            }
            return PlayNotification(notification);
        }
        case VIBRATE_IOS_MODE_CORE_HAPTICS:
        {
            if (options.m_DurationMs == 0 && options.m_Pattern.empty() && options.m_IOSEvents.empty() && options.m_IOSAhapJson.empty())
                return VibratePlatform_Cancel();
            const VibrateResult result = PlayCoreHaptics(options);
            if (result == VIBRATE_RESULT_NO_HARDWARE && options.m_Fallback)
                return PlaySystemVibration();
            return result;
        }
        case VIBRATE_IOS_MODE_SYSTEM:
        default:
            if (options.m_HasDuration && options.m_DurationMs == 0)
                return VibratePlatform_Cancel();
            return PlaySystemVibration();
    }
}

VibrateResult VibratePlatform_Cancel()
{
    return StopPlayer();
}

void VibratePlatform_GetCapabilities(VibrateCapabilities* capabilities)
{
    capabilities->m_Platform = "ios";
    capabilities->m_Supported = true;
    capabilities->m_Predefined = true;

    const bool core_haptics = SupportsCoreHaptics();
    capabilities->m_CoreHaptics = core_haptics;
    capabilities->m_Cancel = core_haptics;
    capabilities->m_Pattern = core_haptics;
    capabilities->m_Intensity = core_haptics;
    capabilities->m_Sharpness = core_haptics;
    capabilities->m_Composition = core_haptics;
    capabilities->m_Repeat = core_haptics;
}

void VibratePlatform_Initialize()
{
}

void VibratePlatform_Finalize()
{
    VibratePlatform_Cancel();
    if (@available(iOS 13.0, *))
    {
        ReleasePlayer();
        if (g_HapticEngine)
        {
            [g_HapticEngine stopWithCompletionHandler:nil];
            [g_HapticEngine release];
            g_HapticEngine = nil;
        }
    }
}

#endif
