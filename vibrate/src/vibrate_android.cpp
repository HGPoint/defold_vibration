#include "vibrate_private.h"

#if defined(DM_PLATFORM_ANDROID)

#include <dmsdk/sdk.h>
#include <dmsdk/dlib/android.h>

namespace
{
    static const char* JAVA_CLASS = "com.defold.android.vibrate.VibrateExtension";

    enum AndroidCapability
    {
        ANDROID_CAPABILITY_SUPPORTED   = 1 << 0,
        ANDROID_CAPABILITY_PATTERN     = 1 << 1,
        ANDROID_CAPABILITY_INTENSITY   = 1 << 2,
        ANDROID_CAPABILITY_CANCEL      = 1 << 3,
        ANDROID_CAPABILITY_PREDEFINED  = 1 << 4,
        ANDROID_CAPABILITY_COMPOSITION = 1 << 5,
        ANDROID_CAPABILITY_REPEAT      = 1 << 6,
    };

    struct AndroidBindings
    {
        jclass m_Class;
        jmethodID m_TriggerOneShot;
        jmethodID m_TriggerWaveform;
        jmethodID m_TriggerPredefined;
        jmethodID m_TriggerComposition;
        jmethodID m_Cancel;
        jmethodID m_GetCapabilities;

        AndroidBindings()
        : m_Class(0)
        , m_TriggerOneShot(0)
        , m_TriggerWaveform(0)
        , m_TriggerPredefined(0)
        , m_TriggerComposition(0)
        , m_Cancel(0)
        , m_GetCapabilities(0)
        {
        }
    };

    static AndroidBindings g_Bindings;

    static bool ClearJavaException(JNIEnv* env, const char* operation)
    {
        if (!env || !env->ExceptionCheck())
            return false;

        dmLogError("Java exception while %s", operation);
        env->ExceptionDescribe();
        env->ExceptionClear();
        return true;
    }

    static void ResetBindings(JNIEnv* env)
    {
        if (env && g_Bindings.m_Class)
            env->DeleteGlobalRef(g_Bindings.m_Class);
        g_Bindings = AndroidBindings();
    }

    static bool LoadStaticMethod(JNIEnv* env, jmethodID* method, const char* name, const char* signature)
    {
        *method = env->GetStaticMethodID(g_Bindings.m_Class, name, signature);
        const bool had_exception = ClearJavaException(env, name);
        if (*method && !had_exception)
            return true;

        if (!had_exception)
            dmLogError("Unable to find Java method %s%s", name, signature);
        return false;
    }

    static bool EnsureBindings(JNIEnv* env)
    {
        if (g_Bindings.m_Class)
            return true;
        if (!env)
            return false;

        jclass local_class = dmAndroid::LoadClass(env, JAVA_CLASS);
        const bool load_exception = ClearJavaException(env, "loading VibrateExtension");
        if (!local_class || load_exception)
        {
            if (local_class)
                env->DeleteLocalRef(local_class);
            return false;
        }

        g_Bindings.m_Class = (jclass)env->NewGlobalRef(local_class);
        env->DeleteLocalRef(local_class);
        const bool global_ref_exception = ClearJavaException(env, "creating VibrateExtension global reference");
        if (!g_Bindings.m_Class || global_ref_exception)
        {
            ResetBindings(env);
            return false;
        }

        const bool loaded =
            LoadStaticMethod(env, &g_Bindings.m_TriggerOneShot, "TriggerOneShot", "(Landroid/app/Activity;JIIZ)I") &&
            LoadStaticMethod(env, &g_Bindings.m_TriggerWaveform, "TriggerWaveform", "(Landroid/app/Activity;[J[IIIZ)I") &&
            LoadStaticMethod(env, &g_Bindings.m_TriggerPredefined, "TriggerPredefined", "(Landroid/app/Activity;IIZJI)I") &&
            LoadStaticMethod(env, &g_Bindings.m_TriggerComposition, "TriggerComposition", "(Landroid/app/Activity;[I[F[JIZJI)I") &&
            LoadStaticMethod(env, &g_Bindings.m_Cancel, "Cancel", "(Landroid/app/Activity;)I") &&
            LoadStaticMethod(env, &g_Bindings.m_GetCapabilities, "GetCapabilities", "(Landroid/app/Activity;)I");

        if (!loaded)
            ResetBindings(env);
        return loaded;
    }

    static VibrateResult ToResult(jint result)
    {
        if (result < VIBRATE_RESULT_OK || result > VIBRATE_RESULT_NO_HARDWARE)
            return VIBRATE_RESULT_PLATFORM_ERROR;
        return (VibrateResult)result;
    }

    static int UnitFloatToAmplitude(float intensity)
    {
        if (intensity <= 0.0f)
            return 0;
        if (intensity >= 1.0f)
            return 255;
        int amplitude = (int)(intensity * 255.0f + 0.5f);
        return amplitude < 1 ? 1 : amplitude;
    }

    static int ResolveAmplitude(const VibrateOptions& options)
    {
        if (options.m_HasAndroidAmplitude)
            return options.m_AndroidAmplitude;
        return UnitFloatToAmplitude(options.m_Intensity);
    }

    static int ResolvePredefinedEffect(const VibrateOptions& options)
    {
        if (options.m_AndroidMode == VIBRATE_ANDROID_MODE_PREDEFINED || options.m_HasAndroidEffect)
            return options.m_AndroidEffect;

        switch (options.m_Preset)
        {
            case VIBRATE_PRESET_SELECTION:
            case VIBRATE_PRESET_IMPACT_LIGHT:
            case VIBRATE_PRESET_IMPACT_SOFT:
            case VIBRATE_PRESET_TICK:
                return VIBRATE_ANDROID_EFFECT_TICK;
            case VIBRATE_PRESET_SUCCESS:
            case VIBRATE_PRESET_DOUBLE_CLICK:
                return VIBRATE_ANDROID_EFFECT_DOUBLE_CLICK;
            case VIBRATE_PRESET_IMPACT_HEAVY:
            case VIBRATE_PRESET_WARNING:
            case VIBRATE_PRESET_ERROR:
            case VIBRATE_PRESET_HEAVY_CLICK:
                return VIBRATE_ANDROID_EFFECT_HEAVY_CLICK;
            default:
                return VIBRATE_ANDROID_EFFECT_CLICK;
        }
    }

    static int ResolveMode(const VibrateOptions& options)
    {
        if (options.m_AndroidMode != VIBRATE_ANDROID_MODE_AUTO)
            return options.m_AndroidMode;
        if (!options.m_AndroidPrimitives.empty())
            return VIBRATE_ANDROID_MODE_COMPOSITION;
        if (!options.m_AndroidTimings.empty())
            return VIBRATE_ANDROID_MODE_WAVEFORM;
        if (options.m_HasAndroidEffect)
            return VIBRATE_ANDROID_MODE_PREDEFINED;
        if (!options.m_Pattern.empty())
            return VIBRATE_ANDROID_MODE_WAVEFORM;
        if (options.m_Preset != VIBRATE_PRESET_DEFAULT)
            return VIBRATE_ANDROID_MODE_PREDEFINED;
        return VIBRATE_ANDROID_MODE_ONE_SHOT;
    }

    static jlongArray MakeLongArray(JNIEnv* env, const std::vector<uint32_t>& values)
    {
        if (values.empty())
            return 0;

        jlongArray result = env->NewLongArray((jsize)values.size());
        if (!result)
            return 0;

        std::vector<jlong> converted(values.size());
        for (uint32_t i = 0; i < values.size(); ++i)
            converted[i] = (jlong)values[i];
        env->SetLongArrayRegion(result, 0, (jsize)converted.size(), &converted[0]);
        return result;
    }

    static jintArray MakeIntArray(JNIEnv* env, const std::vector<int32_t>& values)
    {
        if (values.empty())
            return 0;

        jintArray result = env->NewIntArray((jsize)values.size());
        if (!result)
            return 0;

        std::vector<jint> converted(values.size());
        for (uint32_t i = 0; i < values.size(); ++i)
            converted[i] = (jint)values[i];
        env->SetIntArrayRegion(result, 0, (jsize)converted.size(), &converted[0]);
        return result;
    }

    static jfloatArray MakeFloatArray(JNIEnv* env, const std::vector<float>& values)
    {
        if (values.empty())
            return 0;

        jfloatArray result = env->NewFloatArray((jsize)values.size());
        if (!result)
            return 0;
        env->SetFloatArrayRegion(result, 0, (jsize)values.size(), &values[0]);
        return result;
    }

    static VibrateResult CallIntMethod(JNIEnv* env, jmethodID method)
    {
        jobject activity = dmGraphics::GetNativeAndroidActivity();
        if (!activity)
            return VIBRATE_RESULT_PLATFORM_ERROR;

        const jint result = env->CallStaticIntMethod(g_Bindings.m_Class, method, activity);
        if (ClearJavaException(env, "calling VibrateExtension"))
            return VIBRATE_RESULT_PLATFORM_ERROR;
        return ToResult(result);
    }

    static VibrateResult TriggerOneShot(JNIEnv* env, const VibrateOptions& options)
    {
        jobject activity = dmGraphics::GetNativeAndroidActivity();
        if (!activity)
            return VIBRATE_RESULT_PLATFORM_ERROR;

        const jint result = env->CallStaticIntMethod(
            g_Bindings.m_Class,
            g_Bindings.m_TriggerOneShot,
            activity,
            (jlong)options.m_DurationMs,
            (jint)ResolveAmplitude(options),
            (jint)options.m_AndroidUsage,
            (jboolean)options.m_Fallback);
        if (ClearJavaException(env, "triggering Android one-shot vibration"))
            return VIBRATE_RESULT_PLATFORM_ERROR;
        return ToResult(result);
    }

    static VibrateResult TriggerWaveform(JNIEnv* env, const VibrateOptions& options)
    {
        if (options.m_AndroidTimings.empty() && !options.m_AndroidAmplitudes.empty())
            return VIBRATE_RESULT_REJECTED;

        std::vector<uint32_t> timings;
        std::vector<int32_t> amplitudes;
        int repeat_index = options.m_AndroidRepeatIndex;

        if (!options.m_AndroidTimings.empty())
        {
            timings = options.m_AndroidTimings;
            amplitudes = options.m_AndroidAmplitudes;
            if (!amplitudes.empty() && amplitudes.size() != timings.size())
                return VIBRATE_RESULT_REJECTED;
        }
        else if (!options.m_Pattern.empty())
        {
            // Lua/web patterns start with vibration. Android waveform timings start
            // with a delay, therefore prepend a zero-duration OFF segment.
            timings.reserve(options.m_Pattern.size() + 1);
            amplitudes.reserve(options.m_Pattern.size() + 1);
            timings.push_back(0);
            amplitudes.push_back(0);

            const int default_amplitude = ResolveAmplitude(options);
            for (uint32_t i = 0; i < options.m_Pattern.size(); ++i)
            {
                timings.push_back(options.m_Pattern[i]);
                if ((i & 1) == 0)
                {
                    int amplitude = default_amplitude;
                    if (!options.m_HasAndroidAmplitude && !options.m_Intensities.empty())
                        amplitude = UnitFloatToAmplitude(options.m_Intensities[i]);
                    amplitudes.push_back(amplitude);
                }
                else
                {
                    amplitudes.push_back(0);
                }
            }

            if (repeat_index >= 0)
            {
                if (repeat_index >= (int)options.m_Pattern.size())
                    return VIBRATE_RESULT_REJECTED;
                ++repeat_index;
            }
        }
        else
        {
            if (repeat_index >= 0)
                return VIBRATE_RESULT_REJECTED;
            timings.push_back(0);
            timings.push_back(options.m_DurationMs);
            amplitudes.push_back(0);
            amplitudes.push_back(ResolveAmplitude(options));
        }

        jlongArray java_timings = MakeLongArray(env, timings);
        jintArray java_amplitudes = MakeIntArray(env, amplitudes);
        const bool array_exception = ClearJavaException(env, "creating Android waveform arrays");
        if (!java_timings || (!amplitudes.empty() && !java_amplitudes) || array_exception)
        {
            if (java_timings)
                env->DeleteLocalRef(java_timings);
            if (java_amplitudes)
                env->DeleteLocalRef(java_amplitudes);
            return VIBRATE_RESULT_PLATFORM_ERROR;
        }

        jobject activity = dmGraphics::GetNativeAndroidActivity();
        if (!activity)
        {
            env->DeleteLocalRef(java_timings);
            if (java_amplitudes)
                env->DeleteLocalRef(java_amplitudes);
            return VIBRATE_RESULT_PLATFORM_ERROR;
        }

        const jint result = env->CallStaticIntMethod(
            g_Bindings.m_Class,
            g_Bindings.m_TriggerWaveform,
            activity,
            java_timings,
            java_amplitudes,
            (jint)repeat_index,
            (jint)options.m_AndroidUsage,
            (jboolean)options.m_Fallback);

        env->DeleteLocalRef(java_timings);
        if (java_amplitudes)
            env->DeleteLocalRef(java_amplitudes);
        if (ClearJavaException(env, "triggering Android waveform vibration"))
            return VIBRATE_RESULT_PLATFORM_ERROR;
        return ToResult(result);
    }

    static VibrateResult TriggerPredefined(JNIEnv* env, const VibrateOptions& options)
    {
        jobject activity = dmGraphics::GetNativeAndroidActivity();
        if (!activity)
            return VIBRATE_RESULT_PLATFORM_ERROR;

        const jint result = env->CallStaticIntMethod(
            g_Bindings.m_Class,
            g_Bindings.m_TriggerPredefined,
            activity,
            (jint)ResolvePredefinedEffect(options),
            (jint)options.m_AndroidUsage,
            (jboolean)options.m_Fallback,
            (jlong)options.m_DurationMs,
            (jint)ResolveAmplitude(options));
        if (ClearJavaException(env, "triggering Android predefined vibration"))
            return VIBRATE_RESULT_PLATFORM_ERROR;
        return ToResult(result);
    }

    static VibrateResult TriggerComposition(JNIEnv* env, const VibrateOptions& options)
    {
        if (options.m_AndroidPrimitives.empty())
            return VIBRATE_RESULT_REJECTED;

        std::vector<int32_t> types;
        std::vector<float> scales;
        std::vector<uint32_t> delays;
        types.reserve(options.m_AndroidPrimitives.size());
        scales.reserve(options.m_AndroidPrimitives.size());
        delays.reserve(options.m_AndroidPrimitives.size());
        for (uint32_t i = 0; i < options.m_AndroidPrimitives.size(); ++i)
        {
            types.push_back(options.m_AndroidPrimitives[i].m_Type);
            scales.push_back(options.m_AndroidPrimitives[i].m_Scale);
            delays.push_back(options.m_AndroidPrimitives[i].m_DelayMs);
        }

        jintArray java_types = MakeIntArray(env, types);
        jfloatArray java_scales = MakeFloatArray(env, scales);
        jlongArray java_delays = MakeLongArray(env, delays);
        const bool array_exception = ClearJavaException(env, "creating Android composition arrays");
        if (!java_types || !java_scales || !java_delays || array_exception)
        {
            if (java_types)
                env->DeleteLocalRef(java_types);
            if (java_scales)
                env->DeleteLocalRef(java_scales);
            if (java_delays)
                env->DeleteLocalRef(java_delays);
            return VIBRATE_RESULT_PLATFORM_ERROR;
        }

        jobject activity = dmGraphics::GetNativeAndroidActivity();
        if (!activity)
        {
            env->DeleteLocalRef(java_types);
            env->DeleteLocalRef(java_scales);
            env->DeleteLocalRef(java_delays);
            return VIBRATE_RESULT_PLATFORM_ERROR;
        }

        const jint result = env->CallStaticIntMethod(
            g_Bindings.m_Class,
            g_Bindings.m_TriggerComposition,
            activity,
            java_types,
            java_scales,
            java_delays,
            (jint)options.m_AndroidUsage,
            (jboolean)options.m_Fallback,
            (jlong)options.m_DurationMs,
            (jint)ResolveAmplitude(options));

        env->DeleteLocalRef(java_types);
        env->DeleteLocalRef(java_scales);
        env->DeleteLocalRef(java_delays);
        if (ClearJavaException(env, "triggering Android composition vibration"))
            return VIBRATE_RESULT_PLATFORM_ERROR;
        return ToResult(result);
    }
}

VibrateResult VibratePlatform_Trigger(const VibrateOptions& options)
{
    const int mode = ResolveMode(options);
    if (!options.m_Fallback)
    {
        if (options.m_HasSharpness || !options.m_Sharpnesses.empty())
            return VIBRATE_RESULT_UNSUPPORTED;

        const bool uses_common_amplitude =
            mode == VIBRATE_ANDROID_MODE_ONE_SHOT ||
            (mode == VIBRATE_ANDROID_MODE_WAVEFORM && options.m_AndroidTimings.empty());
        const bool uses_common_pattern =
            mode == VIBRATE_ANDROID_MODE_WAVEFORM &&
            options.m_AndroidTimings.empty() && !options.m_Pattern.empty();
        if (options.m_HasIntensity && !uses_common_amplitude)
            return VIBRATE_RESULT_UNSUPPORTED;
        if (!options.m_Intensities.empty() && !uses_common_pattern)
            return VIBRATE_RESULT_UNSUPPORTED;
    }

    dmAndroid::ThreadAttacher thread_attacher;
    JNIEnv* env = thread_attacher.GetEnv();
    if (!EnsureBindings(env))
        return VIBRATE_RESULT_PLATFORM_ERROR;

    switch (mode)
    {
        case VIBRATE_ANDROID_MODE_ONE_SHOT:
            return TriggerOneShot(env, options);
        case VIBRATE_ANDROID_MODE_WAVEFORM:
            return TriggerWaveform(env, options);
        case VIBRATE_ANDROID_MODE_PREDEFINED:
            return TriggerPredefined(env, options);
        case VIBRATE_ANDROID_MODE_COMPOSITION:
            return TriggerComposition(env, options);
        default:
            return VIBRATE_RESULT_REJECTED;
    }
}

VibrateResult VibratePlatform_Cancel()
{
    dmAndroid::ThreadAttacher thread_attacher;
    JNIEnv* env = thread_attacher.GetEnv();
    if (!EnsureBindings(env))
        return VIBRATE_RESULT_PLATFORM_ERROR;
    return CallIntMethod(env, g_Bindings.m_Cancel);
}

void VibratePlatform_GetCapabilities(VibrateCapabilities* capabilities)
{
    if (!capabilities)
        return;

    capabilities->m_Platform = "android";

    dmAndroid::ThreadAttacher thread_attacher;
    JNIEnv* env = thread_attacher.GetEnv();
    if (!EnsureBindings(env))
        return;

    jobject activity = dmGraphics::GetNativeAndroidActivity();
    if (!activity)
        return;

    const jint flags = env->CallStaticIntMethod(g_Bindings.m_Class, g_Bindings.m_GetCapabilities, activity);
    if (ClearJavaException(env, "reading Android vibration capabilities"))
        return;

    capabilities->m_Supported = (flags & ANDROID_CAPABILITY_SUPPORTED) != 0;
    capabilities->m_Pattern = (flags & ANDROID_CAPABILITY_PATTERN) != 0;
    capabilities->m_Intensity = (flags & ANDROID_CAPABILITY_INTENSITY) != 0;
    capabilities->m_Cancel = (flags & ANDROID_CAPABILITY_CANCEL) != 0;
    capabilities->m_Predefined = (flags & ANDROID_CAPABILITY_PREDEFINED) != 0;
    capabilities->m_Composition = (flags & ANDROID_CAPABILITY_COMPOSITION) != 0;
    capabilities->m_Repeat = (flags & ANDROID_CAPABILITY_REPEAT) != 0;
}

void VibratePlatform_Initialize()
{
    dmAndroid::ThreadAttacher thread_attacher;
    EnsureBindings(thread_attacher.GetEnv());
}

void VibratePlatform_Finalize()
{
    dmAndroid::ThreadAttacher thread_attacher;
    ResetBindings(thread_attacher.GetEnv());
}

#endif
