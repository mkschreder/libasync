CC:=gcc 
CFLAGS:=-std=c99 -Iinclude
AR:=ar

#define BuildStaticLibrary
BIN_DIR:=bin
OUTDIR:=$(BIN_DIR)/lib
BUILD_DIR:=build
TARGET:=$(OUTDIR)/libasync.a
SRC_DIR:=src
curdir:=$(SRC_DIR)
-include $(SRC_DIR)/Makefile

obj-y:=$(patsubst %.o,$(BUILD_DIR)/%.o,$(obj-y))
dirs:=$(uniq $(dir $(obj-y)))

all: $(OUTDIR)/libasync.a $(obj-y);

$(BUILD_DIR)/$(SRC_DIR)/%.o: $(SRC_DIR)/%.c 
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<
$(OUTDIR): 
	mkdir -p $(OUTDIR)
$(TARGET): $(obj-y)
	$(AR) cr $(TARGET) $(obj-y)
#endef

#$(eval $(call BuildStaticLibrary,src,libasync.a))

define ExampleTemplate
$(BIN_DIR)/$(1): $(BIN_DIR)/lib/libasync.a $$(wildcard examples/$(1)/*.c)
	$$(CC) $$(CFLAGS) -o $$@ $$(wildcard examples/$(1)/*.c) -L$(BIN_DIR)/lib -lc -lasync 
endef

$(eval $(call ExampleTemplate,hello_world))
$(eval $(call ExampleTemplate,tcp_server))

#EXAMPLE:=hello_world
#SRC_DIR:=examples/$(EXAMPLE)
#obj-y:=$(wildcard $(SRC_DIR)/*.c)
#OUTDIR:=bin
#TARGET:= $(OUTDIR)/$(EXAMPLE)
#$(TARGET): $(obj-y)
	
clean: 
	find . -type f -name "*.o" | xargs rm -f
	rm -rf build
	rm -rf bin
