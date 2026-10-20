#ifndef HOLOMORPHIC_GPU_TIMER_H
#define HOLOMORPHIC_GPU_TIMER_H

/* Optional physical GPU time measurement for GLES3.
 * This records EXT_disjoint_timer_query time, never an EGL/CPU surrogate.
 * Results are polled asynchronously; four slots, one query per 8 frames. */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifndef GL_TIME_ELAPSED_EXT
#define GL_TIME_ELAPSED_EXT 0x88BF
#endif
#ifndef GL_GPU_DISJOINT_EXT
#define GL_GPU_DISJOINT_EXT 0x8FBB
#endif
#define HOLO_GPU_TIMER_SLOTS 4

struct holo_gpu_timer {
    bool enabled;
    const char *reason;
    GLuint query_ids[HOLO_GPU_TIMER_SLOTS];
    bool pending[HOLO_GPU_TIMER_SLOTS];
    unsigned next_slot;
    uint64_t attempts, samples, busy, rejected;
    double total_ms, max_ms;
    void (GL_APIENTRYP *begin)(GLenum, GLuint);
    void (GL_APIENTRYP *end)(GLenum);
    void (GL_APIENTRYP *get64)(GLuint, GLenum, GLuint64 *);
};

static inline void holo_gpu_timer_init(struct holo_gpu_timer *timer, bool generated) {
    memset(timer, 0, sizeof(*timer));
    timer->reason = generated ? "missing-disjoint-extension" : "legacy-reference";
    if (!generated) return;
    GLint extension_count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &extension_count);
    bool available = false;
    for (GLint i = 0; i < extension_count; ++i) {
        const char *name = (const char *)glGetStringi(GL_EXTENSIONS, (GLuint)i);
        if (name != NULL && strcmp(name, "GL_EXT_disjoint_timer_query") == 0) {
            available = true;
            break;
        }
    }
    if (!available) return;
    timer->reason = "entrypoint-missing";
    timer->begin = (void (GL_APIENTRYP *)(GLenum, GLuint))
        eglGetProcAddress("glBeginQueryEXT");
    timer->end = (void (GL_APIENTRYP *)(GLenum))
        eglGetProcAddress("glEndQueryEXT");
    timer->get64 = (void (GL_APIENTRYP *)(GLuint, GLenum, GLuint64 *))
        eglGetProcAddress("glGetQueryObjectui64vEXT");
    if (timer->begin == NULL || timer->end == NULL || timer->get64 == NULL) return;
    glGenQueries(HOLO_GPU_TIMER_SLOTS, timer->query_ids);
    timer->reason = "query-allocation-error";
    if (glGetError() != GL_NO_ERROR || timer->query_ids[0] == 0) {
        glDeleteQueries(HOLO_GPU_TIMER_SLOTS, timer->query_ids);
        memset(timer->query_ids, 0, sizeof(timer->query_ids));
        return;
    }
    timer->enabled = true;
    timer->reason = "available";
}

static inline void holo_gpu_timer_shutdown(struct holo_gpu_timer *timer) {
    if (timer->query_ids[0] != 0)
        glDeleteQueries(HOLO_GPU_TIMER_SLOTS, timer->query_ids);
    memset(timer, 0, sizeof(*timer));
}

static inline int holo_gpu_timer_begin(struct holo_gpu_timer *timer, uint64_t frame) {
    if (!timer->enabled || (frame % 8u) != 0u) return -1;
    unsigned slot = timer->next_slot;
    if (timer->pending[slot]) {
        ++timer->busy;
        return -1;
    }
    GLint disjoint = 0;
    glGetIntegerv(GL_GPU_DISJOINT_EXT, &disjoint);
    if (disjoint != 0) {
        ++timer->rejected;
        timer->reason = "gpu-disjoint";
        timer->enabled = false;
        return -1;
    }
    timer->begin(GL_TIME_ELAPSED_EXT, timer->query_ids[slot]);
    if (glGetError() != GL_NO_ERROR) {
        ++timer->rejected;
        timer->reason = "begin-query-error";
        timer->enabled = false;
        return -1;
    }
    ++timer->attempts;
    return (int)slot;
}

static inline void holo_gpu_timer_end(struct holo_gpu_timer *timer, int slot) {
    if (slot < 0) return;
    timer->end(GL_TIME_ELAPSED_EXT);
    if (glGetError() != GL_NO_ERROR) {
        ++timer->rejected;
        timer->reason = "end-query-error";
        timer->enabled = false;
        return;
    }
    timer->pending[(unsigned)slot] = true;
    timer->next_slot = ((unsigned)slot + 1u) % HOLO_GPU_TIMER_SLOTS;
}

static inline void holo_gpu_timer_poll(struct holo_gpu_timer *timer) {
    if (!timer->enabled) return;
    GLint disjoint = 0;
    glGetIntegerv(GL_GPU_DISJOINT_EXT, &disjoint);
    if (disjoint != 0) {
        timer->reason = "gpu-disjoint";
        timer->enabled = false;
        ++timer->rejected;
        return;
    }
    for (unsigned i = 0; i < HOLO_GPU_TIMER_SLOTS; ++i) {
        if (!timer->pending[i]) continue;
        GLuint ready = GL_FALSE;
        glGetQueryObjectuiv(timer->query_ids[i], GL_QUERY_RESULT_AVAILABLE, &ready);
        if (ready != GL_TRUE) continue;
        GLuint64 nanos = 0;
        timer->get64(timer->query_ids[i], GL_QUERY_RESULT, &nanos);
        GLint after = 0;
        glGetIntegerv(GL_GPU_DISJOINT_EXT, &after);
        if (glGetError() != GL_NO_ERROR || after != 0) {
            ++timer->rejected;
            timer->reason = "query-result-invalid";
            timer->enabled = false;
            return;
        }
        double elapsed = (double)nanos * 1.0e-6;
        timer->total_ms += elapsed;
        if (elapsed > timer->max_ms) timer->max_ms = elapsed;
        ++timer->samples;
        timer->pending[i] = false;
    }
}

static inline void holo_gpu_timer_reset_window(struct holo_gpu_timer *timer) {
    timer->attempts = timer->samples = timer->busy = timer->rejected = 0;
    timer->total_ms = timer->max_ms = 0.0;
}

#endif
