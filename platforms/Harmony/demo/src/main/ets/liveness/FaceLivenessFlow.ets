import { FaceAction, FaceLivenessConfig } from 'hiface';

/**
 * Random-action and passive-liveness policy matching Android FaceLivenessActivity.
 *
 * Android FaceLivenessActivity 的随机动作和静默活体策略。
 */
export class FaceLivenessFlow {
  static readonly LIVENESS_ACTION_COUNT: number = 3;

  static config(actions: ReadonlyArray<FaceAction>): FaceLivenessConfig {
    return new FaceLivenessConfig(actions, 3000, 10000, true);
  }
}
