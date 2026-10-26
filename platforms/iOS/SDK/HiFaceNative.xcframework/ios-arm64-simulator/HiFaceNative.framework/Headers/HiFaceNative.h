#ifndef HIFACE_NATIVE_FRAMEWORK_H
#define HIFACE_NATIVE_FRAMEWORK_H

#if __has_include(<HiFaceNative/hiface/hiface_c_api.h>)
#include <HiFaceNative/hiface/hiface_c_api.h>
#else
#include "hiface/hiface_c_api.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Link anchor and runtime compatibility probe used by the Objective-C++ adapter. */
HIFACE_API uint32_t HIFACE_CALL hiface_ios_native_api_version(void);
/** SDK-owned system identity reference; never contains device identifiers or keys. */
HIFACE_API HiFaceStatus HIFACE_CALL hiface_ios_license_profile_v1(char*, size_t, size_t*);

#ifdef __cplusplus
}
#endif

#endif /* HIFACE_NATIVE_FRAMEWORK_H */
