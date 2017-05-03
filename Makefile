SHELL := bash -O extglob
TRUNK?=$(PWD)

STRIP?=strip
CFLAGS+=-Wno-deprecated
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

COMBINED_BUILD=1
ifeq ($(REVERSE_CONNECT),1)
	COMBINED_BUILD=0
	CLI_CFLAGS += -DCLI_REVERSE_CONNECT
	SVR_CFLAGS += -DSVR_REVERSE_CONNECT
endif


ifeq ($(BUILDSTATIC),1)
	LDFLAGS+="-static"
endif

CLI_CFLAGS += $(CFLAGS)
SVR_CFLAGS += $(CFLAGS)

DROPBEAR=dropbear
SCP=scp
DBCLIENT=dbclient

DB_SERVER_STAMP=.db_server
DB_CLIENT_STAMP=.db_client
CONFIG_CLI_STAMP=.config_cli_stamp
CONFIG_SVR_STAMP=.config_svr_stamp

CONFIG_OPTIONS=--disable-syslog --disable-zlib --disable-pam --disable-shadow

all:$(DB_SERVER_STAMP) $(DB_CLIENT_STAMP)


client:$(DB_CLIENT_STAMP)

server:$(DB_SERVER_STAMP)

db_clean:
	-make -C src clean


$(DB_SERVER_STAMP): $(CONFIG_SVR_STAMP) src/$(DROPBEAR)
	cp src/$(DROPBEAR) .
	touch $@
	$(STRIP) $(DROPBEAR)

$(DB_CLIENT_STAMP): $(CONFIG_CLI_STAMP) src/$(SCP) src/$(DBCLIENT)
	cp $ src/$(SCP) src/$(DBCLIENT) .
	touch $@
	$(STRIP) $(SCP)


$(CONFIG_CLI_STAMP):
	cd src && \
	./configure --verbose LDFLAGS=$(LDFLAGS) $(CONFIG_OPTIONS) --host=$(HOST) CFLAGS="$(CLI_CFLAGS)"
	touch src/*.c
	touch src/*.h
	touch $@

$(CONFIG_SVR_STAMP):
	cd src && \
	./configure --verbose LDFLAGS=$(LDFLAGS) $(CONFIG_OPTIONS) --host=$(HOST) CFLAGS="$(SVR_CFLAGS)"
	touch src/*.c
	touch src/*.h
	touch $@

src/$(SCP) src/$(DBCLIENT):
	make -C src PROGRAMS="dbclient scp"
	

src/$(DROPBEAR):
	make -C src PROGRAMS="dropbear"


clean:
	-rm -f $(DB_SERVER_STAMP) $(DB_CLIENT_STAMP) $(CONFIG_CLI_STAMP) $(CONFIG_SVR_STAMP)
	-rm $(DROPBEAR)
	-rm $(SCP)
	-rm $(DBCLIENT)
	make -C src distclean

