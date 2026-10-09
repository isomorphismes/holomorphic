# Direct ICK compilation of the existing maintained worker regression.
ICK ?= ick
BUILD ?= _/build/icky-host
.PHONY: test
test: $(BUILD)/test-holomorphic-walk
	$(BUILD)/test-holomorphic-walk

$(BUILD)/test-holomorphic-walk: tests/test_holomorphic_walk.c android/app/src/main/cpp/holomorphic_walk.c android/app/src/main/cpp/holomorphic_walk.h
	mkdir -p "$(BUILD)"
	$(ICK) -fno-link-libatomic -std=c11 -Wall -Wextra -Werror -Iandroid/app/src/main/cpp tests/test_holomorphic_walk.c android/app/src/main/cpp/holomorphic_walk.c -pthread -lm -o "$@"
