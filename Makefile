# Iris1 bootloader + app build

CC=arm-none-eabi-gcc
CXX=arm-none-eabi-g++
OBJCOPY=arm-none-eabi-objcopy
BETA_BIN=release/Iris1_v2.0-beta1.bin

CFLAGS_COMMON=-mcpu=cortex-m0 -mthumb -Os -ffunction-sections -fdata-sections -fno-builtin \
  -Wall -Wextra -Werror
DEPFLAGS=-MMD -MP
CXXFLAGS=$(CFLAGS_COMMON) -fno-exceptions -fno-rtti -fno-threadsafe-statics -std=gnu++17
CFLAGS=$(CFLAGS_COMMON) -std=c11

LDFLAGS_COMMON=-Wl,--gc-sections -nostartfiles -Wl,-Map=$@.map

INCLUDES=-Iinclude

APP_SRCS=$(wildcard src/*.cpp) startup_app.c
APP_OBJS=$(patsubst %.cpp,build/%.o,$(filter %.cpp,$(APP_SRCS))) \
         $(patsubst %.c,build/%.o,$(filter %.c,$(APP_SRCS)))

BOOT_SRCS=bootloader/boot_main.c bootloader/startup.c
BOOT_OBJS=$(patsubst %.c,build/%.o,$(BOOT_SRCS))
DEPFILES=$(APP_OBJS:.o=.d) $(BOOT_OBJS:.o=.d)

all: precheck build/bootloader.elf build/bootloader.bin build/iris1.elf build/iris1.bin build/iris1_combined.bin

precheck: test

.PHONY: precheck

build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@

build/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@

build/bootloader.elf: $(BOOT_OBJS) linker/bootloader.ld
	$(CC) $(CFLAGS) $(BOOT_OBJS) -T linker/bootloader.ld $(LDFLAGS_COMMON) -o $@

build/bootloader.bin: build/bootloader.elf
	$(OBJCOPY) -O binary $< $@

build/iris1.elf: $(APP_OBJS) linker/iris_app.ld
	$(CXX) $(CXXFLAGS) $(APP_OBJS) -T linker/iris_app.ld $(LDFLAGS_COMMON) -o $@

build/iris1.bin: build/iris1.elf
	$(OBJCOPY) -O binary $< $@

build/iris1_combined.bin: build/bootloader.bin build/iris1.bin tools/combine_images.py
	python3 tools/combine_images.py \
		--bootloader build/bootloader.bin \
		--app build/iris1.bin \
		--output $@

flash: build/iris1_combined.bin
	openocd -f interface/stlink.cfg -f target/stm32f0x.cfg \
		-c "program build/iris1_combined.bin 0x08000000 verify reset exit"

package_beta: build/iris1_combined.bin
	@mkdir -p $(dir $(BETA_BIN))
	cp build/iris1_combined.bin $(BETA_BIN)

clean:
	rm -rf build
	rm -rf release

.PHONY: all flash clean package_beta

build/iris1_noboot.elf: $(APP_OBJS) linker/iris_app_noboot.ld
	$(CXX) $(CXXFLAGS) $(APP_OBJS) -T linker/iris_app_noboot.ld $(LDFLAGS_COMMON) -o $@

build/bootloader.elf build/bootloader.bin build/iris1.elf build/iris1.bin build/iris1_combined.bin build/iris1_noboot.elf: | precheck

flash_app_noboot: build/iris1_noboot.elf
	openocd -f interface/stlink.cfg -f target/stm32f0x.cfg \
		-c "program build/iris1_noboot.elf verify reset exit"

.PHONY: flash_app_noboot

audio_wav: build/iris1.bin tools/iris1_audio_boot_wav.py
	python3 tools/iris1_audio_boot_wav.py --input build/iris1.bin --output build/iris1_update.wav

.PHONY: audio_wav

test:
	cmake -S tests -B build/tests -DCMAKE_BUILD_TYPE=Debug
	cmake --build build/tests -j4
	ctest --test-dir build/tests --output-on-failure

.PHONY: test

-include $(DEPFILES)
