/*
 * Copyright (c) 2025 Space Cubics Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sc_qspi_nor.h"
#include "version.h"

#define SCOBCA1_FPGA_SYSREG_CODEMSEL (0x4F000000)
#define SCOBCA1_FPGA_HRMEM_ADDR      (0x60000000)
#define SYSREG_CODEMSEL_ENABLE_HRMEM (0x5a5a0000)

int main(void)
{
	int ret;

	printk("SC-OBC Module A1 Bootloader Ver %s\n", SCOBC_A1_BOOT_LOADER_VERSION);

	ret = sc_qspi_nor_copy_to_hrmem(SC_CFG_MEM0, KB(CONFIG_SCOBC_A1_BOOT_CFG_COPY_SIZE_KB),
					SCOBCA1_FPGA_HRMEM_ADDR);
	if (ret < 0) {
		printk("Failed to copy the Zephyr application from Config memory to HRMEM.\n");
		goto end;
	}

	sys_write32(SYSREG_CODEMSEL_ENABLE_HRMEM, SCOBCA1_FPGA_SYSREG_CODEMSEL);

end:
	return ret;
}
