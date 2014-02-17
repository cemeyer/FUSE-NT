SRCS=fuseinit.cc fuseio.cc fusequery.cc fuseutil.cc hashmap.cc fuse.rc
OBJS=fuseinit.o  fuseio.o  fusequery.o  fuseutil.o  hashmap.o  fuse.o

MINGWROOT?=/usr/x86_64-w64-mingw32/sys-root/mingw

CXXFLAGS?=-O2 -g -pipe -Wall -Wp,-D_FORTIFY_SOURCE=2 -fexceptions --param=ssp-buffer-size=4

CXXFLAGS+=-fvisibility=hidden

CXXFLAGS+=-I$(MINGWROOT)/include/ddk
CXXFLAGS+=-nostdlib

CXXFLAGS+=-L$(MINGWROOT)/lib

# Some magic constants below borrowed from ReactOS ... don't know if they're important
ntfuse.sys: $(OBJS)
	$(CXX) $(CXXFLAGS) -shared $^ -o $@ \
		-Wl,--entry=DriverEntry -Wl,--wdmdriver \
		-Wl,--exclude-all-symbols -Wl,--file-alignment=0x1000 \
		-Wl,--section-alignment=0x1000 -Wl,--image-base=0x10000 \
		-lntoskrnl -lgcc_s -lstdc++

%.o: %.rc
	$(WINDRES) --input-format rc --input $< --output-format coff --output $@


clean:
	rm -f $(OBJS) ntfuse.sys
