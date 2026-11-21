# ADR-0004 sources

Read 2026-11-21.

## Arrow and Iceberg

- Arrow format 1.5 specifies validity bitmaps, fixed-width primitives, and variable-size UTF-8 with signed 32-bit offsets. It prefers 64-byte buffer alignment and padding and describes the large UTF-8 and view layouts. [Arrow Columnar Format](https://arrow.apache.org/docs/format/Columnar.html#physical-memory-layout)
- C Data defines array offsets, zero-size buffer pointers, and producer ownership through `private_data` and release callbacks. Its release rules place no thread restriction on the callback. A consumer can move a parent or move children out and immediately release the parent. [Arrow C Data buffers](https://arrow.apache.org/docs/format/CDataInterface.html#c.ArrowArray.buffers), [C Data memory management](https://arrow.apache.org/docs/format/CDataInterface.html#memory-management)
- C Stream gives results from `get_schema` and `get_next` independent lifetimes. Consumers serialize calls to `get_next` across threads. [Result lifetimes](https://arrow.apache.org/docs/format/CStreamInterface.html#result-lifetimes), [Thread safety](https://arrow.apache.org/docs/format/CStreamInterface.html#thread-safety)
- The upstream Apache-2.0 header contains the ABI declarations approved for verbatim placement in `column` by ADR-0002. [Arrow `abi.h` at 26.0.1](https://github.com/apache/arrow/blob/apache-arrow-26.0.1/cpp/src/arrow/c/abi.h)
- Iceberg projects file columns by field ID because column names and order can change. [Iceberg specification, Column Projection](https://iceberg.apache.org/spec/#column-projection)

## C++ library

- The C++23 proposal defines the value-or-error contract for `std::expected`, including the `void` specialization and `std::unexpected`. libc++ lists the proposal as complete from version 16. [WG21 P0323R12](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p0323r12.html), [libc++ C++23 status](https://libcxx.llvm.org/Status/Cxx23.html)
- The C++ allocation specification defines alignment-aware allocation, a non-throwing overload that can return null, and matching aligned deallocation. These overloads date to C++17. [C++ draft, single-object allocation and deallocation](https://eel.is/c++draft/new.delete.single), [cppreference allocation](https://en.cppreference.com/cpp/memory/new/operator_new), [cppreference deallocation](https://en.cppreference.com/cpp/memory/new/operator_delete)
- Allocating raw storage with `operator new` can implicitly create objects of implicit-lifetime types. [C++ draft, object model](https://eel.is/c++draft/intro.object)
