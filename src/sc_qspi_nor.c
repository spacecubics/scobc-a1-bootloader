/*
 * Copyright (c) 2025 Space Cubics Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/storage/flash_map.h>
#include "sc_qspi_nor.h"

/* Base address */
#define SCOBCA1_FPGA_CFG_BASE_ADDR    (0x40000000)
#define SCOBCA1_FPGA_DATA_BASE_ADDR   (0x40100000)
#define SCOBCA1_FPGA_SYSREG_BASE_ADDR (0x4F000000)

/* Offset */
#define SCQSPI_ACR_OFFSET       (0x0000) /* QSPI Access Control Register */
#define SCQSPI_TDR_OFFSET       (0x0004) /* QSPI TX Data Register */
#define SCQSPI_RDR_OFFSET       (0x0008) /* QSPI RX Data Register */
#define SCQSPI_ASR_OFFSET       (0x000C) /* QSPI Access Status Register */
#define SCQSPI_FIFOSR_OFFSET    (0x0010) /* QSPI FIFO Status Register */
#define SCQSPI_FIFORR_OFFSET    (0x0014) /* QSPI FIFO Reset Register */
#define SCQSPI_ISR_OFFSET       (0x0020) /* QSPI Interrupt Status Register */
#define SCQSPI_IER_OFFSET       (0x0024) /* QSPI Interrupt Enable Register */
#define SCQSPI_CCR_OFFSET       (0x0030) /* QSPI Clock Control Register */
#define SCQSPI_DCMSR_OFFSET     (0x0034) /* QSPI Data Capture Mode Setting Register */
#define SCQSPI_FTLSR_OFFSET     (0x0038) /* QSPI FIFO Threshold Level Setting Register */
#define SCQSPI_VER_OFFSET       (0xF000) /* QSPI Controller IP Version Register */
#define SYSREG_CFGMEMCTL_OFFSET (0x0010) /* Configuration Flash Memory Register */
#define SYSREG_BOOTSTS_OFFSET   (0x0014) /* Boot Status Register */

/* System Register Config */
#define SCOBCA1_FPGA_SYSREG_CFGMEMCTL (SCOBCA1_FPGA_SYSREG_BASE_ADDR + SYSREG_CFGMEMCTL_OFFSET)
#define SCOBCA1_FPGA_SYSREG_BOOTSTS   (SCOBCA1_FPGA_SYSREG_BASE_ADDR + SYSREG_BOOTSTS_OFFSET)

/* QSPI Control Register for Data/ Configutation Memory */
#define SCOBCA1_FPGA_NORFLASH_QSPI_ACR(base)    (base + SCQSPI_ACR_OFFSET)
#define SCOBCA1_FPGA_NORFLASH_QSPI_TDR(base)    (base + SCQSPI_TDR_OFFSET)
#define SCOBCA1_FPGA_NORFLASH_QSPI_RDR(base)    (base + SCQSPI_RDR_OFFSET)
#define SCOBCA1_FPGA_NORFLASH_QSPI_ASR(base)    (base + SCQSPI_ASR_OFFSET)
#define SCOBCA1_FPGA_NORFLASH_QSPI_FIFOSR(base) (base + SCQSPI_FIFOSR_OFFSET)
#define SCOBCA1_FPGA_NORFLASH_QSPI_FIFORR(base) (base + SCQSPI_FIFORR_OFFSET)
#define SCOBCA1_FPGA_NORFLASH_QSPI_ISR(base)    (base + SCQSPI_ISR_OFFSET)
#define SCOBCA1_FPGA_NORFLASH_QSPI_IER(base)    (base + SCQSPI_IER_OFFSET)
#define SCOBCA1_FPGA_NORFLASH_QSPI_CCR(base)    (base + SCQSPI_CCR_OFFSET)
#define SCOBCA1_FPGA_NORFLASH_QSPI_DCMSR(base)  (base + SCQSPI_DCMSR_OFFSET)
#define SCOBCA1_FPGA_NORFLASH_QSPI_FTLSR(base)  (base + SCQSPI_FTLSR_OFFSET)
#define SCOBCA1_FPGA_NORFLASH_QSPI_VER(base)    (base + SCQSPI_VER_OFFSET)

#define SCQSPI_NOR_FLASH_SECTOR_BYTE       KB(4)
#define SCQSPI_NOR_FLASH_BLOCK_BYTE        KB(64)
#define SCQSPI_NOR_FLASH_MEM_ADDR_SIZE     (3u)
#define SCQSPI_DATA_MEM0_SS                (0x01)
#define SCQSPI_DATA_MEM1_SS                (0x02)
#define SCQSPI_ASR_IDLE                    (0x00)
#define SCQSPI_ASR_BUSY                    (0x01)
#define SCQSPI_RX_FIFO_MAX_BYTE            (16U)
#define SCQSPI_NOR_FLASH_DUMMY_CYCLE_COUNT (4U)
#define SCQSPI_SPI_MODE_QUAD               (0x00020000)
#define SCQSPI_BOOTSTS_FALLBACK            BIT(1)
#define SCQSPI_ALL_MASK                    GENMASK(31, 0)
#define SYSREG_CFGMEMSEL_MON_MASK          GENMASK(5, 4) /* CFGMEMSEL and CFGMEMMON bits mask */

#define SCQSPI_REG_READ_RETRY(count) (count)

static inline void write32(uint32_t addr, uint32_t val)
{
	sys_write32(val, addr);
}

static bool verify(uint32_t addr, uint32_t exp, uint32_t read_mask, uint32_t retry)
{
	uint32_t regval;

	regval = sys_read32(addr) & read_mask;

	if (regval == exp) {
		return true;
	} else if (retry == 0) {
		printk("Err: read32  [0x%08X] 0x%08x (exp:0x%08x)", addr, regval, exp);
	}

	for (uint32_t i = 0; i < retry; i++) {
		regval = sys_read32(addr) & read_mask;
		if (regval == exp) {
			return true;
		} else if (i + 1 == retry) {
			printk("Err: read32  [0x%08X] 0x%08x (exp:0x%08x) (retry:%d)", addr, regval,
			       exp, i + 1);
		}
		k_usleep(1);
	}

	printk("!!! Verify failed: retry count: %d\n", retry);
	return false;
}

bool qspi_is_fallback(void)
{
	uint32_t val = sys_read32(SCOBCA1_FPGA_SYSREG_BOOTSTS);
	return (val & SCQSPI_BOOTSTS_FALLBACK) != 0;
}

static bool qspi_select_mem(uint32_t base, uint8_t mem_no, uint32_t *spi_ss)
{
	if (base == SCOBCA1_FPGA_CFG_BASE_ADDR) {
		/* Config Memory is switched by CFGMEMSEL */
		if (mem_no == SC_DATA_MEM0) {
			/* Select Config Memory 0 */
			write32(SCOBCA1_FPGA_SYSREG_CFGMEMCTL, 0x00);
			if (!verify(SCOBCA1_FPGA_SYSREG_CFGMEMCTL, 0x00, SYSREG_CFGMEMSEL_MON_MASK,
				    SCQSPI_REG_READ_RETRY(100000))) {
				printk("!!! Can not select Config Memory %d\n", mem_no);
				return false;
			}
		} else {
			/* Select Config Memory 1 */
			write32(SCOBCA1_FPGA_SYSREG_CFGMEMCTL, 0x10);
			if (!verify(SCOBCA1_FPGA_SYSREG_CFGMEMCTL, 0x30, SYSREG_CFGMEMSEL_MON_MASK,
				    SCQSPI_REG_READ_RETRY(100000))) {
				printk("!!! Can not select Config Memory %d\n", mem_no);
				return false;
			}
		}
		*spi_ss = 0x01;
	} else {
		if (mem_no == SC_DATA_MEM0) {
			*spi_ss = SCQSPI_DATA_MEM0_SS;
		} else {
			*spi_ss = SCQSPI_DATA_MEM1_SS;
		}
	}

	return true;
}

static bool is_qspi_idle(uint32_t base)
{
	/* Confirm QSPI Access Status is `Idle` */
	if (!verify(SCOBCA1_FPGA_NORFLASH_QSPI_ASR(base), SCQSPI_ASR_IDLE, SCQSPI_ALL_MASK,
		    SCQSPI_REG_READ_RETRY(10))) {
		printk("QSPI (Data Memory) is busy, so exit\n");
		return false;
	}

	return true;
}

static bool activate_spi_ss(uint32_t base, uint32_t spi_mode)
{
	write32(SCOBCA1_FPGA_NORFLASH_QSPI_ACR(base), spi_mode);
	if (!is_qspi_idle(base)) {
		return false;
	}

	return true;
}

static bool inactivate_spi_ss(uint32_t base)
{
	write32(SCOBCA1_FPGA_NORFLASH_QSPI_ACR(base), 0x00000000);
	if (!is_qspi_idle(base)) {
		return false;
	}

	return true;
}

static void write_mem_addr_to_flash(uint32_t base, mm_reg_t mem_addr)
{
	write32(SCOBCA1_FPGA_NORFLASH_QSPI_TDR(base), (mem_addr & 0xFF000000) >> 24);
	write32(SCOBCA1_FPGA_NORFLASH_QSPI_TDR(base), (mem_addr & 0x00FF0000) >> 16);
	write32(SCOBCA1_FPGA_NORFLASH_QSPI_TDR(base), (mem_addr & 0x0000FF00) >> 8);
	write32(SCOBCA1_FPGA_NORFLASH_QSPI_TDR(base), (mem_addr & 0x000000FF));
}

static bool send_dummy_cycle(uint32_t base, uint8_t dummy_count)
{
	for (uint8_t i = 0; i < dummy_count; i++) {
		write32(SCOBCA1_FPGA_NORFLASH_QSPI_RDR(base), 0x00);
	}

	if (!is_qspi_idle(base)) {
		return false;
	}

	/* Discard dummy data */
	for (uint8_t i = 0; i < dummy_count; i++) {
		sys_read32(SCOBCA1_FPGA_NORFLASH_QSPI_RDR(base));
	}

	return true;
}

static bool read_rx_data(uint32_t base, size_t read_size, uint8_t *read_vals)
{
	bool ret = true;

	/* Reqest RX FIFO */
	for (uint8_t i = 0; i < read_size; i++) {
		write32(SCOBCA1_FPGA_NORFLASH_QSPI_RDR(base), 0x00);
	}

	if (!is_qspi_idle(base)) {
		return false;
	}

	/* Read RX FIFO */
	for (uint8_t i = 0; i < read_size; i++) {
		read_vals[i] = sys_read8(SCOBCA1_FPGA_NORFLASH_QSPI_RDR(base));
	}

	return ret;
}

static bool is_qspi_control_done(uint32_t base)
{
	/* Confirm QSPI Interrupt Stauts is `SPI Control Done` */
	if (!verify(SCOBCA1_FPGA_NORFLASH_QSPI_ISR(base), 0x01, SCQSPI_ALL_MASK,
		    SCQSPI_REG_READ_RETRY(10))) {
		return false;
	}

	/* Clear QSPI Interrupt Stauts */
	write32(SCOBCA1_FPGA_NORFLASH_QSPI_ISR(base), 0x01);
	if (!verify(SCOBCA1_FPGA_NORFLASH_QSPI_ISR(base), 0x00, SCQSPI_ALL_MASK,
		    SCQSPI_REG_READ_RETRY(10))) {
		return false;
	}

	return true;
}

static bool clear_status_register(uint32_t base, uint32_t spi_ss)
{
	/* Activate SPI SS with SINGLE-IO */
	if (!activate_spi_ss(base, spi_ss)) {
		return false;
	}

	/* Clear All ISR */
	write32(SCOBCA1_FPGA_NORFLASH_QSPI_ISR(base), 0xFFFFFFFF);

	/* Clear Status Register (Instructure:0x30) */
	write32(SCOBCA1_FPGA_NORFLASH_QSPI_TDR(base), 0x30);

	/* Inactive SPI SS */
	if (!inactivate_spi_ss(base)) {
		return false;
	}

	/* Confirm SPI Control is Done */
	if (!is_qspi_control_done(base)) {
		return false;
	}

	return true;
}

static bool qspi_norflash_set_quad_read_mode(uint32_t base, uint32_t spi_ss)
{
	/* Active SPI SS with SINGLE-IO */
	if (!activate_spi_ss(base, spi_ss)) {
		return false;
	}

	/* Set QUAD-IO read mode with 4byte (Instruction:0xEC) */
	write32(SCOBCA1_FPGA_NORFLASH_QSPI_TDR(base), 0xEC);
	if (!is_qspi_idle(base)) {
		return false;
	}

	/* Keep SPI SS for QUAD Read */
	return true;
}

static bool qspi_norflash_quad_read_data(uint32_t base, uint32_t spi_ss, mm_reg_t mem_addr,
					 uint8_t read_size, uint8_t *read_vals)
{
	bool ret;

	/* Activate SPI SS with Quad-IO SPI Mode */
	write32(SCOBCA1_FPGA_NORFLASH_QSPI_ACR(base), SCQSPI_SPI_MODE_QUAD + spi_ss);

	/* Send Memory Address (4byte) */
	write_mem_addr_to_flash(base, mem_addr);

	/* Send Mode (0x00) */
	write32(SCOBCA1_FPGA_NORFLASH_QSPI_TDR(base), 0x00);

	/* Send Dummy Cycle */
	send_dummy_cycle(base, SCQSPI_NOR_FLASH_DUMMY_CYCLE_COUNT);
	if (!is_qspi_idle(base)) {
		return false;
	}

	/* Read Initial RX data and verify */
	ret = read_rx_data(base, read_size, read_vals);
	if (!ret) {
	}

	/* Inactive the SPI SS */
	if (!inactivate_spi_ss(base)) {
		return false;
	}

	return ret;
}

int sc_qspi_nor_copy_to_hrmem(uint8_t mem_no, uint32_t size, mm_reg_t sram_mem_addr)
{
	int ret = 0;
	uint32_t base = SCOBCA1_FPGA_CFG_BASE_ADDR;
	uint32_t spi_ss;
	uint8_t read_vals[SCQSPI_RX_FIFO_MAX_BYTE];
	uint32_t loop_count;
	mm_reg_t cfg_mem_addr = FIXED_PARTITION_OFFSET(update_fsw);

	if (mem_no > 1) {
		printk("Invalid Mem number %d (expected 0 or 1)\n", mem_no);
		ret = -EINVAL;
		goto end;
	}

	if (qspi_is_fallback()) {
		cfg_mem_addr = FIXED_PARTITION_OFFSET(golden_fsw);
	}
	printk("Load from Configuration Memory address : 0x%08lx\n", cfg_mem_addr);

	if (!qspi_select_mem(base, mem_no, &spi_ss)) {
		ret = -EIO;
		goto end;
	}

	if (!clear_status_register(base, spi_ss)) {
		ret = -EIO;
		goto end;
	}

	loop_count = size / SCQSPI_RX_FIFO_MAX_BYTE;
	for (int i = 0; i < loop_count; i++) {

		if (!qspi_norflash_set_quad_read_mode(base, spi_ss)) {
			ret = -EIO;
			goto end;
		}

		if (!qspi_norflash_quad_read_data(base, spi_ss, cfg_mem_addr,
						  SCQSPI_RX_FIFO_MAX_BYTE, read_vals)) {
			ret = -EIO;
			goto end;
		}

		memcpy((void *)sram_mem_addr, read_vals, SCQSPI_RX_FIFO_MAX_BYTE);

		cfg_mem_addr += SCQSPI_RX_FIFO_MAX_BYTE;
		sram_mem_addr += SCQSPI_RX_FIFO_MAX_BYTE;
	}

end:
	return ret;
}
