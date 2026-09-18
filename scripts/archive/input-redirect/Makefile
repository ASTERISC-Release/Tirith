all: libsdlintercept.so server_responder client_app1

libsdlintercept.so:
	gcc -shared -fPIC -o libsdlintercept.so client_stub.c -ldl -lpthread

server_responder:
	gcc -o server_responder server_responder.c -lSDL2 -lX11 -lpthread -lrt

client_app1:
	gcc -o client_app1 application/client_app1.c -lSDL2

clean:
	rm -rf *.so server_responder client_app1