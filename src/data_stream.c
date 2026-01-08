/**
 * @file:       data_stream.c
 * @author:     Lucas Wennerholm <lucas.wennerholm@gmail.com>
 * @brief:      Implementation of the data stream buffer manager
 *
 * @license: MIT License
 *
 * Copyright (c) 2025 Lucas Wennerholm
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/

#include "data_stream.h"
#include "stdio.h"
#include <stdarg.h>

// Weakly defined logging function - can be overridden by user
__attribute__((weak)) void util_log(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

#ifndef LOG
#define LOG(f_, ...) util_log((f_), ##__VA_ARGS__)
#endif

#ifndef LOG_DEBUG
#define LOG_DEBUG(f_, ...)
#endif

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

__attribute__((weak)) int32_t dataStreamLockAcquire(dataStream_t *inst) {
    UNUSED(inst);
    return DATA_STREAM_SUCCESS;
}
__attribute__((weak)) int32_t dataStreamLockRelease(dataStream_t *inst) {
    UNUSED(inst);
    return DATA_STREAM_SUCCESS;
}
__attribute__((weak)) int32_t dataStreamLockInit(dataStream_t *inst) {
    UNUSED(inst);
    return DATA_STREAM_SUCCESS;
}
__attribute__((weak)) int32_t dataStreamLockDeInit(dataStream_t *inst) {
    UNUSED(inst);
    return DATA_STREAM_SUCCESS;
}

int32_t dataStreamDeInit(dataStream_t *inst) {
    return dataStreamLockDeInit(inst);
}

int32_t dataStreamInit(dataStream_t *inst) {

    inst->buffer_out_state            = (1 << DATA_STREAM_NUM_STREAM_BUFFERS) - 1;
    inst->buffer_ready_state          = 0x00;
    inst->ready_queue_head            = 0;
    inst->ready_queue_tail            = 0;

    int32_t res = DATA_STREAM_SUCCESS;

    // Init the lock
    if ((res = dataStreamLockInit(inst)) != DATA_STREAM_SUCCESS) {
        return res;
    }

    // Init all Stream buffers
    for (uint32_t i = 0; i < DATA_STREAM_NUM_STREAM_BUFFERS; i++) {
        // Create a radio message buffer
        if ((res = cBufferInit(&inst->buffers[i].buffer, inst->buffers[i].buf_array, DATA_STREAM_BUFFER_SIZE + C_BUFFER_ARRAY_OVERHEAD)) != C_BUFFER_SUCCESS) {
            LOG("DATA STREAM INIT FAILED! %i\n", res);
            return res;
        }
    }

    return DATA_STREAM_SUCCESS;
}

int32_t dataStreamNotifyBufferReady(dataStream_t *inst, uint8_t buffer_id) {
    if (inst == NULL) {
        return DATA_STREAM_NULL_ERROR;
    }

    if (buffer_id >= DATA_STREAM_NUM_STREAM_BUFFERS) {
        return DATA_STREAM_BUFFER_ERROR;
    }

    uint8_t buffer_mask = (1 << buffer_id);

    dataStreamLockAcquire(inst);

    // Prevent double notify
    if (inst->buffer_ready_state & buffer_mask) {
        dataStreamLockRelease(inst);
        LOG("Double notify %u\n", buffer_id);
        return DATA_STREAM_DOUBLE_NOTIFY;
    }

    if (~inst->buffer_out_state & buffer_mask) {
        inst->buffer_ready_state |= 1 << buffer_id;
        inst->ready_queue[inst->ready_queue_tail] = buffer_id;
        inst->ready_queue_tail = (inst->ready_queue_tail + 1) % DATA_STREAM_NUM_STREAM_BUFFERS;
        dataStreamLockRelease(inst);
    } else {
        dataStreamLockRelease(inst);
        LOG("Invalid Notification: %#x %#x %u\n", inst->buffer_ready_state, inst->buffer_out_state, buffer_id);
    }

    return DATA_STREAM_SUCCESS;
}

int32_t dataStreamGetNewBuffer(dataStream_t *inst, cBuffer_t **buf, uint8_t *buffer_id) {
    if (inst == NULL || buf == NULL || buffer_id == NULL) {
        return DATA_STREAM_NULL_ERROR;
    }

    dataStreamLockAcquire(inst);
    uint32_t available = inst->buffer_out_state;

    // Check if there is any buffer available
    if ((available) == 0) {
        dataStreamLockRelease(inst);
        LOG_DEBUG("NO BUFFER %#x %#x\n", inst->buffer_out_state);
        *buf       = NULL;
        *buffer_id = 0xFF;
        return DATA_STREAM_NO_BUF_ERROR;
    }

    // builtin_ctz returns index of least-significant 1 bit
    uint32_t idx = __builtin_ctz(available);

    if (idx >= DATA_STREAM_NUM_STREAM_BUFFERS) {
        dataStreamLockRelease(inst);
        LOG("Invalid buffer index!\n");
        return DATA_STREAM_BUFFER_ERROR;
    }

    // mark it in-use
    inst->buffer_out_state &= ~(1u << idx);
    dataStreamLockRelease(inst);

    // Populate parameters
    *buf       = &inst->buffers[idx].buffer;
    *buffer_id = (uint8_t)idx;
    cBufferClear(*buf);

    return DATA_STREAM_SUCCESS;
}

int32_t dataStreamGetNextReadyBuffer(dataStream_t *inst, cBuffer_t **buf, uint8_t *buffer_id) {
    if (inst == NULL) {
        return DATA_STREAM_NULL_ERROR;
    }

    dataStreamLockAcquire(inst);

    // Check if queue empty using bitmask
    if (inst->buffer_ready_state == 0) {
        dataStreamLockRelease(inst);
        LOG_DEBUG("NO BUFFER %#x %#x\n", inst->buffer_out_state);
        *buf       = NULL;
        *buffer_id = 0xFF;
        return DATA_STREAM_NO_BUF_ERROR;
    }

    // Dequeue next ready buffer
    uint8_t idx = inst->ready_queue[inst->ready_queue_head];
    inst->ready_queue_head = (inst->ready_queue_head + 1) % DATA_STREAM_NUM_STREAM_BUFFERS;
    inst->buffer_ready_state &= ~(1 << idx);
    dataStreamLockRelease(inst);

    *buf       = &inst->buffers[idx].buffer;
    *buffer_id = idx;
    return DATA_STREAM_DATA_AVAILABLE;
}

int32_t dataStreamNumBuffersReady(dataStream_t *inst) {
    if (inst == NULL) {
        return DATA_STREAM_NULL_ERROR;
    }

    return __builtin_popcount(inst->buffer_ready_state);
}

int32_t dataStreamAnyBufferReady(dataStream_t *inst) {
    if (inst == NULL) {
        return DATA_STREAM_NULL_ERROR;
    }

    if (inst->buffer_ready_state) {
        return DATA_STREAM_DATA_AVAILABLE;
    }

    return DATA_STREAM_SUCCESS;
}

int32_t dataStreamReturnBuffer(dataStream_t *inst, uint8_t buffer_id) {
    if (inst == NULL) {
        return DATA_STREAM_NULL_ERROR;
    }

    if (buffer_id >= DATA_STREAM_NUM_STREAM_BUFFERS) {
        return DATA_STREAM_BUFFER_ERROR;
    }

    uint8_t buffer_mask = (1 << buffer_id);

    dataStreamLockAcquire(inst);

    // Prevent early return while buffer still ready
    if (inst->buffer_ready_state & buffer_mask) {
        dataStreamLockRelease(inst);
        LOG("Early return %u\n", buffer_id);
        return DATA_STREAM_EARLY_RETURN;
    }

    // Return a buffer only if it is out
    if (~inst->buffer_out_state & buffer_mask) {
        inst->buffer_out_state |= 1 << buffer_id;
        dataStreamLockRelease(inst);
    } else {
        dataStreamLockRelease(inst);
        LOG("Bad buffer return %u %u %u\n", inst->buffer_out_state, inst->buffer_ready_state, buffer_id);
        return DATA_STREAM_INVALID_ERROR;
    }

    return DATA_STREAM_SUCCESS;
}
