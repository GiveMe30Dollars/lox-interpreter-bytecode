# 09I: Long Opcodes

`LONG` opcode variants have been added to the compiler. They are about what you expect to be honest.

## Utility Function(s)

To make my life easier for the VM, I collected all of the variant opcodes in this helper function:

```c
// in chunk.h

static inline bool isLongOpcode(Opcode op){
    switch (op){
        case OP_CONSTANT_LONG:
        case OP_DEFINE_GLOBAL_LONG:
        case OP_GET_GLOBAL_LONG:
        case OP_SET_GLOBAL_LONG:
        case OP_CLOSURE_LONG:
        case OP_CLASS_LONG:
        case OP_GET_PROPERTY_LONG:
        case OP_SET_PROPERTY_LONG:
        case OP_METHOD_LONG:
        case OP_INVOKE_LONG:
        case OP_GET_SUPER_LONG:
        case OP_SUPER_INVOKE_LONG:
            return true;
        default:
            return false;
    }
}
```

*Aside: You would not believe the number of segfaults not keeping this list up-to-date has caused me.*

It will be nice to consolidate this here, now; the only behavioural difference between a normal opcode and its `LONG` variant during runtime is the reading of the constant, so making it easy to determine that is crucial for reusing code.

## Disassembly Support

`constantLongInstruction()` and `invokeLongInstruction()` helper functions were copied over from their normal variants. I could add conditional logic in the normal functions, but the disassembler is a giant switch table anyways, and these are the only two functions we need to add `LONG` support for.

Aside from that, update the disassembler. There's 12 new cases in there now. They're about what you expect.

## Compiler Support

A crucial thing I did when adding the `LONG` variants into the `Opcode` enum is to add them immediately after their normal variants. This is the main reason why.

Keeping these variants in mind when writing code is a slog. The chunk's `addConstant` function (and by extension `identifierConstant` in the compiler) already supports adding in an arbitrary number of constants (well, until your machine runs out of memory anyways), so the only thing we need to edit is the emit function. Ideally, we make it work for any arbitrary opcode that takes in a `cidx` and thus has a `LONG` variant.

`emitConstant()` has been changed to the following:

```c
static void emitConstant(Opcode instruction, uint32_t constIdx){
    if ((constIdx & 0xff) == constIdx){
        emitBytes(instruction, (uint8_t)constIdx);
    } else if ((constIdx & 0xffffff) == constIdx){
        emitByte(instruction + 1);
        emitByte((uint8_t)((constIdx >> 16) & 0xff));
        emitByte((uint8_t)((constIdx >> 8) & 0xff));
        emitByte((uint8_t)(constIdx & 0xff));
    } else {
        error("Number of constants in one chunk cannot exceed 2^24.");
    }
}
```

If the `cidx` (or `constIdx` in this case) can fit in a byte operand, emit the bytecode for the normal opcode. Otherwise, use the `LONG` variant by adding 1. A compile-time check is also added to make sure we don't go over the maximum (though this is.. very unlikely to occur, what with having 16,777,216 total constants available for use).

From here, it's relatively trivial to go through the code and replace any part where we emit a `cidx`-requiring opcode with the new `emitConstant`. 

Just try not to miss any spots. 

I jest, don't worry. An upcoming section will help us test the emitting of `LONG` opcodes more easily.

## VM Support

Since the only difference a normal opcode and its `LONG` variant is how it reads constants, we only really need to substantially change the `READ_CONSTANT()` preprocessor macro and friends:

```c
// in run()
    #define READ_LONG()       (ip += 3, (uint32_t)ip[-3] << 16 | ip[-2] << 8 | ip[-1])
    #define READ_CONSTANT(instruction) \
        (getFrameFunction(frame)->chunk.constants.values[isLongOpcode(instruction) ? READ_LONG() : READ_BYTE()])
```

This is where that `isLongOpcode()` helper function comes into play. This does also mean that when we add a new `LONG` variant opcode, we also have to add it there, otherwise the VM won't know how to read it.

See how `READ_CONSTANT`'s signature changed? That wasn't strictly necessary, but it does make it clear that `READ_CONSTANT`'s behaviour changes depending on the opcode type. It also handily highlights all the places where `READ_CONSTANT` was previously used as errors. In other words, every opcode that now also has a `LONG` variant.

We add the `LONG` variant as a fallthrough case to the original behaviour. And that's it!

## Testing Long Opcode Compilation and Execution

Cramming 256 constants into code is hard. Let's artifically induce that behaviour instead:

In `initChunk`, we detect whether preprocessor flag `CHUNK_TEST_LONG_OPCODES` is defined, and if so:

```c
// in initChunk(), after everything else
    #ifdef CHUNK_TEST_LONG_OPCODES
    ValueArray* array = &chunk->constants;
    array->count = 256;
    array->capacity = 256;
    array->values = GROW_ARRAY(Value, array->values, 0, array->capacity);
    #endif
```

Yep. There's nothing more to it.

Now anything you put into the constants array is bound to overflow a single byte operand, and result in the emitting of a `LONG` opcode.

## Compiler Bloat

I didn't consider this when I made this. All I knew was that this was something that Nystrom (rightfully) identified as a limitation of the implementation, to be addressed in Challenge 14-2. I'm pretty sure Wren and Magpie (Nystrom's other scripting languages, intended for real actual use) had to deal with this problem too.

But consider that the VM went from 38 opcodes to 50. That's nearly a 30% increase, *just* from this. We haven't even addressed local variables yet!

The question boils down to: what is Lox being used for?

And so far, for me? Toy languages and baby's first compiler/interpreter.

I don't write this to disparage my efforts with extending Lox. I just think that it would be better spent on new features like collection types or Exceptions or sentinel classes. This is something you do *after* the language starts seeing use. Or when I start writing the STL. Whichever comes first, I suppose.

Splintered to branch `long-opcodes`.