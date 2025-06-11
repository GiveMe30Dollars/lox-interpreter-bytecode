# 03E: Multiline Comments

Multiline comments are begin with `/*` and terminate with `*/`. Any characters within the multiline comment are escaped.

Multiline comments can also nest:

```
// Commenting out code for a quick while...
/*
print "..." ;    /*Nested comment*/
*/
```

Note that due to this behaviour, any `/*` or `*/` characters within strings have to be accounted for when commenting out existing code:

```
print "This /* is in a string. Nothing happens.";
// Begin nesting...
/*
print "This /* is not in a string and acts as a nested comment.";
*/
Only one pair has been closed.
You are now in the twilight zone.
```

Note that unpaired `/*` or `*/` symbols are considered compile errors.