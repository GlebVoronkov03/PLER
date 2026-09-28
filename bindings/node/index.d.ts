export type AlignMode = 0 | 1 | 2 | 3;

export interface PlerOptions {
  num_rays: number;
  min_rays: number;
  max_rays: number;
  align_mode: number;
  compute_tsi: number;
  converge: number;
  prefer_cuda: number;
  voxel_resolution: number;
  sphere_margin: number;
  cache_dir: string;
}

export interface PlerResult {
  pler_db: number;
  mse: number;
  mean_error: number;
  max_error: number;
  peak: number;
  miss_rate_ref: number;
  miss_rate_test: number;
  num_rays: number;
  computation_time_s: number;
  tsi: number;
  volume_iou: number;
  ok: boolean;
  backend: string;
  error: string;
  return_code?: number;
}

export declare const AlignMode: {
  OFF: 0;
  VOLUME_IOU: 1;
  ICP: 2;
  IOU_THEN_ICP: 3;
};

export declare function versionString(): string;
export declare function versionMajor(): number;
export declare function versionMinor(): number;
export declare function versionPatch(): number;
export declare function buildHasCuda(): boolean;
export declare function optionsInit(): PlerOptions;
export declare function lastError(): string;
export declare function computeFiles(
  ref: string,
  test: string,
  options?: Partial<PlerOptions>
): PlerResult;
export declare function cliPath(): string;
export declare function platformKey(): string;
