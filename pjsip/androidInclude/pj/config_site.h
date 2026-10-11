#ifndef CONFIG_SITE_H
#define CONFIG_SITE_H

#if defined(ANDROID) || defined(__ANDROID__)
    /* ========================================= */
    /* Android Configuration                     */
    /* ========================================= */
    #define PJ_CONFIG_ANDROID 1
    #define PJMEDIA_VIDEO_DEV_HAS_ANDROID 1
    #define PJMEDIA_AUDIO_DEV_HAS_ANDROID 1

#else
    /* ========================================= */
    /* Desktop / Windows Configuration           */
    /* ========================================= */
    /* Ensure iLBC codec default mode is explicitly 30ms */
    #ifndef PJMEDIA_ILBC_DEFAULT_MODE
    #define PJMEDIA_ILBC_DEFAULT_MODE 30
    #endif

#endif

/* Shared / Common configuration */
#include <pj/config_site_sample.h>

#define PJMEDIA_HAS_VIDEO 1

/* Optional: disable features not needed */
#undef PJ_HAS_SSL_SOCK
#define PJ_HAS_SSL_SOCK 0

#endif /* CONFIG_SITE_H */
