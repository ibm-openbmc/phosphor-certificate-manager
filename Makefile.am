AM_DEFAULT_SOURCE_EXT = .cpp

# Build these headers, don't install them
noinst_HEADERS = \
	certs_manager.hpp \
	argument.hpp \
	certificate.hpp \
	csr.hpp \
	watch.hpp

bin_PROGRAMS = \
	phosphor-certificate-manager

phosphor_certificate_manager_SOURCES = \
	mainapp.cpp \
	certs_manager.cpp \
	argument.cpp \
	certificate.cpp \
	csr.cpp \
	watch.cpp

phosphor_certificate_manager_LDFLAGS = \
	$(SDBUSPLUS_LIBS) \
	$(SDEVENTPLUS_LIBS) \
	$(PHOSPHOR_DBUS_INTERFACES_LIBS) \
	$(PHOSPHOR_LOGGING_LIBS) \
	$(OPENSSL_LIBS) \
	-lstdc++fs

phosphor_certificate_manager_CXXFLAGS = \
	$(SYSTEMD_CFLAGS) \
	$(SDEVENTPLUS_LIBS) \
	$(PHOSPHOR_DBUS_INTERFACES_CFLAGS) \
	$(PHOSPHOR_LOGGING_CFLAGS)

SUBDIRS = . test
