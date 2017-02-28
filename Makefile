CC=gcc
CFLAGS := -Wall -w -g -D_GNU_SOURCE -D_REENTRANT
CFLAGS += -I/opt/etherlab/include
CFLAGS += -I/usr/xenomai/include

ECAT_LIBS += -L/opt/etherlab/lib -lethercat_rtdm
XENO_LIBS += -L/usr/xenomai/lib -lrtdm -lnative -lxenomai -lpthread -lrt

YASKAWA_FRAME = yaskawa
YASKAWA_DC = yaskawa_dc
HIGEN_DC = higen_dc_2
PANASONIC = panasonic
SANYO = sanyo
HYBRID = hybid

all: $(YASKAWA_FRAME) $(YASKAWA_DC) $(HIGEN_DC) $(SANYO) $(PANASONIC) $(HYBRID)

$(YASKAWA_FRAME): yaskawa.c
	$(CC) $(CFLAGS) -static -o $@ $^ $(ECAT_LIBS) $(XENO_LIBS)

$(YASKAWA_DC): yaskawa_dc.c
	$(CC) $(CFLAGS) -static -o $@ $^ $(ECAT_LIBS) $(XENO_LIBS)

$(HIGEN_DC): higen_dc_2.c
	$(CC) $(CFLAGS) -static -o $@ $^ $(ECAT_LIBS) $(XENO_LIBS)

$(PANASONIC): panasonic.c
	$(CC) $(CFLAGS) -static -o $@ $^ $(ECAT_LIBS) $(XENO_LIBS)

$(SANYO): sanyo.c
	$(CC) $(CFLAGS) -static -o $@ $^ $(ECAT_LIBS) $(XENO_LIBS)

$(HYBRID): yas_pana_2.c
	$(CC) $(CFLAGS) -static -o $@ $^ $(ECAT_LIBS) $(XENO_LIBS)
clean:
	rm $(YASKAWA_FRAME) $(YASKAWA_DC) $(HIGEN_DC) $(SANYO) $(PANASONIC) $(HYBRID)

