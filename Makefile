# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright © 2015-2018 ANSSI. All Rights Reserved.
CFLAGS += -Wall
#CURL_DEPENDS := $(shell curl-config --libs)
CURL_DEPENDS := -lcurl -lssl -lcrypto -lz
BEACON_DEFINES := \
	-DBEACON_URL_FILE=\"$(BEACON_URL_FILE)\" \
	-DBEACON_REPORT_DIR=\"$(BEACON_REPORT_DIR)\" \
	-DBEACON_CERT_FILE=\"$(BEACON_CERT_FILE)\" \
	-DBEACON_KEY_FILE=\"$(BEACON_KEY_FILE)\" \
	-DBEACON_CA_PATH=\"$(BEACON_CA_PATH)\"

.PHONY: all debug clean

all: clip-beacon-send clip-beacon

debug: CFLAGS += -g
debug: all

clip-beacon-send: clip-beacon-send.c
ifndef BEACON_REPORT_DIR
	$(error BEACON_REPORT_DIR must be defined)
endif
ifndef BEACON_URL_FILE
	$(error BEACON_URL_FILE must be defined)
endif
ifndef BEACON_CERT_FILE
	$(error BEACON_CERT_FILE must be defined)
endif
ifndef BEACON_KEY_FILE
	$(error BEACON_KEY_FILE must be defined)
endif
ifndef BEACON_CA_PATH
	$(error BEACON_CA_PATH must be defined)
endif
	$(CC) $(CFLAGS) $(CURL_DEPENDS) $(LDFLAGS) $(BEACON_DEFINES) -o $@ $^

clip-beacon: clip-beacon.in
ifndef BEACON_REPORT_DIR
	$(error BEACON_REPORT_DIR must be defined)
endif
ifndef BEACON_SCRIPTS
	$(error BEACON_SCRIPTS must be defined)
endif
ifndef BEACON_SEND_BIN
	$(error BEACON_SEND_BIN must be defined)
endif
	sed -e "s,@BEACON_REPORT_DIR@,$(BEACON_REPORT_DIR)," \
		-e "s,@BEACON_SCRIPTS@,$(BEACON_SCRIPTS)," \
		-e "s,@BEACON_SEND_BIN@,$(BEACON_SEND_BIN)," \
		< $< > $@

clean:
	rm -f clip-beacon-send clip-beacon
