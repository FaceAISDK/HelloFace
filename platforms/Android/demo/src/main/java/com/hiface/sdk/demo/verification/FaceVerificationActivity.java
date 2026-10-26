package com.hiface.sdk.demo.verification;

import android.os.Bundle;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;

import com.hiface.sdk.common.HiFaceError;
import com.hiface.sdk.demo.R;
import com.hiface.sdk.demo.camera.CameraFlowActivity;
import com.hiface.sdk.demo.ui.DemoErrorUi;
import com.hiface.sdk.demo.ui.FaceFlowUi;
import com.hiface.sdk.flow.FaceFlowListener;
import com.hiface.sdk.flow.FaceFlowProgress;
import com.hiface.sdk.flow.FaceFlowResult;
import com.hiface.sdk.verification.FaceVerificationConfig;

import java.util.List;
import java.util.Locale;

/**
 * Choose a saved Demo Face ID and run combined action liveness, passive liveness, and dynamic 1:1
 * authentication.
 *
 * 从 Demo 已保存的 Face ID 中选择一个，并执行动作 + 静默活体 + 动态 1:1 复合认证。
 */
public final class FaceVerificationActivity extends CameraFlowActivity {
    private static final int ACTION_COUNT = 2;
    private FaceFeatureStore featureStore;
    private String faceId;
    private String faceFeature;
    private AlertDialog dialog;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        featureStore = new FaceFeatureStore();
        captureOverlay.setStatusText(R.string.verification_select_face);

        List<String> ids = featureStore.getFaceIds();
        if (ids.isEmpty()) {
            Toast.makeText(this, R.string.verification_no_face_id, Toast.LENGTH_SHORT).show();
            finish();
            return;
        }
        showFaceIdSelection(ids);
    }

    private void showFaceIdSelection(@NonNull List<String> ids) {
        String[] items = ids.toArray(new String[0]);
        dialog = new AlertDialog.Builder(this)
                .setTitle(R.string.verification_select_face)
                .setItems(items, (dialog, which) -> selectFaceId(items[which]))
                .setOnCancelListener(dialog -> finish())
                .show();
    }

    private void selectFaceId(@NonNull String selectedId) {
        String feature = featureStore.getFeature(selectedId);
        if (feature == null || feature.trim().isEmpty()) {
            Toast.makeText(
                    getApplicationContext(),
                    R.string.verification_feature_missing,
                    Toast.LENGTH_SHORT).show();
            finish();
            return;
        }
        dialog = null;
        faceId = selectedId;
        faceFeature = feature;
        initializeCameraSdk();
        startCameraFlow();
    }

    @Override
    protected void startCameraFlow() {
        if (!canStartCameraFlow()
                || (dialog != null && dialog.isShowing())) return;
        FaceVerificationConfig config = FaceVerificationConfig.builder()
                .referenceFeature(faceFeature)
                .threshold(FaceVerificationConfig.DEFAULT_THRESHOLD)
                // Configure zero to four random actions; passive liveness is enabled independently.
                // 可配置零到四个随机动作；静默活体独立启用。
                .actions(FaceFlowUi.randomActions(ACTION_COUNT))
                .enableSilentLiveness(true)
                .allowMultipleFaces(true) // 多人画面继续处理最大人脸 / Continue with the largest face when multiple faces are visible.
                .build();

        startCameraFlowRequest(() -> hiFaceSdk.verification().startCameraVerification(
                config,
                new FaceFlowListener() {
                    @Override
                    public void onProgress(@NonNull FaceFlowProgress progress) {
                        if (isCameraFlowRunning()) {
                            FaceFlowUi.renderVerifyProgress(
                                    captureOverlay,
                                    progress,
                                    config.getActionTimeoutMillis());
                        }
                    }

                    @Override
                    public void onResult(@NonNull FaceFlowResult result) {
                        if (!finishCameraFlow()) return;
                        showVerificationResult(result);
                    }

                    /**
                     * Invalid parameters or an underlying SDK failure prevented completion, so no business decision was
                     * produced.
                     *
                     * 传入参数错误，SDK底层异常导致SDK 无法完成这次请求，没有形成业务结论
                     */
                    @Override
                    public void onError(@NonNull HiFaceError error) {
                        if (!finishCameraFlow()) return;
                        showVerificationError(error);
                    }
                }));
    }

    /**
     * Show similarity for both match and mismatch; show only the rejection reason if the flow ends early.
     *
     * 匹配和不匹配展示相似度；流程提前结束时只展示拒绝原因。
     */
    private void showVerificationResult(@NonNull FaceFlowResult result) {
        String message;
        switch (result.getOutcome()) {
            case PASSED:
                message = getString(R.string.verification_match,
                        faceId, formatSimilarityPercent(result.getSimilarity()));
                break;
            case NOT_MATCHED:
                message = getString(R.string.verification_mismatch,
                        faceId, formatSimilarityPercent(result.getSimilarity()));
                break;
            default:
                // The flow did not complete identity comparison; show only the rejection reason.
                // 流程未完成身份比对，只展示拒绝原因。
                message = FaceFlowUi.rejectionMessage(this, result.getOutcome());
                break;
        }
        // All business outcomes allow retry; handle SDK execution errors separately.
        // 业务结果均允许重试；SDK 执行错误由错误处理方法单独判断。
        showCompletionDialog(message, true);
    }

    /**
     * The SDK guarantees similarity for match/mismatch; retain a null fallback and show a placeholder
     * instead of 0%.
     *
     * SDK 保证匹配/不匹配时有相似度；保留空值兜底，显示占位符而不是 0%。
     */
    @NonNull
    private static String formatSimilarityPercent(@Nullable Float similarity) {
        return similarity == null
                ? "-" : String.format(Locale.getDefault(), "%.2f%%", similarity * 100f);
    }

    /**
     * Display SDK execution errors and offer retry only for recoverable failures.
     *
     * 显示 SDK 执行错误；仅为可恢复错误提供重试入口。
     */
    private void showVerificationError(@NonNull HiFaceError error) {
        showCompletionDialog(DemoErrorUi.messageFor(this, error), DemoErrorUi.isRetryable(error));
    }

    private void showCompletionDialog(@NonNull String message, boolean canRetry) {
        AlertDialog.Builder builder = new AlertDialog.Builder(this)
                .setMessage(message)
                .setNegativeButton(R.string.verification_close, (ignored, which) -> finish())
                .setCancelable(false)
                .setOnCancelListener(ignored -> finish());
        if (canRetry) {
            builder.setPositiveButton(
                    R.string.verification_retry,
                    (ignored, which) -> retryVerification());
        }
        dialog = builder.show();
    }

    private void retryVerification() {
        dialog = null;
        startCameraFlow();
    }

    @Override
    protected void onDestroy() {
        if (dialog != null) dialog.dismiss();
        super.onDestroy();
    }
}
