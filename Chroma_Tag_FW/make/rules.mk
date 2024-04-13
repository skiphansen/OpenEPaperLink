BUILD_DIR?=$(BUILDS_DIR)/build

HDR_TOOL_DIR=$(FIRMWARE_ROOT)/add_ota_hdr
HDR_TOOL_BUILD_DIR=$(HDR_TOOL_DIR)/build
HDR_TOOL=$(HDR_TOOL_BUILD_DIR)/add_ota_hdr

OBJS = $(patsubst %.c,$(BUILD_DIR)/%.$(OBJFILEEXT),$(notdir $(SOURCES)))

$(BUILD_DIR)/%.$(OBJFILEEXT): %.c
	$(CC) -c $^ $(FLAGS) -o $(BUILD_DIR)/$(notdir $@)

$(BUILD_DIR)/main.ihx: $(OBJS)
	rm -f $(BUILD_DIR)/$(notdir $@)
	$(CC) $^ $(FLAGS) -o $(BUILD_DIR)/$(notdir $@)

$(BUILD_DIR)/main.elf: $(OBJS)
	$(CC) $(FLAGS) -o $(BUILD_DIR)/$(notdir $@) $^

%.bin: %.ihx
	objcopy $^ $(BUILD_DIR)/$(IMAGE_NAME).bin -I ihex -O binary
	@# Display sizes, we're critically short for code and RAM space !
	@grep '^Area' $(BUILD_DIR)/main.map | head -1
	@echo '--------------------------------        ----        ----        ------- ----- ------------'
	@grep = $(BUILD_DIR)/main.map | grep XDATA
	@grep = $(BUILD_DIR)/main.map | grep CODE
	@echo -n ".bin size: "
	@ls -l $(BUILD_DIR)/$(IMAGE_NAME).bin | cut -f5 -d' '

$(BUILD_DIR):
	if [ ! -e $(BUILD_DIR) ]; then mkdir -p $(BUILD_DIR); fi

${HDR_TOOL}: $(wildcard $(HDR_TOOL_DIR)/*) $(FW_COMMON_DIR)/ota_hdr.h
	cmake -S $(HDR_TOOL_DIR) -B $(HDR_TOOL_BUILD_DIR)
	cmake --build $(HDR_TOOL_BUILD_DIR)

$(RELEASE_BINS): $(BUILD_DIR)/main.ihx $(BUILD_DIR)/$(IMAGE_NAME).bin

.PHONY: all clean veryclean flash reset release debug


all: $(BUILD_DIR) $(PREBUILT_DIR) $(TARGETS)

clean:
	@rm -rf $(BUILD_DIR)
	@rm -rf $(RELEASE_BINS)

veryclean: 
	@rm -rf $(BUILDS_DIR)
	@rm -rf $(PREBUILT_DIR)

flash:	all 
	cc-tool -e -w $(BUILD_DIR)/$(IMAGE_NAME).bin

reset:	
	cc-tool --reset

$(PREBUILT_DIR)/$(IMAGE_NAME).bin: all
	cp $(BUILD_DIR)/$(IMAGE_NAME).bin $(PREBUILT_DIR)

$(PREBUILT_DIR)/$(OTA_IMAGE_NAME).bin: all
	$(HDR_TOOL) $(PREBUILT_DIR)/$(IMAGE_NAME).bin

release: $(RELEASE_BINS)

debug:
	@echo "FIRMWARE_ROOT=$(FIRMWARE_ROOT)"
	@echo "SOC_DIR=$(SOC_DIR)"
	@echo "CPU_DIR=$(CPU_DIR)"
	@echo "BOARD=$(BOARD)"
	@echo "BUILD=$(BUILD)"
	@echo "BUILD_DIR=$(BUILD_DIR)"
	@echo "SOURCES=$(SOURCES)"
	@echo "TARGETS=$(TARGETS)"
	@echo "FW_VER=$(FW_VER)"
	@echo "LUT=$(LUT)"
	@echo "FLAGS=$(FLAGS)"
	@echo "HDR_TOOL_DIR=$(HDR_TOOL_DIR)"
	@echo "HHDR_TOOL_BUILD_DIR=$(HDR_TOOL_BUILD_DIR)"
	@echo "HDR_TOOL=$(HDR_TOOL)"

