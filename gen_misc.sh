#!/bin/bash

:<<!
******NOTICE******
MUST set SDK_PATH & BIN_PATH firstly!!!
example:
export SDK_PATH=~/esp_iot_sdk_freertos
export BIN_PATH=~/esp8266_bin

Ta konfiguracja jest odpowiednia dla esp-12e
jeśli chcesz użyć innego modułu,
musisz we własnym zakresie to skonsultować
z dokumentacją techniczną,
a najlepiej użyj gen-misc.sh-original lub gen-misc.bat w windowsie
!

PATH=$PATH:/home/raszit/esp8266/xtensa-lx106-elf.old/bin:/home/raszit/esp8266
export SDK_PATH=/home/raszit/esp8266/ESP8266_RTOS_SDK.new
export BIN_PATH=./BIN

boot=none

echo "boot mode: $boot"

app=0

#spi speed 20MHz  26.7MHz  40MHz  80MHz

spi_speed=40

echo "spi speed: $spi_speed MHz"

#spi mode QIO  QOUT  DIO  DOUT
#jeśli chcesz używać pinów io9 io10 to użyj DIO

spi_mode=QIO

echo "spi mode: $spi_mode"

#choose spi size and map"
#    0= 512KB( 256KB+ 256KB)"
#    2=1024KB( 512KB+ 512KB)"
#    3=2048KB( 512KB+ 512KB)"
#    4=4096KB( 512KB+ 512KB)"
#    5=2048KB(1024KB+1024KB)"
#    6=4096KB(1024KB+1024KB)"
#    7=4096KB(2048KB+2048KB) not support ,just for compatible with nodeMCU board"
#    8=8192KB(1024KB+1024KB)"
#    9=16384KB(1024KB+1024KB)"

spi_size_map=4

echo "BOOT=$boot APP=$app"

echo "start..."

make clean

make BOOT=$boot APP=$app SPI_SPEED=$spi_speed SPI_MODE=$spi_mode SPI_SIZE_MAP=$spi_size_map

OBJDUMP=/home/raszit/esp8266/xtensa-lx106-elf.old/xtensa-lx106-elf/bin/objdump mem_usage.sh .output/eagle/debug/image/eagle.app.v6.out


