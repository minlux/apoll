# apoll
A C++ webserver supporting long polling.

This webserver is based on a super-loop application with non-blocking sockets.

The server can handle static content, as every other webserver. In addition it can
handle so called "dynamic content". Thats content that is pushed the server via POST
requests and pulled from the server using GET request. Dynamic content is identified by
a "HASH value". This hash value is exchanged using a custom HTTP header "Content-Hash".
When a client requests a dynamic resource it may set that custom HTTP header. If it
doesn't, the request will be served immediatelly(*) and the server will reply the
content of the resource. The server will also send the HASH value of the content using
HTTP header "Content-Hash". This HASH value can then be used for subsequent (long polling)
requests by the client with the header "Content-Hash" set!

So if the client request a dynamic resource with HTTP header "Content-Hash" set to
a value X, the server compares X to the HASH value of the requested resource.
If the HASH values are different, the request will be served immediatelly, the server
will reply the content and the (new) HASH of the resource.
However if the HASH values are equal, the server will not reply. Instead, it will leave
the connection open, until the content of the requested resource was changed by other
POST requests served in the meanwile. As far as the content of the dynamic resource has
changed (and thereby its HASH) the server will will reply the (updated) content and the
(new) HASH of the resource.

---

(*) Only for non empty dynamic resources. Request to empty dynamic resources are also deferred!


## How to build
Build process is based on CMake.
 
1. Create a build directory (e.g. `mkdir build`)
2. Within the build directory execute `cmake ..`
3. Within the build directory execute `make`


## Usage (on command line)
`apoll [HTML-base-path] [TCP-port-number]`

- HTML-base-path:
  Absolute or relative path to the base folder that shall be served by apoll.
  E.g. '/home/users/webmaster/www' or '~/www' etc. Default is '.'
  Within that folder, there may be a file called 'dynres.txt' that contains the definition
  of all the available, dynamic resources specified line-by-line. One line specifies
  the URI of the dynamic resource, e.g. '/getTemperature' or '/api/service/xy'

- TPC-port-number:
  The TCP port number apoll shall listen to. E.g. 8080. Default is 8083.


## Example
Create a file `dynres.txt` within your "HTML-base-path" (in this example it will be `.`).
Add line `/bullet-hole` to that file and start "apoll" like this `apoll . 8083`.

### Push dynamic content
`curl -X POST -d '{"x":12.3,"y":4.5}' http://localhost:8083/bullet-hole`

Apoll will reply `OK`. If the dynamic resource doesn't exists, it reply `Not Found`.

### Long polling of dynamic content
At the very first beginning we have no content yet. So we can omit the additional "Content-Hash" header
or (like in this example) we can set the initial "Content-Hash" value to 0.

`curl -i -H 'Content-Hash: 0' http://localhost:8083/bullet-hole`

The server will reply immediatle:

```
HTTP/1.1 200 OK
Content-Type: application/x-www-form-urlencoded
Content-Hash: 3138453061
Content-Length: 18

{"x":12.3,"y":4.5}
```

A subsequent request of the same resource, with "Content-Hash" value as received in the response ... 

`curl -i -H 'Content-Hash: 3138453061' http://localhost:8083/bullet-hole`

...will stay pending, until the resouce on the server changes.
In this example this can be "simulated" by pushing new content to that resource...

`curl -X POST -d '{"x":6.7,"y":8.90}' http://localhost:8083/bullet-hole`

After new data was pushed to the dynamic resource, apoll will reply to all pending requests.

```
HTTP/1.1 200 OK
Content-Type: application/x-www-form-urlencoded
Content-Hash: 3627687525
Content-Length: 18

{"x":6.7,"y":8.90}
```


