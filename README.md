# A Web server implemented fully in C
Handles all IP and TCP header creation and stripping, along with TCP segmentation
for large files

Usage:
```shell
./web <addr> <port> <folder> [-l][-t]
    -l: Enable indepth logging
    -t: Enable logging of current tcp connections

<addr>: The IP address that server will listen on
<port>: The port that the server will be bound too
<folder>: The folder in which the server will look for files to respond to HTTP requests in
```
