# Libuthread Makefile
targets := libfs.a
objs    := disk.o fs.o

CC      := gcc 
CFLAGS  := -c -Wall -Werror 
CFLAGS  += -g 
PANDOC  := pandoc

ifneq ($(V),1) 
Q = @ 
endif

all: $(targets)

deps := $(patsubst %.o,%.d,$(objs)) 
-include $(deps) 
DEPFLAGS = -c -MMD -MF $(@:.o=.d)

libfs.a: $(objs)     
	@echo "CC $@"     
	ar rcs libfs.a $(objs)  

%.o: %.c     
	@echo "CC $@"     
	$(Q)$(CC) $(CFLAGS) -c -o $@ $< $(DEPFLAGS)

clean:     
	@echo "clean"     
	$(Q)rm -f $(targets) $(objs) $(deps) 
