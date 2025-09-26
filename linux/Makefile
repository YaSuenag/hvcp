.PHONY: clean

CXX := g++
CXXFLAGS = -c -g -O3 -Wall -std=c++17

TARGET := hvcp-server
OBJS := server.o command-proc.o

PREFIX ?=
BINDIR ?= /usr/local/bin


all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^

.cpp.o:
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	$(RM) $(TARGET) $(OBJS)

install: $(TARGET)
	cp -f $(TARGET) $(PREFIX)/$(BINDIR)
	sed -e "s|^ExecStart=.\+$$|ExecStart=$(BINDIR)/$(TARGET)|" hvcp.service > $(PREFIX)/usr/lib/systemd/system/hvcp.service

uninstall:
	systemctl disable hvcp
	$(RM) $(PREFIX)/usr/lib/systemd/system/hvcp.service $(PREFIX)/$(BINDIR)/$(TARGET)
