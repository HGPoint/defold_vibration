var LibraryVibrate = {
    VibrateWeb_Trigger__proxy: "sync",
    VibrateWeb_Trigger: function(patternPtr, patternCount) {
        var nav = typeof navigator !== "undefined" ? navigator : null;
        if (!nav || typeof nav.vibrate !== "function")
            return 1; // VIBRATE_RESULT_UNSUPPORTED

        if (typeof document !== "undefined" && document.visibilityState !== "visible")
            return 3; // VIBRATE_RESULT_NOT_VISIBLE

        if (nav.userActivation && nav.userActivation.hasBeenActive === false)
            return 2; // VIBRATE_RESULT_NOT_ALLOWED

        try {
            var pattern = new Array(patternCount);
            var offset = patternPtr >>> 2;
            for (var i = 0; i < patternCount; ++i)
                pattern[i] = HEAPU32[offset + i] >>> 0;

            return nav.vibrate(pattern) ? 0 : 4; // OK / REJECTED
        } catch (error) {
            return 5; // VIBRATE_RESULT_PLATFORM_ERROR
        }
    },

    VibrateWeb_Cancel__proxy: "sync",
    VibrateWeb_Cancel: function() {
        var nav = typeof navigator !== "undefined" ? navigator : null;
        if (!nav || typeof nav.vibrate !== "function")
            return 1; // VIBRATE_RESULT_UNSUPPORTED

        try {
            return nav.vibrate(0) ? 0 : 4; // OK / REJECTED
        } catch (error) {
            return 5; // VIBRATE_RESULT_PLATFORM_ERROR
        }
    },

    VibrateWeb_IsSupported__proxy: "sync",
    VibrateWeb_IsSupported: function() {
        return typeof navigator !== "undefined" && typeof navigator.vibrate === "function" ? 1 : 0;
    },

    VibrateWeb_Initialize__proxy: "sync",
    VibrateWeb_Initialize: function() {
        // No permission prompt or persistent native state exists for this API.
    },

    VibrateWeb_Finalize__proxy: "sync",
    VibrateWeb_Finalize: function() {
        var nav = typeof navigator !== "undefined" ? navigator : null;
        if (!nav || typeof nav.vibrate !== "function")
            return;

        try {
            nav.vibrate(0);
        } catch (error) {
            // The runtime is shutting down, so there is nothing useful to report.
        }
    }
};

addToLibrary(LibraryVibrate);
