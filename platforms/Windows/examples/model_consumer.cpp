#include <hiface/hiface_windows.h>
#include <iostream>
#include <vector>
int main() {
    HiFaceSessionHandle session = nullptr;
    try {
        HiFaceRuntimeConfigV1 runtime{sizeof(runtime), HIFACE_C_API_VERSION_V1, 0, 0};
        if (hiface_runtime_configure_v1(&runtime) != HIFACE_STATUS_OK) throw "Runtime configuration failed";
        HiFaceConfigV1 config{};
        config.struct_size = sizeof(config); config.api_version = HIFACE_C_API_VERSION_V1;
        config.eye_closed_ear_threshold = HIFACE_DEFAULT_EYE_CLOSED_EAR_THRESHOLD;
        config.mouth_open_mar_threshold = HIFACE_DEFAULT_MOUTH_OPEN_MAR_THRESHOLD;
        config.compute_mode = HIFACE_COMPUTE_CPU; config.enable_feature = config.enable_landmark = 1;
        if (hiface_session_create_v1(&config, &session) != HIFACE_STATUS_OK) throw "Session creation failed";
        HiFaceModelLoadReportV1 report{}; report.struct_size = sizeof(report);
        if (hiface_windows_session_load_model_v1(nullptr, HIFACE_MODEL_DETECTOR, &report) != HIFACE_STATUS_INVALID_ARGUMENT)
            throw "Null session must be rejected";
        if (hiface_windows_session_load_model_v1(session, static_cast<HiFaceModelIdV1>(99), &report) != HIFACE_STATUS_INVALID_ARGUMENT)
            throw "Invalid model ID must be rejected";
        for (int id = HIFACE_MODEL_DETECTOR; id <= HIFACE_MODEL_SILENT_LIVENESS_M; ++id)
            if (hiface_windows_session_load_model_v1(session, static_cast<HiFaceModelIdV1>(id), &report) != HIFACE_STATUS_OK)
                throw "Built-in model loading failed";
        if (hiface_session_prepare_loaded_models_v1(session, &report) != HIFACE_STATUS_OK) throw "Preparation failed";
        if (report.detector != HIFACE_MODEL_LOADED || report.feature != HIFACE_MODEL_LOADED
            || report.landmarks != HIFACE_MODEL_LOADED || report.silent_liveness_s != HIFACE_MODEL_LOADED
            || report.silent_liveness_m != HIFACE_MODEL_LOADED) throw "Incomplete model report";
        std::vector<uint8_t> rgba(640 * 480 * 4, 128);
        for (size_t i = 3; i < rgba.size(); i += 4) rgba[i] = 255;
        HiFaceFaceV1 faces[10]{};
        HiFaceFrameResultV1 result{}; result.struct_size = sizeof(result); result.faces = faces; result.capacity = 10;
        HiFaceDetectionRequestV1 request{sizeof(request), HIFACE_C_API_VERSION_V1, 10, 0};
        if (hiface_session_process_rgba_v1(session, rgba.data(), rgba.size(), 640, 480, &request, &result) != HIFACE_STATUS_OK || result.count)
            throw "Blank image detection failed";
        hiface_session_destroy_v1(session);
        std::cout << "Built-in CPU models and blank RGBA detection passed without external model files\n";
        return 0;
    } catch (const char* error) { hiface_session_destroy_v1(session); std::cerr << error << '\n'; return 1; }
}
