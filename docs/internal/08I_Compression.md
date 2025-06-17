# 08I: Compression of Value and Object Header

Object header compression reduces the size footprint of `Obj` from 16 to 8 bytes. Requires `OBJ_HEADER_COMPRESSION` flag to be enabled.

```
// Bit representation:
// .......M NNNNNNNN NNNNNNNN NNNNNNNN NNNNNNNN NNNNNNNN NNNNNNNN EEEEEEEE
// M: isMarked
// N: next pointer
// E: ObjType enum
```

Value NaN-boxing reduces the size footprint of `Value` from 16 to 8 bytes. Requires `VALUE_NAN_BOXING` flag to be enabled.

```
Bit representation:
Signalling NaN: .1111111 1111.... ........ ........ ........ ........ ........ ........
Quiet NaN:      .1111111 111111.. ........ ........ ........ ........ ........ ........
Empty:          01111111 111111.. ........ ........ ........ ........ ........ ......00
Nil:            01111111 111111.. ........ ........ ........ ........ ........ ......01
False:          01111111 111111.. ........ ........ ........ ........ ........ ......10
True:           01111111 111111.. ........ ........ ........ ........ ........ ......11
Object:         11111111 11111100 NNNNNNNN NNNNNNNN NNNNNNNN NNNNNNNN NNNNNNNN NNNNNNNN 
```

Funnily enough the one problem I encountered was an oversight with the garbage collector that never triggered with the tagged-union representation; I allocated the vm.initString *before* setting any garbage-collection related fields. I assume those defaulted to 0. `vm.initString` caused a garbage collection due to being greater than 0 bytes. The ObjString* is deallocated before being assigned to `vm.initString`. Segfault.