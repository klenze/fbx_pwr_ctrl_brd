#ok, a makefile full of phony targets is a bit strange. sue me. 

#.build/uno/firmware.hex:
build:
	./update_git_commit_string.sh
	ino build -f="-I/usr/share/arduino/libraries/Wire/ -O1"

test: build
	ino upload
	screen -U /dev/ttyUSB0 115200

clean:
	ino clean
.PHONY: build test clean
