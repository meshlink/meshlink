# Configuration files

There currently are three different types of configuration files in MeshLink:

- the main configuration file
- the host config files
- pending invitation files

All configuration files are in PackMessage format. The contents will be
described below.

There are three different ways envisioned to store configuration data:

- unencrypted files
- encrypted files
- ephemeral, in-memory storage

To keep things as simple and flexible as possible, there are some restrictions:

- Configuration files are read from and written to memory in one go.
  When a configuration files is changed, it is not modified in place,
  but has to be written from scratch.
- When in-memory, functions from `packmsg.h` can be used to parse and generate the configuration data.
- Configuration files are read and written only via functions declared in `conf.h`.
  This also includes creating and destroying the configuration directory.

## Main configuration file

When stored on disk, it's in `meshlink.conf`. The contents are:

- uint32: configuration format version
- str: name of the local node
- bin: private Ed25519 key
- bin: private Ed25519 key for invitations
- uint16: port number of the local node

More information about the local node is stored in its host config file.

## Host configuration files

When stored on disk, there is one file per node in the mesh, inside the directory `hosts/`.
The contents of a host configuration are:

- uint32: configuration format version
- str: name of the node
- int32: device class
- bool: blacklisted
- bin: public Ed25519 key
- str: canonical address (may be zero length if unset)
- arr[ext]: recent addresses

## Invitation files

When stored on disk, there is one file per pending invitation, inside the directory `invitations/`.
The contents of an invitation file are:

- uint32: invitation format version
- str: name of the invitee
- int32: device class of the invitee (may be unused)
- arr[bin]: one or more host config files

## Encryption

When encryption is enabled, each file is individually encrypted using Chacha20-Poly1305.
A unique counter stored at the start of the file. A (master) key must be provided by the application.

## Exporting configuration

Calling `meshlink_export()` will return an array of host config files:

- arr[bin]: one or more host config files

## Loading, saving and unloading configuration data to/from memory

### Main configuration file

Created during first setup.
Loaded at start.
Never unloaded.
Might have to be saved again when port number or private key for invitations changes.

### Host configuration files

Can be loaded partially into memory:
- devclass+blacklist status only, always required in memory, so loaded in `load_all_nodes()`.
- public key, needed whenever SPTPS is required, or when the application wants a fingerprint or verify signed data from that node.
- canonical and recent addresses, needed whenever we want to make outgoing connections to this node.

Furthermore, in order to properly merge new data, we need to load the whole config file into memory when:
- updating recent addresses
- exporting this node's information (no API for this yet)

Whenever a node's information is updated, we mark it dirty. It is written out at a convenient time.


