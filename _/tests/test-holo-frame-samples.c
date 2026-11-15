/* Synthetic statistics/receipt tests only: no rendering or performance claim. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOGI(...) ((void)0)
#define LOGE(...) ((void)0)
#include "../shader/holo_frame_samples.h"

#define TEST_APPLICATION_COMMIT "1111111111111111111111111111111111111111"
#define TEST_FRAGMENT_SHA256 "2222222222222222222222222222222222222222222222222222222222222222"

static bool file_exists(const char *path) {
    FILE *file = fopen(path, "r");
    if (file == NULL) return false;
    assert(fclose(file) == 0);
    return true;
}

static void make_directory(char path[PATH_MAX], const char *root, const char *kind) {
    int length = snprintf(path, PATH_MAX, "%s/frame-samples-%s-XXXXXX", root, kind);
    assert(length > 0 && length < PATH_MAX);
    assert(mkdtemp(path) != NULL);
}

static void verify_complete_receipt(const char *path) {
    FILE *receipt = fopen(path, "r");
    assert(receipt != NULL);
    char line[2048];
    unsigned rows = 0, summaries = 0, previous_successful_index = 0;
    bool complete = false, intervals = false, first_excluded = false;
    bool application = false, fragment = false, overlay = false, frozen = false;
    while (fgets(line, sizeof(line), receipt) != NULL) {
        if (strcmp(line, "collection_status\tCOMPLETE\n") == 0) complete = true;
        if (strcmp(line, "application_commit\t" TEST_APPLICATION_COMMIT "\n") == 0) application = true;
        if (strcmp(line, "fragment_sha256\t" TEST_FRAGMENT_SHA256 "\n") == 0) fragment = true;
        if (strcmp(line, "overlay_mode\tlocalized\n") == 0) overlay = true;
        if (strcmp(line, "frozen_pose\t1\n") == 0) frozen = true;
        if (strcmp(line, "interval_summary\tvalid=1797\tover_nominal_budget=1\tfailed_swaps=2\tinvalid_phases=0\n") == 0)
            intervals = true;
        if (strncmp(line, "summary\t", 8) == 0) {
            char stage[100];
            unsigned count = 0;
            double p50 = -1.0, p95 = -1.0, p99 = -1.0;
            assert(sscanf(line, "summary\t%99s\tcount=%u\tp50=%lf\tp95=%lf\tp99=%lf",
                stage, &count, &p50, &p95, &p99) == 5);
            if (strcmp(stage, "successful_swap_interval_wall_ms") == 0) {
                assert(count == 1797);
                assert(fabs(p50 - 10.0) < 0.000001);
                assert(fabs(p95 - 10.0) < 0.000001);
                assert(fabs(p99 - 10.0) < 0.000001);
            } else {
                double offset;
                if (strcmp(stage, "scene_preparation_cpu_wall_ms") == 0) offset = 0.0;
                else if (strcmp(stage, "uniform_upload_cpu_wall_ms") == 0) offset = 10000.0;
                else if (strcmp(stage, "gl_submission_cpu_wall_ms") == 0) offset = 20000.0;
                else {
                    assert(strcmp(stage, "egl_swap_cpu_wall_ms") == 0);
                    offset = 30000.0;
                }
                assert(count == 1800);
                assert(p50 == 899.0 + offset && p95 == 1709.0 + offset && p99 == 1781.0 + offset);
            }
            ++summaries;
        }
        if (!isdigit((unsigned char)line[0])) continue;
        unsigned index;
        int success, phases_valid, interval_valid;
        double preparation, upload, submission, swap, returned, interval;
        assert(sscanf(line, "%u\t%d\t%d\t%d\t%lf\t%lf\t%lf\t%lf\t%lf\t%lf",
            &index, &success, &phases_valid, &interval_valid,
            &preparation, &upload, &submission, &swap, &returned, &interval) == 10);
        assert(index == rows && phases_valid == 1);
        assert(preparation == (double)index && upload == (double)index + 10000.0);
        assert(submission == (double)index + 20000.0 && swap == (double)index + 30000.0);
        assert(fabs(returned - (1000.0 + (double)index * 0.01)) < 0.000000001);
        bool expected_success = index != 100 && index != 101;
        assert(success == (int)expected_success);
        if (index == 0 || !expected_success) {
            assert(interval_valid == 0 && interval == -1.0);
            if (index == 0) first_excluded = true;
        } else {
            assert(interval_valid == 1);
            double expected = (double)(index - previous_successful_index) * 10.0;
            assert(fabs(interval - expected) < 0.000001);
        }
        if (expected_success) previous_successful_index = index;
        ++rows;
    }
    assert(ferror(receipt) == 0 && fclose(receipt) == 0);
    assert(rows == 1800 && summaries == 5 && complete && intervals && first_excluded);
    assert(application && fragment && overlay && frozen);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    char directory[PATH_MAX], path[PATH_MAX];
    make_directory(directory, argv[1], "complete");
    struct holo_frame_samples capture;
    assert(holo_frame_samples_begin(&capture, directory));
    snprintf(capture.application_commit, sizeof(capture.application_commit), "%s", TEST_APPLICATION_COMMIT);
    snprintf(capture.fragment_sha256, sizeof(capture.fragment_sha256), "%s", TEST_FRAGMENT_SHA256);
    snprintf(capture.overlay_mode, sizeof(capture.overlay_mode), "%s", "localized");
    capture.frozen_pose = true;
    assert(strlen(capture.output_path) < sizeof(path));
    strcpy(path, capture.output_path);
    for (unsigned index = 0; index < 1800; ++index) {
        assert(holo_frame_samples_collecting(&capture));
        assert(!file_exists(path));
        holo_frame_samples_record(&capture, (double)index, (double)index + 10000.0,
            (double)index + 20000.0, (double)index + 30000.0,
            1000.0 + (double)index * 0.01, index != 100 && index != 101);
    }
    assert(capture.finished && capture.count == 1800 && !holo_frame_samples_collecting(&capture));
    holo_frame_samples_record(&capture, -1.0, -1.0, -1.0, -1.0, -1.0, false);
    assert(capture.count == 1800);
    verify_complete_receipt(path);
    holo_frame_samples_dispose(&capture);
    assert(capture.rows == NULL && capture.output_path == NULL);
    verify_complete_receipt(path);

    /* A restarted capture under the same app-data parent must never point at
     * its predecessor's COMPLETE receipt, even if killed before the first row. */
    char previous_path[PATH_MAX];
    strcpy(previous_path, path);
    assert(holo_frame_samples_begin(&capture, directory));
    assert(strcmp(capture.output_path, previous_path) != 0);
    assert(!file_exists(capture.output_path));
    verify_complete_receipt(previous_path);
    assert(strlen(capture.output_path) < sizeof(path));
    strcpy(path, capture.output_path);
    holo_frame_samples_dispose(&capture);
    assert(!file_exists(path));
    verify_complete_receipt(previous_path);

    assert(holo_frame_samples_begin(&capture, directory));
    assert(strcmp(capture.output_path, previous_path) != 0);
    assert(strcmp(capture.output_path, path) != 0);
    assert(strlen(capture.output_path) < sizeof(path));
    strcpy(path, capture.output_path);
    holo_frame_samples_record(&capture, 1.0, 2.0, 3.0, 4.0, 2000.0, true);
    holo_frame_samples_record(&capture, NAN, 2.0, 3.0, 4.0, 2000.01, false);
    assert(!file_exists(path));
    holo_frame_samples_dispose(&capture);
    FILE *partial = fopen(path, "r");
    assert(partial != NULL);
    char line[2048];
    unsigned partial_rows = 0;
    bool incomplete = false, invalid = false;
    while (fgets(line, sizeof(line), partial) != NULL) {
        if (strcmp(line, "collection_status\tINCOMPLETE\n") == 0) incomplete = true;
        if (strcmp(line, "interval_summary\tvalid=0\tover_nominal_budget=0\tfailed_swaps=1\tinvalid_phases=1\n") == 0)
            invalid = true;
        if (isdigit((unsigned char)line[0])) ++partial_rows;
    }
    assert(ferror(partial) == 0 && fclose(partial) == 0);
    assert(incomplete && invalid && partial_rows == 2);
    puts("PASS synthetic frame statistics: bounded raw rows, quantiles, failed-swap gaps, first interval and incomplete/invalid receipt; no device performance evidence");
    return 0;
}
