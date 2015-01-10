CC = wcc
CFLAGS = -q -ms -fo=.obj -DSC12 -Dsocklen_t=int -D__MWERKS__ -DEMULATE1
LINKER = wlink
LFLAGS = option quiet format dos libpath $(%WATCOM)/lib286/dos:$(%WATCOM)/lib286:$(%BECKCLIB) library clib222s.lib

OBJS = dmxin.obj
OBJS1 = dmxout.obj

.c.obj : .autodepend
	$(CC) $(CFLAGS) $< 

dmxin.exe : $(OBJS)
	$(LINKER) $(LFLAGS) name $@ file { $< }

dmxout.exe : $(OBJS1)
	$(LINKER) $(LFLAGS) name $@ file { $< }

clean : .symbolic
	rm -f *.obj *.exe
