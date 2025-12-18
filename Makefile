.PHONY: all server client qt clean help

all: server qt

server:
	@echo "Building server..."
	$(MAKE) -f Makefile.server

client:
	@echo "Building command-line client..."
	$(MAKE) -f Makefile.client

qt:
	@echo "Building Qt GUI client..."
	$(MAKE) -f Makefile.GUI

clean:
	@echo "Cleaning all build files..."
	$(MAKE) -f Makefile.server clean
	$(MAKE) -f Makefile.client clean
	$(MAKE) -f Makefile.GUI clean
	rm -rf build/
	rm -rf server_files/

help:
	@echo "Available targets:"
	@echo "  make server  - Build multi-threaded server"
	@echo "  make client  - Build command-line client"
	@echo "  make qt      - Build Qt GUI client"
	@echo "  make all     - Build server and CLI client"
	@echo "  make clean   - Remove all build files"
	@echo ""
	@echo "Run commands:"
	@echo "  ./fileserver_mt 8080"
	@echo "  ./fileclient localhost 8080 list"
	@echo "  ./qt_fileclient"