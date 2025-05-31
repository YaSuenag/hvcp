.PHONY: clean

CXX := g++
CXXFLAGS = -c -g -O3 -Wall -std=c++17

TARGET := hvcp-server
OBJS := server.o command-proc.o


all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^

.cpp.o:
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	$(RM) $(TARGET) $(OBJS)

install: $(TARGET)
	cp -f $(TARGET) /usr/local/bin/
	cp -f hvcp.service /usr/lib/systemd/system/

uninstall:
	systemctl disable hvcp
	$(RM) /usr/lib/systemd/system/hvcp.service /usr/local/bin/$(TARGET)
