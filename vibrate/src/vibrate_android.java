package com.defold.android.vibrate;

import android.app.Activity;
import android.content.Context;
import android.media.AudioAttributes;
import android.os.Build;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.os.VibratorManager;
import android.util.Log;

class VibrateExtension {
    private static final String TAG = "vibrate";

    private static final int RESULT_OK = 0;
    private static final int RESULT_UNSUPPORTED = 1;
    private static final int RESULT_NOT_ALLOWED = 2;
    private static final int RESULT_REJECTED = 4;
    private static final int RESULT_PLATFORM_ERROR = 5;
    private static final int RESULT_NO_HARDWARE = 6;

    private static final int CAPABILITY_SUPPORTED = 1 << 0;
    private static final int CAPABILITY_PATTERN = 1 << 1;
    private static final int CAPABILITY_INTENSITY = 1 << 2;
    private static final int CAPABILITY_CANCEL = 1 << 3;
    private static final int CAPABILITY_PREDEFINED = 1 << 4;
    private static final int CAPABILITY_COMPOSITION = 1 << 5;
    private static final int CAPABILITY_REPEAT = 1 << 6;

    private static final int EFFECT_CLICK = 0;
    private static final int EFFECT_DOUBLE_CLICK = 1;
    private static final int EFFECT_TICK = 2;
    private static final int EFFECT_HEAVY_CLICK = 3;

    private static final int USAGE_TOUCH = 0;
    private static final int USAGE_GAME = 1;
    private static final int USAGE_NOTIFICATION = 2;
    private static final int USAGE_ALARM = 3;
    private static final int USAGE_MEDIA = 4;

    private static final int PRIMITIVE_CLICK = 0;
    private static final int PRIMITIVE_THUD = 1;
    private static final int PRIMITIVE_SPIN = 2;
    private static final int PRIMITIVE_QUICK_RISE = 3;
    private static final int PRIMITIVE_SLOW_RISE = 4;
    private static final int PRIMITIVE_QUICK_FALL = 5;
    private static final int PRIMITIVE_TICK = 6;
    private static final int PRIMITIVE_LOW_TICK = 7;

    private VibrateExtension() {
    }

    public static int TriggerOneShot(
            Activity activity,
            long durationMs,
            int amplitude,
            int usage,
            boolean fallback) {
        try {
            Vibrator vibrator = getVibrator(activity);
            if (!hasHardware(vibrator)) {
                return RESULT_NO_HARDWARE;
            }
            return triggerOneShot(vibrator, durationMs, amplitude, usage, fallback);
        } catch (SecurityException exception) {
            Log.w(TAG, "Vibration is not allowed", exception);
            return RESULT_NOT_ALLOWED;
        } catch (IllegalArgumentException exception) {
            Log.w(TAG, "Invalid one-shot vibration", exception);
            return RESULT_REJECTED;
        } catch (Throwable exception) {
            Log.e(TAG, "Unable to trigger one-shot vibration", exception);
            return RESULT_PLATFORM_ERROR;
        }
    }

    public static int TriggerWaveform(
            Activity activity,
            long[] timings,
            int[] amplitudes,
            int repeatIndex,
            int usage,
            boolean fallback) {
        try {
            Vibrator vibrator = getVibrator(activity);
            if (!hasHardware(vibrator)) {
                return RESULT_NO_HARDWARE;
            }
            if (!isValidWaveform(timings, amplitudes, repeatIndex)) {
                return RESULT_REJECTED;
            }
            if (!hasActiveWaveformSegment(timings, amplitudes)) {
                vibrator.cancel();
                return RESULT_OK;
            }

            AudioAttributes attributes = createAudioAttributes(usage);
            if (Build.VERSION.SDK_INT >= 26) {
                if (amplitudes != null
                        && containsVariableAmplitude(amplitudes)
                        && !Api26.hasAmplitudeControl(vibrator)
                        && !fallback) {
                    return RESULT_UNSUPPORTED;
                }
                Api26.vibrateWaveform(vibrator, timings, amplitudes, repeatIndex, attributes);
                return RESULT_OK;
            }

            if (amplitudes != null
                    && !isLegacyAlternatingWaveform(timings, amplitudes)
                    && !fallback) {
                return RESULT_UNSUPPORTED;
            }
            if (amplitudes != null && containsVariableAmplitude(amplitudes) && !fallback) {
                return RESULT_UNSUPPORTED;
            }
            vibrateLegacyWaveform(vibrator, timings, repeatIndex, attributes);
            return RESULT_OK;
        } catch (SecurityException exception) {
            Log.w(TAG, "Vibration is not allowed", exception);
            return RESULT_NOT_ALLOWED;
        } catch (IllegalArgumentException exception) {
            Log.w(TAG, "Invalid waveform vibration", exception);
            return RESULT_REJECTED;
        } catch (Throwable exception) {
            Log.e(TAG, "Unable to trigger waveform vibration", exception);
            return RESULT_PLATFORM_ERROR;
        }
    }

    public static int TriggerPredefined(
            Activity activity,
            int effect,
            int usage,
            boolean fallback,
            long fallbackDurationMs,
            int fallbackAmplitude) {
        try {
            Vibrator vibrator = getVibrator(activity);
            if (!hasHardware(vibrator)) {
                return RESULT_NO_HARDWARE;
            }
            if (!isValidEffect(effect)) {
                return RESULT_REJECTED;
            }

            if (Build.VERSION.SDK_INT >= 29) {
                if (!fallback
                        && (Build.VERSION.SDK_INT < 30
                                || !Api30.isEffectSupported(vibrator, effect))) {
                    return RESULT_UNSUPPORTED;
                }
                Api29.vibratePredefined(vibrator, effect, createAudioAttributes(usage));
                return RESULT_OK;
            }
            if (!fallback) {
                return RESULT_UNSUPPORTED;
            }
            return triggerOneShot(vibrator, fallbackDurationMs, fallbackAmplitude, usage, true);
        } catch (SecurityException exception) {
            Log.w(TAG, "Vibration is not allowed", exception);
            return RESULT_NOT_ALLOWED;
        } catch (IllegalArgumentException exception) {
            Log.w(TAG, "Invalid predefined vibration", exception);
            return RESULT_REJECTED;
        } catch (Throwable exception) {
            Log.e(TAG, "Unable to trigger predefined vibration", exception);
            return RESULT_PLATFORM_ERROR;
        }
    }

    public static int TriggerComposition(
            Activity activity,
            int[] primitiveTypes,
            float[] primitiveScales,
            long[] primitiveDelaysMs,
            int usage,
            boolean fallback,
            long fallbackDurationMs,
            int fallbackAmplitude) {
        try {
            Vibrator vibrator = getVibrator(activity);
            if (!hasHardware(vibrator)) {
                return RESULT_NO_HARDWARE;
            }
            if (!isValidComposition(primitiveTypes, primitiveScales, primitiveDelaysMs)) {
                return RESULT_REJECTED;
            }

            if (Build.VERSION.SDK_INT >= 30) {
                int[] androidPrimitives = Api30.toAndroidPrimitives(primitiveTypes);
                if (androidPrimitives == null) {
                    return RESULT_REJECTED;
                }
                if (!Api30.arePrimitivesSupported(vibrator, androidPrimitives)) {
                    if (!fallback) {
                        return RESULT_UNSUPPORTED;
                    }
                    return triggerOneShot(vibrator, fallbackDurationMs, fallbackAmplitude, usage, true);
                }
                Api30.vibrateComposition(
                        vibrator,
                        androidPrimitives,
                        primitiveScales,
                        primitiveDelaysMs,
                        createAudioAttributes(usage));
                return RESULT_OK;
            }
            if (!fallback) {
                return RESULT_UNSUPPORTED;
            }
            return triggerOneShot(vibrator, fallbackDurationMs, fallbackAmplitude, usage, true);
        } catch (SecurityException exception) {
            Log.w(TAG, "Vibration is not allowed", exception);
            return RESULT_NOT_ALLOWED;
        } catch (IllegalArgumentException exception) {
            Log.w(TAG, "Invalid composed vibration", exception);
            return RESULT_REJECTED;
        } catch (Throwable exception) {
            Log.e(TAG, "Unable to trigger composed vibration", exception);
            return RESULT_PLATFORM_ERROR;
        }
    }

    public static int Cancel(Activity activity) {
        try {
            Vibrator vibrator = getVibrator(activity);
            if (!hasHardware(vibrator)) {
                return RESULT_NO_HARDWARE;
            }
            vibrator.cancel();
            return RESULT_OK;
        } catch (SecurityException exception) {
            Log.w(TAG, "Vibration cancellation is not allowed", exception);
            return RESULT_NOT_ALLOWED;
        } catch (Throwable exception) {
            Log.e(TAG, "Unable to cancel vibration", exception);
            return RESULT_PLATFORM_ERROR;
        }
    }

    public static int GetCapabilities(Activity activity) {
        try {
            Vibrator vibrator = getVibrator(activity);
            if (!hasHardware(vibrator)) {
                return 0;
            }

            int capabilities = CAPABILITY_SUPPORTED
                    | CAPABILITY_PATTERN
                    | CAPABILITY_CANCEL
                    | CAPABILITY_REPEAT;
            if (Build.VERSION.SDK_INT >= 26 && Api26.hasAmplitudeControl(vibrator)) {
                capabilities |= CAPABILITY_INTENSITY;
            }
            if (Build.VERSION.SDK_INT >= 29) {
                capabilities |= CAPABILITY_PREDEFINED;
            }
            if (Build.VERSION.SDK_INT >= 30 && Api30.hasAnySupportedPrimitive(vibrator)) {
                capabilities |= CAPABILITY_COMPOSITION;
            }
            return capabilities;
        } catch (Throwable exception) {
            Log.e(TAG, "Unable to read vibration capabilities", exception);
            return 0;
        }
    }

    private static Vibrator getVibrator(Activity activity) {
        if (activity == null) {
            return null;
        }
        if (Build.VERSION.SDK_INT >= 31) {
            Vibrator vibrator = Api31.getDefaultVibrator(activity);
            if (vibrator != null) {
                return vibrator;
            }
        }
        return (Vibrator) activity.getSystemService(Context.VIBRATOR_SERVICE);
    }

    private static boolean hasHardware(Vibrator vibrator) {
        return vibrator != null && vibrator.hasVibrator();
    }

    private static int triggerOneShot(
            Vibrator vibrator,
            long durationMs,
            int amplitude,
            int usage,
            boolean fallback) {
        if (durationMs < 0 || amplitude < -1 || amplitude > 255) {
            return RESULT_REJECTED;
        }
        if (durationMs == 0 || amplitude == 0) {
            vibrator.cancel();
            return RESULT_OK;
        }

        AudioAttributes attributes = createAudioAttributes(usage);
        if (Build.VERSION.SDK_INT >= 26) {
            if (amplitude != -1
                    && amplitude != 255
                    && !Api26.hasAmplitudeControl(vibrator)
                    && !fallback) {
                return RESULT_UNSUPPORTED;
            }
            Api26.vibrateOneShot(vibrator, durationMs, amplitude, attributes);
            return RESULT_OK;
        }

        if (amplitude != -1 && amplitude != 255 && !fallback) {
            return RESULT_UNSUPPORTED;
        }
        vibrateLegacyOneShot(vibrator, durationMs, attributes);
        return RESULT_OK;
    }

    private static AudioAttributes createAudioAttributes(int usage) {
        int androidUsage;
        switch (usage) {
            case USAGE_GAME:
                androidUsage = AudioAttributes.USAGE_GAME;
                break;
            case USAGE_NOTIFICATION:
                androidUsage = AudioAttributes.USAGE_NOTIFICATION;
                break;
            case USAGE_ALARM:
                androidUsage = AudioAttributes.USAGE_ALARM;
                break;
            case USAGE_MEDIA:
                androidUsage = AudioAttributes.USAGE_MEDIA;
                break;
            case USAGE_TOUCH:
            default:
                androidUsage = AudioAttributes.USAGE_ASSISTANCE_SONIFICATION;
                break;
        }

        return new AudioAttributes.Builder()
                .setUsage(androidUsage)
                .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                .build();
    }

    private static boolean isValidWaveform(long[] timings, int[] amplitudes, int repeatIndex) {
        if (timings == null || timings.length == 0) {
            return false;
        }
        if (amplitudes != null && amplitudes.length != timings.length) {
            return false;
        }
        if (repeatIndex < -1 || repeatIndex >= timings.length) {
            return false;
        }
        for (long timing : timings) {
            if (timing < 0) {
                return false;
            }
        }
        if (amplitudes != null) {
            for (int amplitude : amplitudes) {
                if (amplitude < -1 || amplitude > 255) {
                    return false;
                }
            }
        }
        return true;
    }

    private static boolean hasActiveWaveformSegment(long[] timings, int[] amplitudes) {
        for (int i = 0; i < timings.length; ++i) {
            if (timings[i] == 0) {
                continue;
            }
            if (amplitudes != null) {
                if (amplitudes[i] != 0) {
                    return true;
                }
            } else if ((i & 1) == 1) {
                return true;
            }
        }
        return false;
    }

    private static boolean containsVariableAmplitude(int[] amplitudes) {
        for (int amplitude : amplitudes) {
            if (amplitude > 0 && amplitude < 255) {
                return true;
            }
        }
        return false;
    }

    private static boolean isLegacyAlternatingWaveform(long[] timings, int[] amplitudes) {
        for (int i = 0; i < timings.length; ++i) {
            if (timings[i] == 0) {
                continue;
            }
            boolean shouldVibrate = (i & 1) == 1;
            boolean doesVibrate = amplitudes[i] != 0;
            if (shouldVibrate != doesVibrate) {
                return false;
            }
        }
        return true;
    }

    private static boolean isValidEffect(int effect) {
        return effect >= EFFECT_CLICK && effect <= EFFECT_HEAVY_CLICK;
    }

    private static boolean isValidComposition(
            int[] primitiveTypes,
            float[] primitiveScales,
            long[] primitiveDelaysMs) {
        if (primitiveTypes == null
                || primitiveScales == null
                || primitiveDelaysMs == null
                || primitiveTypes.length == 0
                || primitiveTypes.length != primitiveScales.length
                || primitiveTypes.length != primitiveDelaysMs.length) {
            return false;
        }
        for (int i = 0; i < primitiveTypes.length; ++i) {
            if (primitiveTypes[i] < PRIMITIVE_CLICK || primitiveTypes[i] > PRIMITIVE_LOW_TICK) {
                return false;
            }
            if (Float.isNaN(primitiveScales[i])
                    || primitiveScales[i] < 0.0f
                    || primitiveScales[i] > 1.0f
                    || primitiveDelaysMs[i] < 0
                    || primitiveDelaysMs[i] > Integer.MAX_VALUE) {
                return false;
            }
        }
        return true;
    }

    @SuppressWarnings("deprecation")
    private static void vibrateLegacyOneShot(
            Vibrator vibrator,
            long durationMs,
            AudioAttributes attributes) {
        vibrator.vibrate(durationMs, attributes);
    }

    @SuppressWarnings("deprecation")
    private static void vibrateLegacyWaveform(
            Vibrator vibrator,
            long[] timings,
            int repeatIndex,
            AudioAttributes attributes) {
        vibrator.vibrate(timings, repeatIndex, attributes);
    }

    private static final class Api26 {
        private Api26() {
        }

        static boolean hasAmplitudeControl(Vibrator vibrator) {
            return vibrator.hasAmplitudeControl();
        }

        static void vibrateOneShot(
                Vibrator vibrator,
                long durationMs,
                int amplitude,
                AudioAttributes attributes) {
            vibrator.vibrate(VibrationEffect.createOneShot(durationMs, amplitude), attributes);
        }

        static void vibrateWaveform(
                Vibrator vibrator,
                long[] timings,
                int[] amplitudes,
                int repeatIndex,
                AudioAttributes attributes) {
            VibrationEffect effect = amplitudes == null
                    ? VibrationEffect.createWaveform(timings, repeatIndex)
                    : VibrationEffect.createWaveform(timings, amplitudes, repeatIndex);
            vibrator.vibrate(effect, attributes);
        }
    }

    private static final class Api29 {
        private Api29() {
        }

        static int toAndroidEffect(int effect) {
            int androidEffect;
            switch (effect) {
                case EFFECT_DOUBLE_CLICK:
                    androidEffect = VibrationEffect.EFFECT_DOUBLE_CLICK;
                    break;
                case EFFECT_TICK:
                    androidEffect = VibrationEffect.EFFECT_TICK;
                    break;
                case EFFECT_HEAVY_CLICK:
                    androidEffect = VibrationEffect.EFFECT_HEAVY_CLICK;
                    break;
                case EFFECT_CLICK:
                default:
                    androidEffect = VibrationEffect.EFFECT_CLICK;
                    break;
            }
            return androidEffect;
        }

        static void vibratePredefined(
                Vibrator vibrator,
                int effect,
                AudioAttributes attributes) {
            vibrator.vibrate(
                    VibrationEffect.createPredefined(toAndroidEffect(effect)),
                    attributes);
        }
    }

    private static final class Api30 {
        private Api30() {
        }

        static boolean isEffectSupported(Vibrator vibrator, int effect) {
            return vibrator.areAllEffectsSupported(Api29.toAndroidEffect(effect))
                    == Vibrator.VIBRATION_EFFECT_SUPPORT_YES;
        }

        static boolean hasAnySupportedPrimitive(Vibrator vibrator) {
            int[] primitives = {
                    VibrationEffect.Composition.PRIMITIVE_CLICK,
                    VibrationEffect.Composition.PRIMITIVE_THUD,
                    VibrationEffect.Composition.PRIMITIVE_SPIN,
                    VibrationEffect.Composition.PRIMITIVE_QUICK_RISE,
                    VibrationEffect.Composition.PRIMITIVE_SLOW_RISE,
                    VibrationEffect.Composition.PRIMITIVE_QUICK_FALL,
                    VibrationEffect.Composition.PRIMITIVE_TICK,
                    VibrationEffect.Composition.PRIMITIVE_LOW_TICK,
            };
            boolean[] supported = vibrator.arePrimitivesSupported(primitives);
            for (boolean value : supported) {
                if (value) {
                    return true;
                }
            }
            return false;
        }

        static int[] toAndroidPrimitives(int[] primitives) {
            int[] result = new int[primitives.length];
            for (int i = 0; i < primitives.length; ++i) {
                switch (primitives[i]) {
                    case PRIMITIVE_CLICK:
                        result[i] = VibrationEffect.Composition.PRIMITIVE_CLICK;
                        break;
                    case PRIMITIVE_THUD:
                        result[i] = VibrationEffect.Composition.PRIMITIVE_THUD;
                        break;
                    case PRIMITIVE_SPIN:
                        result[i] = VibrationEffect.Composition.PRIMITIVE_SPIN;
                        break;
                    case PRIMITIVE_QUICK_RISE:
                        result[i] = VibrationEffect.Composition.PRIMITIVE_QUICK_RISE;
                        break;
                    case PRIMITIVE_SLOW_RISE:
                        result[i] = VibrationEffect.Composition.PRIMITIVE_SLOW_RISE;
                        break;
                    case PRIMITIVE_QUICK_FALL:
                        result[i] = VibrationEffect.Composition.PRIMITIVE_QUICK_FALL;
                        break;
                    case PRIMITIVE_TICK:
                        result[i] = VibrationEffect.Composition.PRIMITIVE_TICK;
                        break;
                    case PRIMITIVE_LOW_TICK:
                        result[i] = VibrationEffect.Composition.PRIMITIVE_LOW_TICK;
                        break;
                    default:
                        return null;
                }
            }
            return result;
        }

        static boolean arePrimitivesSupported(Vibrator vibrator, int[] primitives) {
            boolean[] supported = vibrator.arePrimitivesSupported(primitives);
            if (supported == null || supported.length != primitives.length) {
                return false;
            }
            for (boolean value : supported) {
                if (!value) {
                    return false;
                }
            }
            return true;
        }

        static void vibrateComposition(
                Vibrator vibrator,
                int[] primitives,
                float[] scales,
                long[] delaysMs,
                AudioAttributes attributes) {
            VibrationEffect.Composition composition = VibrationEffect.startComposition();
            for (int i = 0; i < primitives.length; ++i) {
                composition.addPrimitive(primitives[i], scales[i], (int) delaysMs[i]);
            }
            vibrator.vibrate(composition.compose(), attributes);
        }
    }

    private static final class Api31 {
        private Api31() {
        }

        static Vibrator getDefaultVibrator(Activity activity) {
            VibratorManager manager =
                    (VibratorManager) activity.getSystemService(Context.VIBRATOR_MANAGER_SERVICE);
            return manager == null ? null : manager.getDefaultVibrator();
        }
    }
}
