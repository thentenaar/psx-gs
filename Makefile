
CC = gcc
CC += -m32 -O2
STRIP = strip

all: libGS.so gsconfig

libGS.so:
	@echo "Building libGS.so..."
	@$(CC) -shared -rdynamic -o libGS.so gpu.c state.c

gsconfig: 
	@echo "Building gsconfig..."
	@$(CC) `pkg-config --cflags gtk+-2.0` -o gsconfig ui.c state.c \
	       `pkg-config --libs gtk+-2.0`
	@$(STRIP) -s gsconfig

clean:
	@rm -f gsconfig libGS.so

install: libGS.so gsconfig
	@mkdir -p ~/.epsxe/cfg
	@mkdir -p ~/.epsxe/plugins
	@install -m0755 libGS.so ~/.epsxe/plugins/libGS.so
	@install -m0755 gsconfig ~/.epsxe/cfg/gsconfig

