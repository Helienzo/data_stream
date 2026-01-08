/**
 * @file:       data_stream.h
 * @author:     Lucas Wennerholm <lucas.wennerholm@gmail.com>
 * @brief:      Header file of the data stream buffer manager
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

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DATA_STREAM_H
#define DATA_STREAM_H

#include <stdint.h>
#include "c_buffer.h"

#ifndef DATA_STREAM_BUFFER_SIZE
#define DATA_STREAM_BUFFER_SIZE 50
#endif /* DATA_STREAM_BUFFER_SIZE  */

#ifndef DATA_STREAM_NUM_STREAM_BUFFERS
#define DATA_STREAM_NUM_STREAM_BUFFERS 3
#endif /* DATA_STREAM_NUM_STREAM_BUFFERS */

typedef enum {
    DATA_STREAM_DATA_AVAILABLE = 1,
    DATA_STREAM_SUCCESS        = 0,
    DATA_STREAM_NULL_ERROR     = -60001,
    DATA_STREAM_INVALID_ERROR  = -60002,
    DATA_STREAM_BUFFER_ERROR   = -60003,
    DATA_STREAM_NO_BUF_ERROR   = -60004,
    DATA_STREAM_LOCK_ERROR     = -60005,
    DATA_STREAM_EARLY_RETURN   = -60006,
    DATA_STREAM_DOUBLE_NOTIFY  = -60007,
} dataStreamErr_t;

typedef struct {
    volatile uint8_t buffer_out_state;   // Bitmask for what buffers out to either the producer or consumer
    volatile uint8_t buffer_ready_state; // Bitmask for what buffer ready for the consumer

    // FIFO queue for ready buffer order
    uint8_t ready_queue[DATA_STREAM_NUM_STREAM_BUFFERS];
    volatile uint8_t ready_queue_head;   // Read position
    volatile uint8_t ready_queue_tail;   // Write position

    // Lock data
    uint32_t lock_state;
    uint32_t lock_id;

    // Output Stream buffers
    struct {
        uint8_t          buf_array[DATA_STREAM_BUFFER_SIZE + C_BUFFER_ARRAY_OVERHEAD];
        cBuffer_t        buffer;
    } buffers[DATA_STREAM_NUM_STREAM_BUFFERS];
} dataStream_t;

/**
 * Initialize a data stream instance
 * Input: dataStream instance
 * Returns: dataStreamErr_t
 */
int32_t dataStreamInit(dataStream_t *inst);

/**
 * De-Init the data stream
 * Input: dataStream instance
 * Returns: dataStreamErr_t
 */
int32_t dataStreamDeInit(dataStream_t *inst);

/*
 * Notify that a stream buffer is ready to send, IRQ safe
 * Input: datastream instance
 * Input: uint8_t buffer ID
 * Returns dataStreamErr_t
*/
int32_t dataStreamNotifyBufferReady(dataStream_t *inst, uint8_t buffer_id);

/**
 * Get a free buffer available for filling
 * Input: datastream instance
 * Input: Buffer to populate
 * Input: Buffer ID
 * Returns dataStreamErr_t
 */
int32_t dataStreamGetNewBuffer(dataStream_t *inst, cBuffer_t **buf, uint8_t *buffer_id);

/**
 * Get the next populated buffer ready for processing, this clears the ready flag
 * Input: datastream instance
 * Input: Buffer to populate
 * Input: Buffer ID
 * Returns dataStreamErr_t
 */
int32_t dataStreamGetNextReadyBuffer(dataStream_t *inst, cBuffer_t **buf, uint8_t *buffer_id);

/**
 * Check if any buffer contains data ready for read
 * Input: datastream instance
 * Returns dataStreamErr_t
 */
int32_t dataStreamAnyBufferReady(dataStream_t *inst);

/**
 * Get the number of buffers that are ready
 * Input: datastream instance
 * Returns: dataStreamErr_t or number of buffers if any
 */
int32_t dataStreamNumBuffersReady(dataStream_t *inst);

/**
 * Return a buffer to the available pool
 * Input: datastream instance
 * Input: Buffer ID
 * Returns dataStreamErr_t
 */
int32_t dataStreamReturnBuffer(dataStream_t *inst, uint8_t buffer_id);

#endif /* DATA_STREAM_H */

#ifdef __cplusplus
}
#endif
