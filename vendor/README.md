# Vendored dependencies

This directory contains copies of dependencies that we want to build into the
PHP agent.

## Adding a dependency

1. Ensure that the dependency is licensed in a way that allows us to use it.
2. Clone the dependency into a directory under this path.
3. Remove `.git`, if necessary.
4. Add rules to [`vendor/Makefile`](Makefile) to build and clean the
   dependency.
5. Update [`make/vendor.mk`](../make/vendor.mk) to provide whatever variables
   are necessary for downstream libraries to include and link against the
   dependency.
6. Update whatever other component(s) need to include the dependency.
7. Add a note to this README about the dependency you added.
8. Update [`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md) to include
   licensing information for the dependency.

Note that you will almost certainly want to build any vendored libraries as
static libraries to avoid deployment issues where specific dynamic libraries
are required on customer systems. You can verify this by building the PHP agent
on Linux, then running `ldd` as follows: `ldd agent/modules/newrelic.so`.

You should see output like the following:

```
	linux-vdso.so.1 (0x00007ffcf65c4000)
	libm.so.6 => /usr/lib/libm.so.6 (0x00007ff08667f000)
	libdl.so.2 => /usr/lib/libdl.so.2 (0x00007ff08667a000)
	librt.so.1 => /usr/lib/librt.so.1 (0x00007ff08666f000)
	libpcre.so.1 => /usr/lib/libpcre.so.1 (0x00007ff0865fd000)
	libpthread.so.0 => /usr/lib/libpthread.so.0 (0x00007ff0865db000)
	libc.so.6 => /usr/lib/libc.so.6 (0x00007ff086415000)
	/usr/lib64/ld-linux-x86-64.so.2 (0x00007ff0868cb000)
```

If you see anything _other_ than these libraries, you should investigate.

## Existing dependencies

### `protobuf`

Version 3.11.4 of [the C++ protobuf
code](https://github.com/protocolbuffers/protobuf), as cloned from the repo.

Two directories have been removed from the checkout to avoid build issues:

* `benchmarks/go`: this was incorrectly picked up by Go 1.8 as a test suite
  that should be run when running the daemon tests. Moving to Go modules will
  fix this, but in the interim, the directory can be removed without affecting
  the protobuf build.
* `examples`: similarly to the above, this would be built as part of the daemon
  rules in the Makefile, and migrating to Go modules will fix this.

### `protobuf-c`

Version 1.3.3 of the [C protobuf
bindings](https://github.com/protobuf-c/protobuf-c).

Note that protobuf-c requires protobuf to build, but it is _not_ required at
runtime; we can link against just `libprotobuf-c.a`.
