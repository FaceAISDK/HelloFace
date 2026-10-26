/* Evaluate one upright, unmirrored GRAY8 frame from a real IR sensor.
 * Usage: hiface_infrared_liveness frame.gray W H STRIDE X Y FACE_W FACE_H THRESHOLD
  * 对真实红外传感器采集的已归正、未镜像 GRAY8 单帧执行检测；参数依次为图像文件、宽高、行步长、人脸框与真人阈值。
 */
#include <hiface/hiface_windows.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int parse_dimension(const char* text, int* result) {
    char* end = NULL;
    long value = strtol(text, &end, 10);
    if (!text[0] || !end || *end || value < 1 || value > INT_MAX) return 0;
    *result = (int)value;
    return 1;
}

int main(int argc, char** argv) {
    if (argc != 10) {
        fprintf(stderr, "Usage: %s frame.gray WIDTH HEIGHT STRIDE X Y FACE_W FACE_H THRESHOLD\n", argv[0]);
        return 2;
    }
    HiFaceInfraredLivenessRequestV1 request = {0};
    request.struct_size = sizeof(request);
    if (!parse_dimension(argv[2], &request.width) || !parse_dimension(argv[3], &request.height)
            || !parse_dimension(argv[4], &request.row_stride) || request.width > 4096
            || request.height > 4096 || request.row_stride < request.width) return 2;
    float* values[] = {&request.face_x, &request.face_y, &request.face_width,
                       &request.face_height, &request.real_threshold};
    for (int i = 0; i < 5; ++i) {
        char* end = NULL; *values[i] = strtof(argv[5 + i], &end);
        if (!argv[5 + i][0] || !end || *end) return 2;
    }
    FILE* file = fopen(argv[1], "rb");
    if (!file || fseek(file, 0, SEEK_END) != 0) { if (file) fclose(file); return 2; }
    long bytes = ftell(file);
    if (bytes < 1 || bytes > 64L * 1024 * 1024 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file); return 2;
    }
    uint8_t* gray = (uint8_t*)malloc((size_t)bytes);
    if (!gray || fread(gray, 1, (size_t)bytes, file) != (size_t)bytes) {
        free(gray); fclose(file); return 2;
    }
    fclose(file); request.gray = gray; request.gray_size = (size_t)bytes;

    HiFaceRuntimeConfigV1 runtime = {0}; runtime.struct_size = sizeof(runtime);
    runtime.api_version = HIFACE_C_API_VERSION_V1; runtime.cpu_threads = 2;
    HiFaceStatus status = hiface_runtime_configure_v1(&runtime);
    HiFaceConfigV1 config = {0}; config.struct_size = sizeof(config);
    config.api_version = HIFACE_C_API_VERSION_V1; config.compute_mode = HIFACE_COMPUTE_CPU;
    HiFaceSessionHandle session = NULL;
    if (status == HIFACE_STATUS_OK) status = hiface_session_create_v1(&config, &session);
    HiFaceModelLoadReportV1 report = {0}; report.struct_size = sizeof(report);
    if (status == HIFACE_STATUS_OK) status = hiface_windows_session_load_model_v1(
        session, HIFACE_MODEL_INFRARED_LIVENESS, &report);
    HiFaceInfraredLivenessResultV1 result = {0}; result.struct_size = sizeof(result);
    if (status == HIFACE_STATUS_OK)
        status = hiface_session_evaluate_infrared_liveness_v1(session, &request, &result);
    if (status == HIFACE_STATUS_OK)
        printf("real_score=%.9g spoof_score=%.9g passed=%u\n",
               result.real_score, result.spoof_score, result.passed);
    else fprintf(stderr, "IR inference failed: status=%d\n", status);
    hiface_session_destroy_v1(session); free(gray);
    return status == HIFACE_STATUS_OK ? 0 : 1;
}
