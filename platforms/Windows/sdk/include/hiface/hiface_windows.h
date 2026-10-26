#ifndef HIFACE_WINDOWS_H
#define HIFACE_WINDOWS_H
#include "hiface/hiface_c_api.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Load one built-in model into a session. No model files or downloads are required.
 * Call prepare_loaded_models after loading the capabilities your application uses.
 * Session ownership, threading and report initialization follow the common C API.
 */
HIFACE_API HiFaceStatus HIFACE_CALL hiface_windows_session_load_model_v1(
        HiFaceSessionHandle session, HiFaceModelIdV1 model_id,
        HiFaceModelLoadReportV1* out_report);
/** Export verified product information as UTF-8 JSON; does not grant access.
 * Output requires room for the terminating NUL; out_size excludes that NUL.
 */
HIFACE_API HiFaceStatus HIFACE_CALL hiface_windows_license_profile_v1(
        char* output, size_t capacity, size_t* out_size);
#ifdef __cplusplus
}
#endif
#endif
