The Exciting World of Ledger C
------------------------------

Knowing C will help you in this adventure. But not as much as it should. There are some fun twists when it comes to Ledger C. Explore them below. Memorize them. There *will* be a quiz...

### Exceptions

C doesn't have them. So you don't have to think about bracketing, exception safety, RAII, try/catch, all that.

Well not on the Ledger. You have exceptions! Which means you also have out-of-band code paths, and you now have to worry about exception safety.

You can `THROW` a `uint16_t` like this `THROW(0x9000)`.

Handling exceptions looks like this.

```c
volatile int something = 0;
BEGIN_TRY {
    TRY {
        //do something;
    }
    CATCH(EXC_PARSE_ERROR) {
        //do something on parse error
    }
    CATCH_OTHER(e) {
        THROW(e);
    }
    FINALLY { }
}
END_TRY;
```

Exceptions that make it all the way to the top of the application are caught and returned as status codes from the APDU.

#### Gotchas

  1. If a variable will be accessed both outside and inside the `BEGIN_TRY`/`END_TRY` block it must be `volatile`. The compiler doesn't expect these shenanigans and will optimize incorrectly if you don't.
  2. Do not `return` in the `TRY` block. It will cause the Ledger to crash. Instead use a `volatile` variable to capture the result you want to `return` at the end.
  3. Don't try to leave out blocks like `CATCH_OTHER(e)` and `FINALLY`. I don't know if that will work right and it's not worth the risk.

#### Implications

  1. If you have some global state and an exception is thrown then, unless you do something about it, that global state will remain. That might be a *very bad thing*. As long as you use globals our way (see Globals Our Way) you should be safe.


### Globals Our Way

`static const` globals are fine. `static` non-const are not fine for two reasons:

  1. If you try to initialize them (which you would want to do!) then the app will crash. For example `static int my_bool = 3;` crashes whenever you try to read or write `my_bool`...
  2. Instead of getting initialized to 0 like the C standard says, they are initialized to `0xA5`. Yes this can cause the compiler to incorrectly optimize your code.

So just don't use `static` non-const globals. Instead we have `globals.h` which defines a large `struct` wher you can put your globals. At the beginning of the application we `memset(&global, 0, sizeof(global))` to clear it all to zeros.

Anything inside of `global.apdu` will get cleared when an exception gets to the top of the app (see Exceptions). To benefit from this behavior you should never return an error code via the in-band way of sending bytes back. All errors should be sent via `THROW`.

### Relocation

When we said `static const` globals were fine, we meant that they were possible. There is
a major gotcha, however: if you initialize a `static const` value with a pointer to another
`static` or `static const` value, the pointers might be incorrect and require relocation.

For example:

```
static const char important_string[] = "Important!";
static const char **important_string_ptrs = { important_string, NULL };
const char *str1 = important_string_ptrs[0];
const char *str2 = important_string;
```

`str` will now have the wrong value. `str2` will not. The reason `str1`
has the wrong value is that the linker gets confused with a reference
from one `static const` variable to another `static` variable on this
platform. To resolve, you can use the `PIC` macro, which will fix broken
pointers but never break a good pointer. Because of this, you can use
it liberally and not have to worry about breaking anything:

```
static const char important_string[] = "Important!";
static const char **important_string_ptrs = { important_string, NULL };
const char *str1 = PIC(important_string_ptrs[0]); // necessary use of PIC
const char *str2 = PIC(important_string); // unnecessary but harmless use of PIC
```

Many of the UI functions call `PIC` for you, so just because a UI function
accepts a data structure, doesn't mean that data structure is valid.

### Dynamic Allocation

Nope. Don't even try. No `malloc`/`calloc`/`free`. Use globals (see Globals).
