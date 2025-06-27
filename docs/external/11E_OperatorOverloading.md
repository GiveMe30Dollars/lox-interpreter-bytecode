# 11E: Operator Overloading

The following operators can be overloaded by defining a corresponding method in the class description.


| Operator | Method to Implement | Return Value        |
| -------- | ------------------- |---------------------|
| `+` `+=` | `add(other)`        | New result instance |
| `-` `-=` | `subtract(other)`   | New result instance |
| `*` `*=` | `multiply(other)`   | New result instance |
| `/` `/=` | `divide(other)`     | New result instance |
| `[]` (get) | `get(idx)`        | Value of the get operation |
| `[]` (set) | `set(idx, value)` | `value` |

