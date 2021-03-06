UVUDEC_ROOT=../../uvudec
LIBUVUDEC_ROOT=$(UVUDEC_ROOT)/libuvudec
LIBUVUDEC_INCLUDE_DIR=$(LIBUVUDEC_ROOT)
UVUDEC_LIB_DIR=$(UVUDEC_ROOT)/lib
LIBUVUDEC_LIB_DIR=$(UVUDEC_LIB_DIR)

ifeq ($(USING_LIBUVUDEC),Y)
	CFLAGS += -I$(LIBUVUDEC_INCLUDE_DIR)
	LFLAGS += -L$(LIBUVUDEC_LIB_DIR) -luvudec
	LFLAGS += -Wl,-rpath,$(LIBUVUDEC_LIB_DIR)
endif

