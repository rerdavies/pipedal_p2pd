// build configuration for WPA supplicant.

#ifndef BUILD_CONFIG_H
#define BUILD_CONFIG_H

/* Insert configuration defines, e.g., #define EAP_MD5, here, if needed. */

#ifdef CONFIG_WIN32_DEFAULTS
#define CONFIG_NATIVE_WINDOWS
#define CONFIG_ANSI_C_EXTRA
#define CONFIG_WINPCAP
#define IEEE8021X_EAPOL
#define PKCS12_FUNCS
#define PCSC_FUNCS

#ifndef CONFIG_CTRL_IFACE
#define CONFIG_CTRL_IFACE
#endif

#define CONFIG_CTRL_IFACE_NAMED_PIPE
#define CONFIG_DRIVER_NDIS
#define CONFIG_NDIS_EVENTS_INTEGRATED
#define CONFIG_DEBUG_FILE
#define EAP_MD5
#define EAP_TLS
#define EAP_MSCHAPv2
#define EAP_PEAP
#define EAP_TTLS
#define EAP_GTC
#define EAP_OTP
#define EAP_LEAP
#define EAP_TNC
#define _CRT_SECURE_NO_DEPRECATE

#ifdef USE_INTERNAL_CRYPTO
#define CONFIG_TLS_INTERNAL_CLIENT
#define CONFIG_INTERNAL_LIBTOMMATH
#define CONFIG_CRYPTO_INTERNAL
#endif /* USE_INTERNAL_CRYPTO */
#else  /* NOT CONFIG_WIN32_DEFAULTS */

#pragma GCC diagnostic ignored "-Wvariadic-macros"

#ifndef CONFIG_CTRL_IFACE
#define CONFIG_CTRL_IFACE
#endif

#endif

#endif /* BUILD_CONFIG_H */