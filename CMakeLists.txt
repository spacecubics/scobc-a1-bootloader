cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(scobc-a1-bootloader)

target_sources(app PRIVATE
  src/main.c
  src/sc_qspi_nor.c)
