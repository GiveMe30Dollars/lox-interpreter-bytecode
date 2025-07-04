# 09R: Sulfox

With the completion of base Lox, it feels inappropriate to continue to refer to this project as Lox as I continue implementing features such as collection types (arrays, hashmaps), the STL and miscellanious ends and bits.

Henceforth, the term "Sulfox" to refer to features that require modification of parsing, object runtime representation or the VM to such an extent that it can no longer be viewed as a simple extension to Lox as described in [Crafting Interpreters](https://www.craftinginterpreters.com).

This mostly includes features in the experimental branch `containers`. This may include the liberal use of native functions and sentinel classes, as some of those (notably, `Array.@raw()` and the inaccessible `@raw` methods of other object types) basically end up functioning as new opcodes during compilation.

Sulfox is a portemanteau of Lox and sulfur, which is both an element that exists as a crystalline yellow solid under room conditions, and namesake of the [sulphur butterflies](https://en.wikipedia.org/wiki/Phoebis_sennae).