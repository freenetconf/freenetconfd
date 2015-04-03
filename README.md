freenetconfd
============

freenetconfd is a NETCONF server and it enables device provisioning through
NETCONF protocol.

- written in C
- extensible with plugins
- interoperable with Tail-f NCS, OpenDaylight (ODL) and testconf

### dependencies

Install *autoconf*, *cmake*, *git*, *json-c*, *libtool*, *pkg-config* and
*zlib* using your package manager while the following dependancies one will
likely need to install from source:

- [*libubox*](http://git.openwrt.org/?p=project/libubox.git;a=summary)
- [*uci*](http://nbd.name/gitweb.cgi?p=uci.git;a=summary)
- [*libroxml*](http://www.libroxml.net/)
- [*ubus*](http://wiki.openwrt.org/doc/techref/ubus)

In order to have functional NETCONF server a SSH subsystem "translator",
such as [*freesub*](https://github.com/freenetconf/freesub), is needed too.

### building freenetconfd

The build procedure itself is simple:

```
git clone https://github.com/freenetconf/freenetconfd.git
cd freenetconfd/build
cmake .. -DCMAKE_INSTALL_PREFIX:PATH=/usr
make
```

### configuring freenetconfd

The *freenetconfd* configuration can be found in
`/etc/config/freenetconfd`. The defaults can be copied from the source:

```
config freenetconfd
    option addr '127.0.0.1'
    option port '1831'
    option yang_dir '/etc/yang'
    option modules_dir "/usr/lib/freenetconfd/"
```

### running freenetconfd

Before starting *freenetconfd* make sure that *ubusd* is running. On
development machines it can done like this:

```
sudo ubusd &
disown
sudo chown `whoami` /run/ubus.sock
```

When everything is configured *freenetconfd* can be simply started like
this:

```
./bin/freenetconfd
```
