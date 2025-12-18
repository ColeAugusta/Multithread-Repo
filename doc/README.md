This is a multi-threaded client-server application with both a command line client (CLI) and a Qt client.
Features password authentication, concurrent clients, Qt GUI and path traversal security.

Setup
------------------------
Linux: sudo apt-get install build-essential g++ make qtbase5-dev qtcreator
Windows: MinGW-64, Qt installation


Building
------------------------
Server: make server OR make -f Makefile.server
CLI client: make client OR make -f Makefile.client
Qt client: make qt OR make -f Makefile.GUI (.GUI.windows on Windows)

Manual compilation:
g++ -std=c++17 -static -Iinclude -o fileserver_mt.exe src/socket.cpp src/thread.cpp src/mutex.cpp src/platform_utils.cpp src/file_manager.cpp src/client_handler.cpp src/server_mt.cpp -lws2_32 -static-libgcc -static-libstdc++
g++ -std=c++17 -static -Iinclude -o fileclient.exe src/socket.cpp src/thread.cpp src/mutex.cpp src/platform_utils.cpp src/client.cpp -lws2_32 -static-libgcc -static-libstdc++


Running
------------------------
Server:
./fileserver_mt [port] [storage_dir] [max_clients] [password]

./fileserver_mt 8080                           # Default: port 8080, password "admin123"
./fileserver_mt 8080 server_files 10 mysecret  # Custom settings


Client:
./linux_gui_client.exe
./windows_gui_client.exe
