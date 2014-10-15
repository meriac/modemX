CXX     :=g++
SOX     :=sox
PLAY    :=play
TARGET  :=modemx
SOURCES :=src/main.cpp src/crypto.cpp

LIBS    :=-lm -pthread

SAMPLING:=48000

# determine program version
PROGRAM_VERSION:=$(shell git describe --tags --abbrev=4 --dirty 2>/dev/null | sed s/^v//)
ifeq ("$(PROGRAM_VERSION)","")
    PROGRAM_VERSION:='unknown'
endif

DEBUG   :=-O3
CXXOPT  :=$(DEBUG)
CXXFLAGS:=-Werror -Wall -DFREQ_SAMPLING_RATE=$(SAMPLING) -D_REENTRANT -DPROGRAM_VERSION=\"$(PROGRAM_VERSION)\" -DPROGRAM_NAME=\"$(TARGET)\" $(ENCRYPTION_KEYS)
LDFLAGS :=$(LIBS)
OBJS    :=$(SOURCES:%.cpp=%.o)
LOGJSON :=$(LOGFILE:%.bin=%.json.bz2)
PREFIX  :=/usr/local

all: $(TARGET)

.PHONY: all install run version cleanall clean wav

version:
	@echo "$(TARGET) version $(PROGRAM_VERSION)"

run: $(TARGET)
	./$(TARGET)

install: $(TARGET) $(FILTER)
	install $(TARGET) $(FILTER) $(PREFIX)/bin/

$(TARGET): .depend $(OBJS)
	$(CXX) $(LDOPT) $(OBJS) $(LDFLAGS) -o $@

.depend: $(SOURCES)
	$(CXX) $(CXXFLAGS) $(CXXOPT) -MM $^ > $@

.cpp.o:
	$(CXX) $(CXXFLAGS) $(CXXOPT) -c $< -o $@

play: $(TARGET)
	./$^ | $(PLAY) -t raw -r $(SAMPLING) -b 16 -c 1 -e unsigned-integer -q -

wav: $(TARGET).wav

$(TARGET).wav: $(TARGET)
	./$^ | $(SOX) -t raw -r $(SAMPLING) -b 16 -c 1 -e unsigned-integer -q - $@

cleanall: clean
	rm -f .depend

clean:
	rm -f $(TARGET) $(OBJS) $(TARGET).wav *~

include .depend
