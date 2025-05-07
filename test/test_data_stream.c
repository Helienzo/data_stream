#include <stdio.h>
#include <assert.h>
#include "data_stream.h"
#include "c_buffer.h"

// Simple macro for test reporting
#define TEST_ASSERT(x) do { if (!(x)) { printf("Test failed: %s, line %d\n", #x, __LINE__); return -1; } } while(0)

int main(void) {
    dataStream_t stream;
    cBuffer_t *buf;
    uint8_t buf_id;
    int32_t res;

    printf("Starting dataStream tests...\n");

    // Test 1: Initialization
    res = dataStreamInit(&stream);
    TEST_ASSERT(res == DATA_STREAM_SUCCESS);
    TEST_ASSERT(stream.buffer_out_state == (1 << DATA_STREAM_NUM_STREAM_BUFFERS) - 1);
    TEST_ASSERT(stream.buffer_ready_state == 0x00);

    // Test 2: Get new buffer
    res = dataStreamGetNewBuffer(&stream, &buf, &buf_id);
    TEST_ASSERT(res == DATA_STREAM_SUCCESS);
    TEST_ASSERT(buf != NULL);
    TEST_ASSERT(buf_id < DATA_STREAM_NUM_STREAM_BUFFERS);
    TEST_ASSERT((stream.buffer_out_state & (1 << buf_id)) == 0); // buffer should be marked "out"

    // Test 3: Notify buffer ready
    res = dataStreamNotifyBufferReady(&stream, buf_id);
    TEST_ASSERT(res == DATA_STREAM_SUCCESS);
    TEST_ASSERT((stream.buffer_ready_state & (1 << buf_id)) != 0); // buffer should be marked ready

    // Test 4: Check if any buffer ready
    res = dataStreamAnyBufferReady(&stream);
    TEST_ASSERT(res == DATA_STREAM_DATA_AVAILABLE);

    // Test 5: Get next ready buffer
    cBuffer_t *ready_buf;
    uint8_t ready_buf_id;
    res = dataStreamGetNextReadyBuffer(&stream, &ready_buf, &ready_buf_id);
    TEST_ASSERT(res == DATA_STREAM_DATA_AVAILABLE);
    TEST_ASSERT(ready_buf == buf);
    TEST_ASSERT(ready_buf_id == buf_id);
    TEST_ASSERT((stream.buffer_ready_state & (1 << buf_id)) == 0); // Should be cleared after getting

    // Test 6: Return buffer
    res = dataStreamReturnBuffer(&stream, buf_id);
    TEST_ASSERT(res == DATA_STREAM_SUCCESS);
    TEST_ASSERT((stream.buffer_out_state & (1 << buf_id)) != 0); // buffer should be available again

    // Test 7: Return same buffer again (should fail)
    res = dataStreamReturnBuffer(&stream, buf_id);
    TEST_ASSERT(res == DATA_STREAM_INVALID_ERROR);

    // Test 8: Get all buffers until none left
    for (int i = 0; i < DATA_STREAM_NUM_STREAM_BUFFERS; i++) {
        res = dataStreamGetNewBuffer(&stream, &buf, &buf_id);
        TEST_ASSERT(res == DATA_STREAM_SUCCESS);
    }

    // Now no buffers should be available
    res = dataStreamGetNewBuffer(&stream, &buf, &buf_id);
    TEST_ASSERT(res == DATA_STREAM_NO_BUF_ERROR);

    printf("All dataStream tests passed!\n");
    return 0;
}
