
CPU = 8051

FLAGS += --xram-loc 0xc000 --xram-size 0x2da2 --model-medium

test: main.bin
	sudo cc-tool -e -w $^ -f
