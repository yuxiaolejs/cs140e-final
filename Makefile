# Just go run all the Makefiles under the labs dir
all: main.bin main.elf main.list
	$(MAKE) -C final
main.%: final/main.%
	mv final/main.$* main.$*
final/main.%: final/Makefile
	$(MAKE) -C final all
clean:
	$(MAKE) -C final clean
	rm -f main.bin main.elf main.list