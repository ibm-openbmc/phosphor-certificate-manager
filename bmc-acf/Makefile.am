bin_PROGRAMS = \
	bmc-acf

noinst_HEADERS = \
	acf_manager.hpp

bmc_acf_SOURCES = \
	mainapp.cpp \
	acf_manager.cpp

bmc_acf_LDFLAGS = \
	$(SDBUSPLUS_LIBS) \
	$(SDEVENTPLUS_LIBS) \
	$(PHOSPHOR_DBUS_INTERFACES_LIBS) \
	$(PHOSPHOR_LOGGING_LIBS) \
	$(OPENSSL_LIBS) \
	-lstdc++fs \
	-lcelogin

bmc_acf_CXXFLAGS = \
	$(SYSTEMD_CFLAGS) \
	$(SDEVENTPLUS_CFLAGS) \
	$(PHOSPHOR_DBUS_INTERFACES_CFLAGS) \
	$(PHOSPHOR_LOGGING_CFLAGS) \
	$(JSMN_CFLAGS)
