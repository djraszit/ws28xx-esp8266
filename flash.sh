#!/bin/bash

usage(){
	echo "Przykład użycia programu"
	echo "-p --port /dev/ttyUSB0"
	return
}

PORT=/dev/ttyUSB0

while [[ -n $1 ]]; do
	case $1 in
		-p | --port)
			shift
			if [[ $1 =~ "/dev/" ]];then
				PORT=$1
			else
				echo "Podaj ścieżkę do portu 'tty'"
				exit 1
			fi
			;;
		*)
			usage >&2
			exit 1
			;;
	esac
	shift
done

echo "Użyję portu $PORT"


sudo esptool --port $PORT write_flash 0x00000 BIN/eagle.flash.bin 0x20000 BIN/eagle.irom0text.bin

