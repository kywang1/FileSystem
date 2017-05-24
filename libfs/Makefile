# Libuthread Makefile
targets := libfs
objs    := disk.o fs.o

CC      := gcc 
CFLAGS  := -Wall -Werror 
CFLAGS  += -g 
PANDOC  := pandoc

ifneq ($(V),1) 
Q = @ 
endif

all: $(targets)

deps := $(patsubst %.o,%.d,$(objs)) 
-include $(deps) 
DEPFLAGS = -MMD -MF $(@:.o=.d)

libfs: $(objs)     
	@echo "CC $@"     
	ar rcs libfs $(objs)  

%.o: %.c     
	@echo "CC $@"     
	$(Q)$(CC) $(CFLAGS) -c -o $@ $< $(DEPFLAGS)

clean:     
	@echo "clean"     
	$(Q)rm -f $(targets) $(objs) $(deps) 
