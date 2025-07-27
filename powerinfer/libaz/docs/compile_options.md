## `AZ_ENABLE_ASAN`

Address sanitizer.

## `AZ_ENABLE_UBSAN`

Undefined behavior sanitizer.

## `AZ_ENABLE_LTO`

Link time optimization.

## `AZ_DISABLE_ASSERT`

Value: `ON`/`OFF`

If set to `ON`, all `AZ_ASSERT` will be no operation.

## `AZ_DISABLE_DEBUG_ASSERT`

关闭所有debug assertion，包括边界检查等。打开这些assertion会造成比较大的性能开销。

## `AZ_ENABLE_PERFETTO`

Value: `ON`/`OFF`

Whether enable Perfetto tracing.

Compiling Perfetto library can be time consuming. Therefore it defaults to `OFF`.
