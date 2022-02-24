# Linux UAPI Bindings

This library provides raw bindings to the Linux UAPI generated by Rust's
`bindgen` tool.

Currently, we need to run the `bindgen` tool manually. To add more bindings,
include an additional header in `wrapper` and re-run `bindgen.sh` from the
root of your Fuchsia source tree.

For the `bindgen.sh` script to work, you will need to install `bindgen` and
add it to your path:

```sh
cargo install bindgen
```

Ideally we would be able to run `bindgen` as part of the build to remove this
manual step.

## libc types

Linux kernel headers typically do not rely on libc types. However, if a header
you want to include does, you will need to manually define typedefs in
`stub/typedefs.h`.