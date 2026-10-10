#!/bin/sh
# SUN T1: demonstrate that the existing worker smoke accepts damaged math,
# while the independent numeric oracle rejects the same compiled implementation.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
source_file="$root/android/app/src/main/cpp/holomorphic_walk.c"
header_dir="$root/android/app/src/main/cpp"
smoke_test="$root/tests/test_holomorphic_walk.c"
semantic_test="$root/tests/test_holomorphic_direction_semantics.c"
revision=$(git -C "$root" rev-parse HEAD)
seed=51a7c3d9
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT HUP INT TERM
receipt_dir=${HOLOMORPHIC_SEMANTIC_RECEIPT_DIR:-"$scratch"}
mkdir -p "$receipt_dir"
receipt="$receipt_dir/holomorphic-semantic-mutants.tsv"
printf 'case_id\tsource_revision\tmutant_source_sha256\told_smoke\tindependent_semantics\tseed\tevidence_layer\n' > "$receipt"

build_and_test() {
    case_id=$1
    candidate=$2
    expected=$3
    source_sha=$(sha256sum "$candidate" | cut -d ' ' -f 1)
    cc -std=c11 -Wall -Wextra -Werror -O2 -I "$header_dir" \
        "$smoke_test" "$candidate" -pthread -lm -o "$scratch/$case_id-smoke"
    "$scratch/$case_id-smoke" > "$scratch/$case_id-smoke.log" 2>&1 || {
        echo "SMOKE-UNEXPECTED-FAIL case=$case_id" >&2
        cat "$scratch/$case_id-smoke.log" >&2
        exit 1
    }
    grep -Fq 'holomorphic workers ready:' "$scratch/$case_id-smoke.log" || {
        echo "SMOKE-OUTPUT-ABSENT case=$case_id" >&2
        exit 1
    }
    cc -std=c11 -Wall -Wextra -Werror -O2 -I "$header_dir" \
        -include "$candidate" "$semantic_test" -pthread -lm \
        -o "$scratch/$case_id-semantic"
    if "$scratch/$case_id-semantic" > "$scratch/$case_id-semantic.log" 2>&1; then
        semantic=PASS
        grep -Fq 'SEMANTIC-PASS cases=134 seed=51a7c3d9' "$scratch/$case_id-semantic.log" || {
            echo "SEMANTIC-MISSING-EXECUTION-MARKER case=$case_id" >&2
            exit 1
        }
    else
        semantic=KILLED
        # An expected mutant is killed only by a numerical disagreement.
        # A compile failure, crash, missing executable or bad reference is not a kill.
        grep -Fq 'SEMANTIC-MISMATCH case=' "$scratch/$case_id-semantic.log" || {
            echo "MUTANT-NOT-SEMANTICALLY-KILLED case=$case_id" >&2
            cat "$scratch/$case_id-semantic.log" >&2
            exit 1
        }
    fi
    if [ "$semantic" != "$expected" ]; then
        echo "MUTATION-EXPECTATION-MISMATCH case=$case_id expected=$expected observed=$semantic" >&2
        cat "$scratch/$case_id-semantic.log" >&2
        exit 1
    fi
    printf '%s\t%s\t%s\tPASS\t%s\t%s\thost-CPU-direction-at-not-GPU\n' \
        "$case_id" "$revision" "$source_sha" "$semantic" "$seed" >> "$receipt"
    printf '%s smoke=PASS semantics=%s\n' "$case_id" "$semantic"
}

build_and_test good "$source_file" PASS

make_mutant() {
    case_id=$1
    find_text=$2
    replace_text=$3
    expected_hits=$4
    mutant="$scratch/$case_id.c"
    actual_hits=$(grep -Ec "$find_text" "$source_file" || true)
    if [ "$actual_hits" -ne "$expected_hits" ]; then
        echo "MUTATION-SOURCE-SHAPE-DRIFT case=$case_id expected=$expected_hits observed=$actual_hits" >&2
        exit 1
    fi
    sed "s/$find_text/$replace_text/g" "$source_file" > "$mutant"
    if cmp -s "$mutant" "$source_file"; then
        echo "MUTATION-NO-CHANGE case=$case_id" >&2
        exit 1
    fi
    build_and_test "$case_id" "$mutant" KILLED
}

# Replace exact source fragments, not broad regular-expression guesses.
make_mutant scale-half 'delta_q\[0\] = 0.0f;' \
    'u_x *= 0.5f; u_y *= 0.5f; delta_q[0] = 0.0f;' 1
make_mutant derivative-degree 'int degree = index + 1;' \
    'int degree = index;' 1
make_mutant omit-fifth 'int degree = index + 1;' \
    'int degree = index + 1; if (index == 4) continue;' 1
make_mutant wrong-real-sign 'delta_q\[0\] += term\[0\];' \
    'delta_q[0] -= term[0];' 1
make_mutant reversed-coefficients \
    'direction\[index\]\[0\], direction\[index\]\[1\],' \
    'direction[4 - index][0], direction[4 - index][1],' 2

printf 'MUTATION-RESULTS valid_control=PASS killed=5 surviving=0 seed=%s\n' "$seed"
cat "$receipt"
