/************************************************************************************
* @file     : ringbuffer_port.h
* @brief    : Optional RingBuffer platform hook definitions.
* @details  : Projects may override these macros before including ringbuffer.c.
* @author   : GitHub Copilot
* @date     : 2026-03-30
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef RINGBUFFER_PORT_H
#define RINGBUFFER_PORT_H

#ifndef RINGBUFFER_PORT_ENTER_CRITICAL
#define RINGBUFFER_PORT_ENTER_CRITICAL() do { } while (0)
#endif

#ifndef RINGBUFFER_PORT_EXIT_CRITICAL
#define RINGBUFFER_PORT_EXIT_CRITICAL() do { } while (0)
#endif

#ifndef RINGBUFFER_PORT_MEMORY_BARRIER
#define RINGBUFFER_PORT_MEMORY_BARRIER() do { } while (0)
#endif

#endif  // RINGBUFFER_PORT_H
/**************************End of file********************************/
