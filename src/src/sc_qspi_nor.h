/*
 * Copyright (c) 2025 Space Cubics Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <zephyr/kernel.h>

#define SC_CFG_MEM0  (0u)
#define SC_CFG_MEM1  (1u)
#define SC_DATA_MEM0 (0u)
#define SC_DATA_MEM1 (1u)

int sc_qspi_nor_copy_to_hrmem(uint8_t mem_no, uint32_t size, mm_reg_t hrmem_mem_addr);
