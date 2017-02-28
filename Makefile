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
SANYO_FRAME = sanyo

# Following application will be updated
SANYO_DC = sanyo_dc
PANASONIC_FRAME = panasonic
PANASONIC_DC = panasonic_dc
AM3359_FRAME = ice
AM3359_DC = ice_dc

all: $(YASKAWA_FRAME) $(YASKAWA_DC) $(HIGEN_DC) $(SANYO_FRAME)

$(YASKAWA_FRAME): yaskawa.c
	$(CC) $(CFLAGS) -static -o $@ $^ $(ECAT_LIBS) $(XENO_LIBS)
	cp -v $@ ${IMG_PATH}/root

$(YASKAWA_DC): yaskawa_dc.c
	$(CC) $(CFLAGS) -static -o $@ $^ $(ECAT_LIBS) $(XENO_LIBS)
	cp -v $@ ${IMG_PATH}/root

$(HIGEN_DC): higen_dc_2.c
	$(CC) $(CFLAGS) -static -o $@ $^ $(ECAT_LIBS) $(XENO_LIBS)
	cp -v $@ ${IMG_PATH}/root

$(SANYO_FRAME): sanyo.c
	$(CC) $(CFLAGS) -static -o $@ $^ $(ECAT_LIBS) $(XENO_LIBS)
	cp -v $@ ${IMG_PATH}/root

clean:
	rm $(YASKAWA_FRAME) $(YASKAWA_DC) $(HIGEN_DC) $(SANYO_FRAME)

