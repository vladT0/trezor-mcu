export MEMORY_PROTECT=0
make -C vendor/libopencm3/
make -C vendor/nanopb/generator/proto/
make -C firmware/protob/
make
make -C bootloader/ align
make -C firmware/ sign
cp bootloader/bootloader.bin bootloader/combine/bl.bin
cp firmware/trezor.bin bootloader/combine/fw.bin
cd bootloader/combine/ && ./prepare.py

