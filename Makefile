CC = arm-linux-gnueabihf-gcc

IMG_PATH=../DEPLOY/Image_ARM/rootfs


CFLAGS := -Wall -w -g -D_GNU_SOURCE -D_REENTRANT
CFLAGS += -I${IMG_PATH}/opt/etherlab/include
CFLAGS += -I${IMG_PATH}/usr/xenomai/include

ECAT_LIBS += -L${IMG_PATH}/opt/etherlab/lib -lethercat_rtdm
XENO_LIBS += -L${IMG_PATH}/usr/xenomai/lib -lrtdm -lnative -lxenomai -lpthread

YASKAWA_FRAME = yaskawa
YASKAWA_DC = yaskawa_dc
HIGEN_DC = higen_dc_2

all: $(YASKAWA_FRAME) $(YASKAWA_DC) $(HIGEN_DC)

$(YASKAWA_FRAME): yaskawa.c
	$(CC) $(CFLAGS) -static -o $@ $^ $(ECAT_LIBS) $(XENO_LIBS)
	cp -v $@ ${IMG_PATH}/root

$(YASKAWA_DC): yaskawa_dc.c
	$(CC) $(CFLAGS) -static -o $@ $^ $(ECAT_LIBS) $(XENO_LIBS)
	cp -v $@ ${IMG_PATH}/root

$(HIGEN_DC): higen_dc_2.c
	$(CC) $(CFLAGS) -static -o $@ $^ $(ECAT_LIBS) $(XENO_LIBS)
	cp -v $@ ${IMG_PATH}/root

clean:
	rm $(YASKAWA_FRAME) $(YASKAWA_DC) $(HIGEN_DC)

