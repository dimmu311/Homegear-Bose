AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -Wall -std=c++11 -DFORTIFY_SOURCE=2 -DGCRYPT_NO_DEPRECATED
AM_LDFLAGS = -Wl,-rpath=/lib/homegear -Wl,-rpath=/usr/lib/homegear -Wl,-rpath=/usr/local/lib/homegear

libdir = $(localstatedir)/lib/homegear/modules
lib_LTLIBRARIES = mod_bose.la
mod_bose_la_SOURCES = BosePacket.cpp Bose.cpp Factory.cpp GD.h Interfaces.h Interfaces.cpp BosePeer.cpp BosePacket.h BosePeer.h Bose.h GD.cpp Factory.h PhysicalInterfaces/IBoseInterface.h PhysicalInterfaces/EventServer.h PhysicalInterfaces/IBoseInterface.cpp PhysicalInterfaces/EventServer.cpp BoseCentral.h BoseCentral.cpp
mod_bose_la_LDFLAGS =-module -avoid-version -shared
install-exec-hook:
	rm -f $(DESTDIR)$(libdir)/mod_bose.la
