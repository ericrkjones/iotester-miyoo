CHAINPREFIX :=/opt/miyoo
CROSS_COMPILE := $(CHAINPREFIX)/usr/bin/arm-miyoo-linux-uclibcgnueabi-

IMAGES = ./console_images
OUTPUT = ./out

CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
STRIP = $(CROSS_COMPILE)strip

SYSROOT     := $(shell $(CC) --print-sysroot)
SDL_CFLAGS  := $(shell $(SYSROOT)/usr/bin/sdl-config --cflags)
SDL_LIBS    := $(shell $(SYSROOT)/usr/bin/sdl-config --libs)

CFLAGS = -ggdb -DTARGET_BITTBOY -DTARGET=$(TARGET) -D__BUILDTIME__="$(BUILDTIME)" -DLOG_LEVEL=3 -g3 $(SDL_CFLAGS) #-I$(CHAINPREFIX)/usr/include/ -I$(SYSROOT)/usr/include/  -I$(SYSROOT)/usr/include/SDL/ # -mhard-float
CXXFLAGS = $(CFLAGS)
LDFLAGS = $(SDL_LIBS) -lfreetype -lSDL -lSDL_image -lSDL_ttf

generic:
	@if [ ! -d $(OUTPUT) ]; then mkdir $(OUTPUT); fi
	$(CXX) $(CFLAGS) $(LDFLAGS) -include iotester-generic.h iotester.c -o $(OUTPUT)/iotester
	@cp $(IMAGES)/generic.png $(OUTPUT)/backdrop.png

pc:
	gcc iotester.c -g -o iotester -ggdb -O0 -DDEBUG -lSDL_image -lSDL -lSDL_ttf -I/usr/include/SDL

ipk: retrogame
	@rm -rf /tmp/.iotester-ipk/ && mkdir -p /tmp/.iotester-ipk/root/home/retrofw/apps/iotester /tmp/.iotester-ipk/root/home/retrofw/apps/gmenu2x/sections/applications
	@cp -r iotester.dge iotester.png backdrop.png /tmp/.iotester-ipk/root/home/retrofw/apps/iotester
	@cp iotester.lnk /tmp/.iotester-ipk/root/home/retrofw/apps/gmenu2x/sections/applications
	@sed "s/^Version:.*/Version: $$(date +%Y%m%d)/" control > /tmp/.iotester-ipk/control
	@tar --owner=0 --group=0 -czvf /tmp/.iotester-ipk/control.tar.gz -C /tmp/.iotester-ipk/ control
	@tar --owner=0 --group=0 -czvf /tmp/.iotester-ipk/data.tar.gz -C /tmp/.iotester-ipk/root/ .
	@echo 2.0 > /tmp/.iotester-ipk/debian-binary
	@ar r iotester.ipk /tmp/.iotester-ipk/control.tar.gz /tmp/.iotester-ipk/data.tar.gz /tmp/.iotester-ipk/debian-binary

clean:
	rm -rf iotester iotester.dge iotester.lnk $(OUTPUT)
