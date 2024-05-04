# Chatroom

### Build
```sh
make
```

### Usage
1. Run the server:

```
./server [PORT]
```

Default port listening is `13000`. We will use for explanation purposes.

2. Use `telnet` to connect:

```
telnet localhost 13000
```

1. Set name

```
Trying 127.0.0.1...
Connected to localhost.
Escape character is '^]'.
John
```

The server will record this event:

```
Client joined the chat: John
```

4. Send messages 

Now that you have appropriately entered the name, you can send messages via Telnet

5. Connecting another client

Open another terminal and run steps 2 to 4.
Upon connecting, the first client should get notified about this event.

```
Trying 127.0.0.1...
Connected to localhost.
Escape character is '^]'.
John

=== Jane has joined the chat ===
```

6. Disconnect client

To close telnet connection, run `Ctrl + ]`. This will open Telnet's command prompt. Type `quit`.
```
telnet> quit
```

The event will get logged in the server and a message will be broadcasted to other clients.

```
Trying 127.0.0.1...
Connected to localhost.
Escape character is '^]'.
Jane

=== John has left the chat ===
```