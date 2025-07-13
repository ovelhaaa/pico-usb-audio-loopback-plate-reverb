#include "ringbuffer.h"

#include <string.h>

#define RINGBUF_CAPACITY (RINGBUF_FRAMES * AUDIO_FRAME_BYTES / sizeof(int32_t))

static inline size_t rb_wrap(size_t idx) { return (idx % RINGBUF_CAPACITY); }

size_t ringbuffer_size(ringbuffer_t *rb) {
    size_t r = rb->read;
    size_t w = rb->write;
    return (w >= r) ? (w - r) : (RINGBUF_CAPACITY - (r - w));
}

size_t ringbuffer_capacity(ringbuffer_t *rb) {
    return (RINGBUF_CAPACITY - 1) - ringbuffer_size(rb);
}

bool ringbuffer_push(ringbuffer_t *rb, int32_t *src, size_t n) {
    if (n == 0 || n > ringbuffer_capacity(rb))
        return false;

    size_t w = rb->write;
    size_t first = (RINGBUF_CAPACITY - w) < n ? (RINGBUF_CAPACITY - w) : n;
    memcpy(&rb->buffer[w], src, first * sizeof(int32_t));

    size_t remain = n - first;
    if (remain)
        memcpy(&rb->buffer[0], src + first, remain * sizeof(int32_t));

    rb->write = rb_wrap(w + n);
    return true;
}

bool ringbuffer_pop(ringbuffer_t *rb, int32_t *dst, size_t n) {
    if (n == 0 || n > ringbuffer_size(rb))
        return false;

    size_t r = rb->read;
    size_t first = (RINGBUF_CAPACITY - r) < n ? (RINGBUF_CAPACITY - r) : n;
    memcpy(dst, &rb->buffer[r], first * sizeof(int32_t));

    size_t remain = n - first;
    if (remain)
        memcpy(dst + first, &rb->buffer[0], remain * sizeof(int32_t));

    rb->read = rb_wrap(r + n);
    return true;
}
