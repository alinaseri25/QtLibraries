#ifndef CONFIG_SITE_H
#define CONFIG_SITE_H

#if defined(ANDROID) || defined(__ANDROID__)
    #define PJ_CONFIG_ANDROID 1
    #define PJ_ANDROID 1
    #define PJ_IS_LITTLE_ENDIAN 1
    #define PJ_IS_BIG_ENDIAN 0
#endif

#include <pj/config_site_sample.h>

/* Custom features */
#define PJMEDIA_HAS_VIDEO 1
#define PJMEDIA_VIDEO_DEV_HAS_ANDROID 1
#define PJMEDIA_AUDIO_DEV_HAS_ANDROID 1

/* Optional: disable features not needed */
#undef PJ_HAS_SSL_SOCK
#define PJ_HAS_SSL_SOCK 0

#endif /* CONFIG_SITE_H */
