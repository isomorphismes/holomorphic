#ifndef WEGERT_FACTOR_STATE_H
#define WEGERT_FACTOR_STATE_H

enum factor_change {
    FACTOR_UNCHANGED,
    FACTOR_APPENDED,
    FACTOR_CANCELLED_OPPOSITE
};

static int factor_find_exact(
    float factors[MAX_FACTORS][2],
    int factor_count,
    float real,
    float imag
) {
    for (int index = 0; index < factor_count; ++index) {
        if (factors[index][0] == real && factors[index][1] == imag) {
            return index;
        }
    }
    return -1;
}

static void factor_remove_at(
    float factors[MAX_FACTORS][2],
    int *factor_count,
    int removed_index
) {
    for (int index = removed_index; index + 1 < *factor_count; ++index) {
        factors[index][0] = factors[index + 1][0];
        factors[index][1] = factors[index + 1][1];
    }
    *factor_count -= 1;
}

static enum factor_change factor_insert_reduced(
    float same_kind[MAX_FACTORS][2],
    int *same_kind_count,
    float opposite_kind[MAX_FACTORS][2],
    int *opposite_kind_count,
    float real,
    float imag
) {
    int opposite_index = factor_find_exact(
        opposite_kind,
        *opposite_kind_count,
        real,
        imag
    );
    if (opposite_index >= 0) {
        factor_remove_at(opposite_kind, opposite_kind_count, opposite_index);
        return FACTOR_CANCELLED_OPPOSITE;
    }

    if (*same_kind_count >= MAX_FACTORS) {
        return FACTOR_UNCHANGED;
    }

    same_kind[*same_kind_count][0] = real;
    same_kind[*same_kind_count][1] = imag;
    *same_kind_count += 1;
    return FACTOR_APPENDED;
}

#endif

