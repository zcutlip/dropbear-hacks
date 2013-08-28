TRUNK?=$(PWD)

STRIP?=strip

ifeq ($(DEBUG),1)
	CFLAGS += -DDEBUG_TRACE
	ENABLE_DB_LOGGING=1
endif

ifeq ($(ENABLE_DB_LOGGING),1)
	CFLAGS += -DENABLE_DB_LOGGING
endif

ifeq ($(FAKE_ROOT),1)
	CFLAGS += -DFAKE_ROOT
endif

ifdef ALT_SHELL
	CFLAGS += -DALT_SHELL=\\\"$(ALT_SHELL)\\\"
endif

DROPBEAR=dropbear
SCP=scp


CONFIG_OPTIONS=--disable-syslog --disable-zlib --disable-pam --disable-shadow

all:src/$(DROPBEAR)
	cp src/$(DROPBEAR) .
	cp src/$(SCP) .

	$(STRIP) $(SCP)
	$(STRIP) $(DROPBEAR)

src/$(DROPBEAR):
	@echo $(PATH)
	OLDPWD=$(PWD)
	cd src && \
	./configure --verbose LDFLAGS=" -static" $(CONFIG_OPTIONS) --host=$(HOST) \
	CFLAGS="$(CFLAGS)" && \
	make PROGRAMS="dropbear scp" STATIC=1 
	cd $(OLDPWD)

clean:
	-rm $(DROPBEAR)
	-rm $(SCP)
	make -C src distclean

