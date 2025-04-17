"# process-manager" 
mingw64 
 gcc -o http-server path/to/file/demo.c -I/path/to/mingw64/include -L/path/to/mingw64/lib -lmicrohttpd -lws2_32 -lpsapi
 ./http-server.exe
