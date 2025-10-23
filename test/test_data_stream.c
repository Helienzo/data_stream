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

    // Test 9: FIFO ordering - notify in order 2, 0, 1 and verify retrieval order
    dataStreamDeInit(&stream);
    dataStreamInit(&stream);

    uint8_t buf_ids[3];
    cBuffer_t *bufs[3];

    // Get all 3 buffers
    for (int i = 0; i < 3; i++) {
        res = dataStreamGetNewBuffer(&stream, &bufs[i], &buf_ids[i]);
        TEST_ASSERT(res == DATA_STREAM_SUCCESS);
    }

    // Notify in specific order: 2, 0, 1
    res = dataStreamNotifyBufferReady(&stream, buf_ids[2]);
    TEST_ASSERT(res == DATA_STREAM_SUCCESS);
    res = dataStreamNotifyBufferReady(&stream, buf_ids[0]);
    TEST_ASSERT(res == DATA_STREAM_SUCCESS);
    res = dataStreamNotifyBufferReady(&stream, buf_ids[1]);
    TEST_ASSERT(res == DATA_STREAM_SUCCESS);

    // Retrieve and verify order: should be 2, 0, 1
    uint8_t retrieved_id;
    res = dataStreamGetNextReadyBuffer(&stream, &buf, &retrieved_id);
    TEST_ASSERT(res == DATA_STREAM_DATA_AVAILABLE);
    TEST_ASSERT(retrieved_id == buf_ids[2]);

    res = dataStreamGetNextReadyBuffer(&stream, &buf, &retrieved_id);
    TEST_ASSERT(res == DATA_STREAM_DATA_AVAILABLE);
    TEST_ASSERT(retrieved_id == buf_ids[0]);

    res = dataStreamGetNextReadyBuffer(&stream, &buf, &retrieved_id);
    TEST_ASSERT(res == DATA_STREAM_DATA_AVAILABLE);
    TEST_ASSERT(retrieved_id == buf_ids[1]);

    // Test 10: Early return prevention
    dataStreamDeInit(&stream);
    dataStreamInit(&stream);

    res = dataStreamGetNewBuffer(&stream, &buf, &buf_id);
    TEST_ASSERT(res == DATA_STREAM_SUCCESS);
    res = dataStreamNotifyBufferReady(&stream, buf_id);
    TEST_ASSERT(res == DATA_STREAM_SUCCESS);

    // Try to return while still ready - should fail
    res = dataStreamReturnBuffer(&stream, buf_id);
    TEST_ASSERT(res == DATA_STREAM_EARLY_RETURN);

    // Test 11: Invalid buffer ID on notify
    res = dataStreamNotifyBufferReady(&stream, 99);
    TEST_ASSERT(res == DATA_STREAM_BUFFER_ERROR);

    // Test 12: Invalid buffer ID on return
    res = dataStreamReturnBuffer(&stream, 99);
    TEST_ASSERT(res == DATA_STREAM_BUFFER_ERROR);

    // Test 13: Double notify prevention
    dataStreamDeInit(&stream);
    dataStreamInit(&stream);

    res = dataStreamGetNewBuffer(&stream, &buf, &buf_id);
    TEST_ASSERT(res == DATA_STREAM_SUCCESS);
    res = dataStreamNotifyBufferReady(&stream, buf_id);
    TEST_ASSERT(res == DATA_STREAM_SUCCESS);

    // Try to notify same buffer again - should fail
    res = dataStreamNotifyBufferReady(&stream, buf_id);
    TEST_ASSERT(res == DATA_STREAM_DOUBLE_NOTIFY);

    // Test 14: Stress test with complex patterns over 512 cycles
    dataStreamDeInit(&stream);
    dataStreamInit(&stream);

    for (int i = 0; i < 512; i++) {
        // Pattern 1: Get 2 buffers, notify both, retrieve in FIFO order
        cBuffer_t *buf1, *buf2;
        uint8_t id1, id2;

        res = dataStreamGetNewBuffer(&stream, &buf1, &id1);
        TEST_ASSERT(res == DATA_STREAM_SUCCESS);
        res = dataStreamGetNewBuffer(&stream, &buf2, &id2);
        TEST_ASSERT(res == DATA_STREAM_SUCCESS);

        res = dataStreamNotifyBufferReady(&stream, id1);
        TEST_ASSERT(res == DATA_STREAM_SUCCESS);
        res = dataStreamNotifyBufferReady(&stream, id2);
        TEST_ASSERT(res == DATA_STREAM_SUCCESS);

        // Retrieve and verify FIFO order
        res = dataStreamGetNextReadyBuffer(&stream, &buf, &buf_id);
        TEST_ASSERT(res == DATA_STREAM_DATA_AVAILABLE);
        TEST_ASSERT(buf_id == id1);

        res = dataStreamReturnBuffer(&stream, buf_id);
        TEST_ASSERT(res == DATA_STREAM_SUCCESS);

        res = dataStreamGetNextReadyBuffer(&stream, &buf, &buf_id);
        TEST_ASSERT(res == DATA_STREAM_DATA_AVAILABLE);
        TEST_ASSERT(buf_id == id2);

        res = dataStreamReturnBuffer(&stream, buf_id);
        TEST_ASSERT(res == DATA_STREAM_SUCCESS);

        // Pattern 2: Get 3, notify 2, retrieve 1, return 1, notify last, retrieve 2
        cBuffer_t *buf3;
        uint8_t id3;

        res = dataStreamGetNewBuffer(&stream, &buf1, &id1);
        TEST_ASSERT(res == DATA_STREAM_SUCCESS);
        res = dataStreamGetNewBuffer(&stream, &buf2, &id2);
        TEST_ASSERT(res == DATA_STREAM_SUCCESS);
        res = dataStreamGetNewBuffer(&stream, &buf3, &id3);
        TEST_ASSERT(res == DATA_STREAM_SUCCESS);

        res = dataStreamNotifyBufferReady(&stream, id3);
        TEST_ASSERT(res == DATA_STREAM_SUCCESS);
        res = dataStreamNotifyBufferReady(&stream, id1);
        TEST_ASSERT(res == DATA_STREAM_SUCCESS);

        res = dataStreamGetNextReadyBuffer(&stream, &buf, &buf_id);
        TEST_ASSERT(res == DATA_STREAM_DATA_AVAILABLE);
        TEST_ASSERT(buf_id == id3);

        res = dataStreamReturnBuffer(&stream, buf_id);
        TEST_ASSERT(res == DATA_STREAM_SUCCESS);

        res = dataStreamNotifyBufferReady(&stream, id2);
        TEST_ASSERT(res == DATA_STREAM_SUCCESS);

        res = dataStreamGetNextReadyBuffer(&stream, &buf, &buf_id);
        TEST_ASSERT(res == DATA_STREAM_DATA_AVAILABLE);
        TEST_ASSERT(buf_id == id1);

        res = dataStreamGetNextReadyBuffer(&stream, &buf, &buf_id);
        TEST_ASSERT(res == DATA_STREAM_DATA_AVAILABLE);
        TEST_ASSERT(buf_id == id2);

        res = dataStreamReturnBuffer(&stream, id1);
        TEST_ASSERT(res == DATA_STREAM_SUCCESS);
        res = dataStreamReturnBuffer(&stream, id2);
        TEST_ASSERT(res == DATA_STREAM_SUCCESS);
    }

    printf("All dataStream tests passed!\n");
    return 0;
}
