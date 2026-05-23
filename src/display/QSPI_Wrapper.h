/**
 * @file QSPI_Wrapper.h
 * @brief Wrapper to provide v1.3.7 async QSPI API compatibility with v1.6.5.
 *
 * In v1.6.5, the async queue-based API (waitAllChunks, queueChunk, etc.)
 * was removed in favor of simpler blocking transfers. This wrapper provides
 * backward compatibility for code written against the v1.3.7 API.
 */
#pragma once

#include <Arduino.h>
#include <driver/spi_master.h>

class QSPI_Wrapper
{
public:
    enum : uint32_t
    {
        DEFAULT_TIMEOUT_MS = 10000
    };

    static bool waitAllChunks(void *bus, uint32_t timeout_ms = DEFAULT_TIMEOUT_MS)
    {
        (void)bus;
        (void)timeout_ms;
        return true;
    }

    static bool waitForTransfer(void *bus, uint32_t timeout_ms = DEFAULT_TIMEOUT_MS)
    {
        (void)bus;
        (void)timeout_ms;
        return true;
    }

    static bool isTransferComplete(void *bus)
    {
        (void)bus;
        return true;
    }

    static bool beginAsyncWrite(void *bus)
    {
        (void)bus;
        return true;
    }

    static bool endAsyncWrite(void *bus)
    {
        (void)bus;
        return true;
    }
};