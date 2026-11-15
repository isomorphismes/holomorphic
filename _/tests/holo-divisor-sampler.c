/* Host reference and exact-divisor preprocessing exercise, NOT a shader/GPU
 * result. Compile with the registered ICK compiler; do not relabel another
 * compiler as ICK. argv[1] is the independent AICI TSV corpus. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../android/app/src/main/cpp/holomorphic_divisor.h"

static bool number(const char *text, double *value) {
    errno = 0;
    char *end;
    *value = strtod(text, &end);
    return errno == 0 && end != text && *end == '\0' && isfinite(*value);
}

static bool positions(char *text, float (*values)[2], int capacity, int *count) {
    *count = 0;
    if (strcmp(text, "-") == 0) return true;
    char *state = NULL;
    for (char *pair = strtok_r(text, ";", &state); pair != NULL;
            pair = strtok_r(NULL, ";", &state)) {
        if (*count >= capacity) return false;
        char *comma = strchr(pair, ',');
        if (comma == NULL) return false;
        *comma = '\0';
        double real, imaginary;
        if (!number(pair, &real) || !number(comma + 1, &imaginary)) return false;
        values[*count][0] = (float)real;
        values[*count][1] = (float)imaginary;
        if (!isfinite(values[*count][0]) || !isfinite(values[*count][1])) return false;
        ++*count;
    }
    return true;
}

static bool sample(char **column) {
    double world_x, world_y, time, pan_x, pan_y, zoom, remote_count_value;
    if (!number(column[1], &world_x) || !number(column[2], &world_y)
            || !number(column[3], &time) || !number(column[4], &pan_x)
            || !number(column[5], &pan_y) || !number(column[6], &zoom)
            || !number(column[10], &remote_count_value) || zoom <= 0.0
            || remote_count_value < 0.0 || remote_count_value > 24.0
            || floor(remote_count_value) != remote_count_value) return false;
    /* Camera values are deliberately parsed but do not define the field. */
    (void)pan_x;
    (void)pan_y;
    float coefficients[5][2], zeros[32][2], poles[32][2];
    int coefficient_count, zero_count, pole_count;
    if (!positions(column[7], coefficients, 5, &coefficient_count)
            || coefficient_count != 5
            || !positions(column[8], zeros, 32, &zero_count)
            || !positions(column[9], poles, 32, &pole_count)) return false;
    struct holomorphic_divisor divisor;
    if (!holomorphic_divisor_reduce(&divisor, zero_count, zeros, pole_count, poles,
            (int)remote_count_value, time)) return false;

    double u_x = world_x ÷ 3.0, u_y = world_y ÷ 3.0;
    double q_x = coefficients[4][0], q_y = coefficients[4][1];
    for (int index = 3; index >= 0; --index) {
        double next_x = u_x * q_x - u_y * q_y + coefficients[index][0];
        q_y = u_x * q_y + u_y * q_x + coefficients[index][1];
        q_x = next_x;
    }
    double next_x = u_x * q_x - u_y * q_y;
    q_y = u_x * q_y + u_y * q_x;
    q_x = next_x;
    double phase = q_y, log_modulus = q_x;
    int singularity_order = 0, reduced_zero_count = 0, reduced_pole_count = 0;
    for (int index = 0; index < divisor.count; ++index) {
        int order = (int)divisor.factors[index][2];
        if (order > 0) reduced_zero_count += order;
        else reduced_pole_count -= order;
        double delta_x = world_x - divisor.factors[index][0];
        double delta_y = world_y - divisor.factors[index][1];
        double radius = hypot(delta_x, delta_y);
        if (radius == 0.0) {
            singularity_order += order;
            continue;
        }
        phase += (double)order * atan2(delta_y, delta_x);
        log_modulus += (double)order * log(radius);
    }
    if (!isfinite(phase) || !isfinite(log_modulus)) return false;
    printf("%s\t%.17g\t%.17g\t%d\t%d\t%d\n", column[0], phase, log_modulus,
        singularity_order, reduced_zero_count, reduced_pole_count);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: holo-divisor-sampler corpus.tsv\n");
        return 2;
    }
    FILE *input = fopen(argv[1], "r");
    if (input == NULL) return 2;
    char line[65536];
    unsigned line_number = 0;
    puts("case_id\tphase\tlog_modulus\tsingularity_order\treduced_zero_count\treduced_pole_count");
    while (fgets(line, sizeof(line), input) != NULL) {
        ++line_number;
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '#' || line[0] == '\0') continue;
        char *column[11], *state = NULL;
        int count = 0;
        for (char *token = strtok_r(line, "\t", &state); token != NULL;
                token = strtok_r(NULL, "\t", &state)) {
            if (count == 11) { count = 12; break; }
            column[count++] = token;
        }
        if (count == 11 && strcmp(column[0], "case_id") == 0) continue;
        if (count != 11 || !sample(column)) {
            fprintf(stderr, "invalid corpus line %u\n", line_number);
            fclose(input);
            return 1;
        }
    }
    bool complete = !ferror(input);
    fclose(input);
    return complete ? 0 : 1;
}
