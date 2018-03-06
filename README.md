# Zion Language

[![Build Status](https://travis-ci.org/zionlang/zion.svg?branch=master)](https://travis-ci.org/zionlang/zion)

Zion is a programming language. Zion prefers correctness and refactorability over performance. Zion
targets scenarios where scalability is intended to happen horizontally, not vertically. That being
said, Zion tries to be fast, using static compilation to native host architecture as its execution
model. Zion is built on [LLVM](https://llvm.org/).

## User Roles
 - In industry there are two archetypes of programming language users, Workers and Librarians. Experienced developers wear either of these hats, switching back and forth as necessary. This switching can happen as new dependencies and integrations are initiated and completed within the scope of feature or new product work.
   - *Workers* build trustworthy applications that solve problems. Workers demand a pleasant and ergonomic experience in order to remain focused on reaching their objectives.
   - *Librarians* extend the capabilities of the language by
     - Creating bindings to external libraries,
     - Integrating external libraries so as to appear fluid and seamless to the Workers,
     - Exposing extrinsic data via serialization that maintains type-safety and ergonomics to the Workers.

## Goals

 - Keep it simple, when possible.
 - Keep it readable, when possible.
 - Make algebraic data types compelling for state modeling.
 - Reduce pain near process boundaries. Be opinionated on how to make serialization as comfortable as possible while
   retaining static type safety.

## Non-goals
 - Solving heavy compute problems is a non-goal. Solve those problems at a lower level,
   and use the FFI to access those components. Favor algorithmic scaling over bit twiddling and fretting over L1 cache hits.
 - Pause-free execution remains a back-burner goal. (ie: Enabling Game loops, high-speed trading platforms, life support monitoring, embedded systems, etc...) However, in a future version, once language development settles down a bit, this will get more attention.
 - In-language concurrency and asynchronicity will get treatment in the future. These can currently be solved at other levels of
   the stack (hint: use OS processes.) This is not because it's not important. But, basic ergonomics of the language come first.

## Syntax

Zion looks a bit like Python, minus the colons.

```
module hello_world

def main()
	print("Hello, world!")
```

Comments use `#`.
```
def favorite_number(x int) bool
	# This is a comment
	return x == 12
```

Zion is strict by default, not lazy.  Memory is managed using garbage collection.

### README TODO
- [ ] discuss namespaces.
- [ ] discuss future plans for safe dynamic dispatch. ([some thoughts exist here](https://gist.github.com/wbbradley/86cd672651cf129a91d14586523e979f))
- [ ] discuss the std library layout.


Types are declared as follows:

```
# Declare a structure type (aka product type, aka "struct")
type Vector2D has
	var x float
	var y float

# Note the use of the word "has" after the type name. This signifies that the
# Giraffe type "has" the succeeding "dimensions" of data associated with its
# instances.
type Giraffe has
	var name str
	var age int
	var number_of_spots int

type Gender is Male or Female

type Lion has
	var name str
	var age int
	var gender Gender

type Mouse has
	var fur_color str

# tags are how you declare a global singleton enum values.

tag Zion
tag Yellowstone
tag Yosemite

type NationalPark is
	Zion or
	Yellowstone or
	Yosemite

type Bison has
	var favorite_national_park NationalPark

# Types are not limited to being included in only one sum type, they can be
# included as subtypes of multiple supertypes. Note that Mouse is a possible
# substitutable type for either AfricanAnimal, or NorthAmericanAnimal.

type AfricanAnimal is
	Lion or
	Giraffe or
	Mouse

type NorthAmericanAnimal is
	Mouse or
	Bison
```
etc...

Some examples of standard types are:
```
type bool is
	true or
	false

# The squiggly braces are "type variables", they are not bound to the type until
# an instance of the type is created by calling its implicit constructor. If the
# "has" type contains 2 dimensions, then the generated constructor takes 2
# parameters. The question-mark indicates that the preceding type is a "maybe"
# type. This means that it can sometimes be `null`. Zion will try its best to not
# let you dereference `null` pointers.

```

The bool and vector types are declared in the standard library exactly as depicted
above. See `lib/std.zion`.

When a call to a function that takes a sum type and there still remain free type
variables, they will be substituted with the "unreachable" type (void) during
function instantiation.

### Getting LLVM built on your Mac

```
./llvm-build.sh
```
Then add $HOME/opt/llvm/release_40/MinSizeRel/bin to your path. Be sure to add it before any existing versions of clang
or llvm tools, etc...

### TODO

- [ ] Check for duplicate bound function instantiations deeper within function instantiation
- [ ] Change := to be let by default
- [ ] Consider uniform calling syntax for .-chaining and how to acheive customizable monadic behaviors by looking at the receiver
- [ ] Consider allowing overloads for arbitrary `tk_operator`s to enable overloading random symbols.
- [ ] Consider implementing a macro specifier for declaring macros which would expand inline and have hygenic names
- [ ] Implement slice array indexing rg[s:end], etc...
- [ ] Implement vector slicing for strings and arrays
- [ ] Implement native structures as non-pointer values
- [ ] Explore closures with capture by value
- [ ] Optimize `scope_t`'s `get_nominal_env` and `get_total_env` to be cached
- [ ] Add safety checks on casting (as)
- [ ] Implement generic sort
- [ ] Explore using a conservative collector
- [ ] Consider marking null-terminated strings differently for FFI purposes
- [ ] Builtin data structures
  - [x] string (as slices)
  - [x] vectors
  - [ ] hash map
  - [ ] binary tree
  - [ ] avl tree
  - [ ] link defs are not yet functional
- [ ] decide on `with` (Python) / `using`(`dispose`) (C#) / 'defer' (Golang) style syntax for deterministic destruction
- [ ] Use DIBuilder to add line-level debugging information
- [ ] Rework debug logging to filter based on taglevels, rather than just one global level (to enable debugging particular parts more specifically)
- [ ] enable linking to variadic functions (like printf)
- [x] (un)signed integers
  - [x] write a C integer promotion compatibility test as part of test framework
  - [x] integers as a type with parameterized number of bits and whether to use
    sign-extend or zero-extend
  - [x] promotions upon binary operators
  - [x] prevent overloading integer operations unless one side is not an integer
  - [x] deal with casting integers
- [x] implement `let` declarations
- [x] change `str` to use `wchar_t` as its underlying physical character type
  - [x] use C99's `mbstowcs` and `wcstombs` to convert back and forth
  - [x] propagate usage of utf8 for `char`
- [x] 'for' syntax - based on `tests/test_list_iter.zion` pattern
- [x] Ternary operator
- [x] Logical and/or (build with ternary operator)
- [x] Type refinements for ternary / conditional expressions
- [x] Implement vector literal checking and code gen
- [x] Design/Implement tags functionality (for integration with ctags and LSP)
