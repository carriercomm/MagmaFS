![MagmaFS](logo.png)          
MagmaFS distributed file system
========================================================================

Magma is a **distributed network filesystem** for Linux. By distributed we mean that 
**Magma spreads data across a cluster** (called a *lava network*) of nodes (called *volcanos*).
Each object managed by Magma (being it a file, a directory, a symlink, ...) is called *flare*.
Each volcano holds a *slice of the SHA1 keyspace*. When a flare is stored inside Magma, its 
path is hashed to produce an SHA1 key. The flare will be hosted by the volcano that holds 
the slice of the SHA1 keyspace the key belongs to.

This design has been conceived to *overcome the well known problems bound to the presence of
single point of failure roles* like master nodes, name nodes and so on.

**Magma is currently an experimental project**. It's able to form a network of two or possibly
more nodes and to redundantly copy each operation to the next node (on a two volcanos scenario
this is basically equivalent to a network RAID1). Magma has a partially implemented load
balancer that will move keys across nodes to equalize the pressure on the whole network.

A Magma filesystem can be mounted on Linux using the provided `mount.magma` command. It requires
*FUSE support* in the kernel.

Magma uses UDP datagrams for its Flare (data) and Node (metadata) protocols, respectively defaulting
on ports 12000 and 12001. On port 12002 it features a TCP console which can be reached using `telnet`.

A Magma network can be already built, shutdown and re-established, but **no extensive testing has
been done on its reliability**.

## How to compile

Magma is divided in four packages:

 * libmagma
 * magmad
 * mount.magma
 * magma_tools
 
*libmagma* is the foundation library used by the other three packages and must be compiled first.
*magmad* is the Magma daemon. A lava network is formed by several *magmad*. *mount.magma* runs on each client and mounts the lava network on a local directory. 

Magma uses the usual autotools setup:

    $ ./autogen.sh
    $ ./configure
    $ make
    $ sudo make install

## How to build a network

A lava network is built by starting the first *magmad* with the `-b` option. For example this command:

    $ magmad -n wallace -s wallace.intranet -k m4gm4s3cr3t -i 192.168.1.20 -b -d /srv/magma -Dfs

starts a lava network (`-b`) on the host called *wallace* (fully qualified name: *wallace.intranet*) setting secret key *m4gm4s3cr3t* and binding to local address *192.168.1.20*, storing data in `/srv/magma` and enabling logging for the flare operations (`-Df`) and the SQL queries used to store metadata (`-Ds`).

## How to expand a network

A lava network can be expanded by starting more nodes in *join mode* with option `-r` followed by the IP address of the node to be contacted for the join operation. For example:

    $ magmad -n gromit -s gromit.intranet -k m4gm4s3cr3t -i 192.168.1.21 -d /srv/magma -r 192.168.1.20 -DV

`magmad` is started on a node called *gromit* on IP address *192.168.1.21* and it's instructed to contact node at address *192.168.1.20* (the previously started *wallace* node) to join the network after it. The remote node will commit to the joining node the key slice delimited by the key following its last used key and its last assigned key.

For example, being wallace the only node of the network, its key slice is the whole key space, from `0000000000000000000000000000000000000000` to `ffffffffffffffffffffffffffffffffffffffff`. It's last saved key is `8363a82ab9833cd88d90382b3984e834a83v23ab` while its last assigned key is obviously `ffffffffffffffffffffffffffffffffffffffff`. Thus *wallace* (the active node) entrusts to *gromit* (the joining node) the key slice between `8363a82ab9833cd88d90382b3984e834a83v23ac` (the key after its last saved key) and `ffffffffffffffffffffffffffffffffffffffff`. The magma ring topology would then be:

| lava topology   | wallace                                    | gromit                                     |
| --------------- | ------------------------------------------ | ------------------------------------------ |
| address         | *192.168.1.20*                             | *192.168.1.21*                             |
| key slice start | `0000000000000000000000000000000000000000` | `8363a82ab9833cd88d90382b3984e834a83v23ac` |
| key slice end   | `8363a82ab9833cd88d90382b3984e834a83v23ab` | `ffffffffffffffffffffffffffffffffffffffff` |

Joining nodes receive a copy of all the keys managed by the joined node and start acting as its redundant mirror. When a third node named *penguin* joins the network after *gromit*, the redundancy topology becomes:

| keys of  | are replicated on |
| -------- | ------------------|
| wallace  | gromit            |
| gromit   | penguin           |
| penguin  | wallace           |

## How to shutdown a network

Using `telnet`, connect to `magmad` console port and issue a `shutdown`:

    $ telnet 192.168.1.20 12002
    Trying 192.168.1.20...
    Connected to 192.168.1.20.
    Escape character is '^]'.

    Welcome to MAGMA version 0.1.20150213
    This is server wallace (192.168.1.20)
    Type help [ENTER] for available commands.

    MAGMA [wallace]:/> shutdown
    Shutting down magma... Connection closed by foreign host.
    $

## How to restart a previously shutdown network

To restart a network just run each node without any `-b` or `-r` option. Lava nodes will reload their previous configuration from disk and will wait for the whole network to be available. The node that holds the key of a special internal directory called `/.dht/` will act as coordinator.

## How to mount a Lava network

`mount.magma` is the client side tool that mounts a Lava network on a local directory. The syntax is:

    $ mount.magma --host=192.168.1.20 -s --key=m4gm4s3cr3t ~/magma

The mountpoint is specified as the last argument. Every volcano can be contacted to mount the lava network.

## Routing and proxying

To access a flare, a client should ideally contact the volcano owning it. However current client implementation just sends each request to the node selected with the `--host` argument during the mount operation. When that node receives a request, it first calculate the SHA1 key for the flare path to route the request. Routing the request means identifying the right volcano the request should be addressed to.

For this reason, `magmad` acts as a transparent proxy for incoming request. If a request must be managed by another node, `magmad` just forwards it and sends the response back to the originating client. When the client implementation will be refined to download the lava topology to address requests to responsible nodes, the proxy feature will be used less frequently, but will however remain crucial to handle a special case. Indeed it could happen that a volcano has commited a key to its sibling node for load balancing purposes and the requesting client still holds the outdated network topology. The proxying feature ensures that the request is fulfiled in any case.

## Balancing

Load balancing across the Lava network is still under development. The fundamental design dictates that a node can commit keys for balancing purposes to its following node only. In the (wallace -> gromit -> penguin) network, wallace can commit keys to gromit and gromit to penguin which can't commit keys to anyone. When a new node is required in the middle of the topology, the system administratror can add it manually or Magma can start a special operation to free its first node from all its keys and then rejoin it where required.

## The console

`magmad` has a `telnet` accessible console on port 12002. Several commands are available:

    $ telnet 192.168.1.20 12002
    Trying 192.168.1.20...
    Connected to 192.168.1.20.
    Escape character is '^]'.

    Welcome to MAGMA version 0.1.20150213
    This is server wallace (192.168.1.20)
    Type help [ENTER] for available commands.

    MAGMA [wallace]:/> help

          cache load: print number of flare actyally cached
           cd <path>: change current directory
          cat <path>: show content of file <path>
          debug on X: enable debug on channel X (see print debug)
         debug off X: enable debug on channel X (see print debug)
        erase <path>: erase flare referred by <path>, only if not a directory
                exit: close current session
                help: print this message
      inspect <path>: show available data on flare <path
          debug on X: enable debug on channel X (see print debug)
                lava: print network topology
           ls <path>: show <path> contents
         print cache: cache state as a tree
         print debug: show current debug mask
           print acl: print established ACL
                 pwd: print working directory
                quit: close current session
            shutdown: shutdown magma server

    MAGMA [wallace]:/> ls /.dht
    
    d---------      tx0      tx0          13 .
    drwxrwxrwx     root     root          10 ..
    ----------     root     root         288 wallace
 
     -- total entries: 3
 
    MAGMA [wallace]:/> cat /.dht/wallace
    hashpath = /tmp/magma
    servername = wallace.myplace.taz
    nickname = wallace
    ipaddr = 192.168.1.20
    port = 12000
    bandwidth = 1
    bootserver = (null)
    secretkey = m4gm4s3cr3t
    load = 0.000000
    start_key = 0000000000000000000000000000000000000000
    stop_key = ffffffffffffffffffffffffffffffffffffffff
 
    MAGMA [wallace]:/>

The `print cache` command shows the flares loaded in the in-memory cache. The `print debug` command lists debug channels enabled for logging. The `inspect path` command prints metadata on a flare, and the `lava` command shows the Lava topology known to this node.
