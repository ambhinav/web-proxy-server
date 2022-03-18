# Code & Design Decisions
This document explains the code logic and outlines the design decisions made for the proxy server.

## Overview
The program is written in C and uses the Berkley Sockets API for accepting/connecting to clients/servers and POSIX threads for multi-threading.

## Proxy server
The server works as a server for incoming connections and a client for the remote servers the clients want to communicate with. It then forwards the client's requests to the servers and forwards the servers' response to the clients and so on.

### Logic
#### Init
The `main` method reads the input agruments (port number, telemetry and blacklist file name). It sets a global var called telemetry based on this value (0 or 1). Then it reads the blacklist file and stores each line (hostname) into a char array. Finally, it creates the proxy server's socket using the port number given by the user and starts accepting client connections.

#### Connection flow
Information about the client and the remote server - such as URI, HTTP version, etc - are parsed and stored into a struct *serverInfo*. The IP of the remote server is obtained using the `gethostbyname` method from the `netdb` API. If the **CONNECT** keyword is seen in the HTTP request then it is passed to the `runSocket` method which will handle **HTTPS** request processing. **HTTP** requests will not have the **CONNECT** keyword and will be handled by the `runSocketHTTP` method.

Since the proxy is handling connections to the client and remote server simultaneously, the `select` method is used to poll the socket file descriptors regularly to check if non-blocking read/write operations can be performed. After checking if the client/server file descriptors are available, it forwards data between them till no more data is read. It then closes both sockets.

### Multi-threading
The proxy uses 8 threads. We decided to use a thread pool approach for multithreading where each client connection is a task. Reasons for using a thread pool include:
- Time: Each connection can last for a variable period of time.
- Thread limit: Threads can be reused (since we are only allowed to use max 8 we cannot create new threads for each connection).
- Priority: The client connections do not have priority.

8 threads are created in the `main` method. A class called `myqueue` has a queue to serve as the thread pool. Each connection context will serve as a node of the queue. When a client connection (task) is established it enqueues the task to the queue and passes the connection context as a struct *serverInfo* to the thread handler `thread_function`. `thread_function` polls the queue continuosuly to check for a connection context. If the context information has the keyword "CONNECT", it is passed to the `runSocket` handler or `runSocketHTTP` otherwise.

Since the enqueueing and dequeueing can lead to race conditions, these operations are guarded by a mutex.

## Telemetry
If the user requires telemetry data, the global var is updated to 1. Inside `runSocket`, when a client connection is established to the server the total number of bytes transferred between them as well as the connection time is recorded and printed out after the connections are closed.

## Blacklist
As mentioned above, the blacklist text file provided by the user is read into memory and stored in a char array. When a client connection is established, the server URI is parsed and checked to see if it is a substring of any of the hostnames in the aforementioned array or vice versa. If any match, the client connection is closed and the proxy is ready to server other clients.

## Difference between HTTP/1.0 and HTTP/1.1
The telemetry results were compared by using the proxy on the same website but with different HTTP versions set.

For HTTP/1.0 more client connections with less bytes transferred are opened. This is because HTTP 1.1 allows for multiple requests and responses in one connection, while HTTP 1.0 closes the connection after each request and response.
