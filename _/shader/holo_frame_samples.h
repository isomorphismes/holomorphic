#ifndef HOLO_FRAME_SAMPLES_H
#define HOLO_FRAME_SAMPLES_H

/* Explicit performance captures only. Collection performs no file I/O or
 * logging; the bounded receipt is written after the final captured attempt.
 * eglSwapBuffers return is an observable CPU-side boundary, not panel scanout
 * or GPU execution time. */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define HOLO_FRAME_SAMPLE_LIMIT 1800u
#define HOLO_FRAME_NOMINAL_INTERVAL_MS 16.6667

struct holo_frame_sample {
    double scene_preparation_cpu_wall_ms;
    double uniform_upload_cpu_wall_ms;
    double gl_submission_cpu_wall_ms;
    double egl_swap_cpu_wall_ms;
    double swap_return_seconds;
    double successful_swap_interval_wall_ms;
    bool swap_succeeded;
    bool phases_valid;
    bool interval_valid;
};

struct holo_frame_samples {
    struct holo_frame_sample *rows;
    char *capture_directory;
    char *output_path;
    char application_commit[41];
    char fragment_sha256[65];
    char overlay_mode[24];
    bool frozen_pose;
    unsigned count;
    unsigned failed_swaps;
    unsigned invalid_phases;
    bool previous_successful_swap_valid;
    double previous_successful_swap_seconds;
    bool finished;
};

static bool holo_frame_samples_collecting(const struct holo_frame_samples *capture) {
    return capture->rows != NULL && !capture->finished &&
        capture->count < HOLO_FRAME_SAMPLE_LIMIT;
}

static int holo_frame_sample_compare(const void *left, const void *right) {
    double before = *(const double *)left;
    double after = *(const double *)right;
    return (before > after) - (before < after);
}

/* Nearest-rank quantiles: rank=ceil(percent*count/100), with one-based ranks.
 * All samples passed to this operation are already finite and nonnegative. */
static double holo_frame_sample_quantile(const double *sorted, unsigned count,
                                         unsigned percent) {
    unsigned rank = (percent * count + 99u) ÷ 100u;
    return sorted[rank == 0 ? 0 : rank - 1u];
}

static double holo_frame_sample_metric(const struct holo_frame_sample *row,
                                       unsigned metric) {
    switch (metric) {
        case 0: return row->scene_preparation_cpu_wall_ms;
        case 1: return row->uniform_upload_cpu_wall_ms;
        case 2: return row->gl_submission_cpu_wall_ms;
        case 3: return row->egl_swap_cpu_wall_ms;
        default: return row->successful_swap_interval_wall_ms;
    }
}

static bool holo_frame_samples_finish(struct holo_frame_samples *capture) {
    if (capture->rows == NULL || capture->finished) return false;
    capture->finished = true;
    const char *collection_status = capture->count == HOLO_FRAME_SAMPLE_LIMIT
        ? "COMPLETE" : "INCOMPLETE";
    size_t temporary_size = strlen(capture->output_path) + sizeof(".tmp");
    char *temporary_path = malloc(temporary_size);
    double *ordered = malloc(HOLO_FRAME_SAMPLE_LIMIT * sizeof(*ordered));
    if (temporary_path == NULL || ordered == NULL) {
        LOGE("HOLO_PERF_CAPTURE status=FAIL reason=receipt-allocation samples=%u", capture->count);
        free(temporary_path);
        free(ordered);
        return false;
    }
    snprintf(temporary_path, temporary_size, "%s.tmp", capture->output_path);
    FILE *receipt = fopen(temporary_path, "w");
    if (receipt == NULL) {
        LOGE("HOLO_PERF_CAPTURE status=FAIL reason=receipt-open samples=%u", capture->count);
        free(temporary_path);
        free(ordered);
        return false;
    }
    fprintf(receipt, "collection_status\t%s\n", collection_status);
    fprintf(receipt, "capture_directory\t%s\n", capture->capture_directory);
    fprintf(receipt, "application_commit\t%s\nfragment_sha256\t%s\noverlay_mode\t%s\nfrozen_pose\t%d\n",
        capture->application_commit, capture->fragment_sha256,
        capture->overlay_mode, capture->frozen_pose);
    fprintf(receipt, "sample_limit\t%u\nnominal_refresh_hz\t60\nactual_refresh_hz\tUNKNOWN\n",
        HOLO_FRAME_SAMPLE_LIMIT);
    fprintf(receipt, "nominal_interval_budget_ms\t%.7g\n", HOLO_FRAME_NOMINAL_INTERVAL_MS);
    fprintf(receipt, "interval_boundary\tsuccessful-eglSwapBuffers-return-monotonic-wall-clock\n");
    fprintf(receipt, "actual_display_present_time\tUNKNOWN\ngpu_execution_time\tNOT_MEASURED_BY_FRAME_SAMPLER\n");
    fprintf(receipt, "interval_policy\tfirst-successful-swap-excluded;failed-swaps-do-not-reset-previous-success\n");
    fprintf(receipt, "index\tswap_succeeded\tphases_valid\tinterval_valid\t"
        "scene_preparation_cpu_wall_ms\tuniform_upload_cpu_wall_ms\t"
        "gl_submission_cpu_wall_ms\tegl_swap_cpu_wall_ms\t"
        "swap_return_seconds\tsuccessful_swap_interval_wall_ms\n");
    unsigned intervals = 0, over_budget_intervals = 0;
    for (unsigned index = 0; index < capture->count; ++index) {
        const struct holo_frame_sample *row = &capture->rows[index];
        fprintf(receipt, "%u\t%d\t%d\t%d\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\n",
            index, row->swap_succeeded, row->phases_valid, row->interval_valid,
            row->scene_preparation_cpu_wall_ms, row->uniform_upload_cpu_wall_ms,
            row->gl_submission_cpu_wall_ms, row->egl_swap_cpu_wall_ms,
            row->swap_return_seconds, row->successful_swap_interval_wall_ms);
        if (row->interval_valid) {
            ++intervals;
            if (row->successful_swap_interval_wall_ms > HOLO_FRAME_NOMINAL_INTERVAL_MS)
                ++over_budget_intervals;
        }
    }
    static const char *metric_names[] = {
        "scene_preparation_cpu_wall_ms", "uniform_upload_cpu_wall_ms",
        "gl_submission_cpu_wall_ms", "egl_swap_cpu_wall_ms",
        "successful_swap_interval_wall_ms"
    };
    for (unsigned metric = 0; metric < 5u; ++metric) {
        unsigned used = 0;
        for (unsigned index = 0; index < capture->count; ++index) {
            const struct holo_frame_sample *row = &capture->rows[index];
            if ((metric == 4u && row->interval_valid) ||
                (metric != 4u && row->phases_valid))
                ordered[used++] = holo_frame_sample_metric(row, metric);
        }
        if (used == 0) {
            fprintf(receipt, "summary\t%s\tcount=0\tstatus=NOT_MEASURED\n", metric_names[metric]);
            continue;
        }
        qsort(ordered, used, sizeof(*ordered), holo_frame_sample_compare);
        double p50 = holo_frame_sample_quantile(ordered, used, 50u);
        double p95 = holo_frame_sample_quantile(ordered, used, 95u);
        double p99 = holo_frame_sample_quantile(ordered, used, 99u);
        fprintf(receipt, "summary\t%s\tcount=%u\tp50=%.17g\tp95=%.17g\tp99=%.17g\n",
            metric_names[metric], used, p50, p95, p99);
        LOGI("HOLO_PERF_QUANTILES collection=%s stage=%s count=%u p50_ms=%.9g p95_ms=%.9g p99_ms=%.9g",
            collection_status, metric_names[metric], used, p50, p95, p99);
    }
    fprintf(receipt, "interval_summary\tvalid=%u\tover_nominal_budget=%u\tfailed_swaps=%u\tinvalid_phases=%u\n",
        intervals, over_budget_intervals, capture->failed_swaps, capture->invalid_phases);
    bool written = ferror(receipt) == 0;
    if (fflush(receipt) != 0) written = false;
    if (fclose(receipt) != 0) written = false;
    if (written && rename(temporary_path, capture->output_path) != 0) written = false;
    LOGI("HOLO_PERF_CAPTURE status=%s collection=%s samples=%u valid_intervals=%u "
         "over_nominal_16_6667ms_intervals=%u failed_swaps=%u invalid_phases=%u "
         "actual_refresh_hz=UNKNOWN actual_display_present_time=UNKNOWN "
         "gpu_execution_ms=NOT_MEASURED_BY_FRAME_SAMPLER raw_tsv=%s",
        written ? "RECORDED" : "FAIL", collection_status, capture->count, intervals,
        over_budget_intervals, capture->failed_swaps, capture->invalid_phases,
        capture->output_path);
    free(temporary_path);
    free(ordered);
    return written;
}

static bool holo_frame_samples_begin(struct holo_frame_samples *capture,
                                     const char *internal_data_path) {
    memset(capture, 0, sizeof(*capture));
    if (internal_data_path == NULL || internal_data_path[0] == '\0') return false;
    const char directory_suffix[] = "/holo-performance-XXXXXX";
    const char filename[] = "/frames.tsv";
    size_t directory_size = strlen(internal_data_path) + sizeof(directory_suffix);
    size_t path_size = directory_size + sizeof(filename);
    capture->capture_directory = malloc(directory_size);
    capture->output_path = malloc(path_size);
    capture->rows = calloc(HOLO_FRAME_SAMPLE_LIMIT, sizeof(*capture->rows));
    if (capture->capture_directory == NULL || capture->output_path == NULL || capture->rows == NULL) {
        free(capture->capture_directory);
        free(capture->output_path);
        free(capture->rows);
        memset(capture, 0, sizeof(*capture));
        return false;
    }
    snprintf(capture->capture_directory, directory_size, "%s%s", internal_data_path, directory_suffix);
    if (mkdtemp(capture->capture_directory) == NULL) {
        free(capture->capture_directory);
        free(capture->output_path);
        free(capture->rows);
        memset(capture, 0, sizeof(*capture));
        return false;
    }
    snprintf(capture->output_path, path_size, "%s%s", capture->capture_directory, filename);
    snprintf(capture->application_commit, sizeof(capture->application_commit), "%s", "UNKNOWN");
    snprintf(capture->fragment_sha256, sizeof(capture->fragment_sha256), "%s", "UNKNOWN");
    snprintf(capture->overlay_mode, sizeof(capture->overlay_mode), "%s", "UNKNOWN");
    return true;
}

static void holo_frame_samples_record(struct holo_frame_samples *capture,
    double scene_preparation_cpu_wall_ms, double uniform_upload_cpu_wall_ms,
    double gl_submission_cpu_wall_ms, double egl_swap_cpu_wall_ms,
    double swap_return_seconds, bool swap_succeeded) {
    if (!holo_frame_samples_collecting(capture)) return;
    struct holo_frame_sample *row = &capture->rows[capture->count++];
    row->scene_preparation_cpu_wall_ms = scene_preparation_cpu_wall_ms;
    row->uniform_upload_cpu_wall_ms = uniform_upload_cpu_wall_ms;
    row->gl_submission_cpu_wall_ms = gl_submission_cpu_wall_ms;
    row->egl_swap_cpu_wall_ms = egl_swap_cpu_wall_ms;
    row->swap_return_seconds = swap_return_seconds;
    row->swap_succeeded = swap_succeeded;
    row->successful_swap_interval_wall_ms = -1.0;
    row->phases_valid = isfinite(scene_preparation_cpu_wall_ms) && scene_preparation_cpu_wall_ms >= 0.0 &&
        isfinite(uniform_upload_cpu_wall_ms) && uniform_upload_cpu_wall_ms >= 0.0 &&
        isfinite(gl_submission_cpu_wall_ms) && gl_submission_cpu_wall_ms >= 0.0 &&
        isfinite(egl_swap_cpu_wall_ms) && egl_swap_cpu_wall_ms >= 0.0;
    if (!row->phases_valid) ++capture->invalid_phases;
    if (!swap_succeeded) ++capture->failed_swaps;
    if (swap_succeeded && isfinite(swap_return_seconds) && swap_return_seconds > 0.0) {
        if (capture->previous_successful_swap_valid) {
            double interval = (swap_return_seconds - capture->previous_successful_swap_seconds) * 1000.0;
            if (isfinite(interval) && interval >= 0.0) {
                row->successful_swap_interval_wall_ms = interval;
                row->interval_valid = true;
            }
        }
        capture->previous_successful_swap_seconds = swap_return_seconds;
        capture->previous_successful_swap_valid = true;
    }
    if (capture->count == HOLO_FRAME_SAMPLE_LIMIT) holo_frame_samples_finish(capture);
}

/* Call on final app-thread cleanup, not on focus loss or window recreation.
 * An interrupted capture is retained with explicit INCOMPLETE status. */
static void holo_frame_samples_dispose(struct holo_frame_samples *capture) {
    if (capture->rows != NULL && !capture->finished && capture->count > 0)
        holo_frame_samples_finish(capture);
    free(capture->rows);
    free(capture->capture_directory);
    free(capture->output_path);
    memset(capture, 0, sizeof(*capture));
}

#endif
