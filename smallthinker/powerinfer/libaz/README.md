A collection of low-level operators.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --config RelWithDebInfo -j4
```

## Testing

```bash
cmake --build build --config RelWithDebInfo --target az-test-main -j4
./az-test-main
```
