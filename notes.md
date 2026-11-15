## Document purpose

This aim of this project is to learn metaprogramming.  This document is to record concepts as I understand them.

## Basics

### Types as values

The main idea is to treat types as data.  Instead of functions that take a value and return a value, we write metafunctions that take a type and return a type.

```
int square(int x) { return x*x; }

// types can hold values in template parameters
template<int x> struct Int { static constexpr int value = x; };

template<typename T> struct Square { using type = Int<T::value * T::value>; };

static_assert(std::is_same_v<Square<Int<5>>::type, Int<25>>);
```
[Compiler Explorer](https://godbolt.org/z/oPKKGGaoM)

Using `using type = ReturnType` is standard.  

Typechecking is minimal.  The above allows the instantiation of `Square<int>`, which will lead to the error `error: 'value' is not a member of 'int'`.  This happens when `Square<int>` is actually instantiated, not when it is named, so the compiler output can be very long, listing the whole chain of instantiations leading to it.

Partial specialization can let us extract template arguments, and prevent instantiation of invalid arguments, though the error is still at instantiation time.  This will give an error `invalid use of incomplete type` if we don't provide a default, or we can customize the error message by having a default with a failing `static_assert`.  Note that the static_assert method will still allow the type to be created, so the compilation will continue and produce more errors when `Square<int>` has no member `type`.  Concepts & requires clauses can also help with error messages, but can also increase error output when multiple partial specializations would match without the requires.


When calling other metafunctions, eg `template<typename T> struct Quartic { using type = typename Square<typename Square<T>::type>::type };` The compiler can't know that `Square<T>::type` is a type until it instantiates `Square<T>`, but in the definition of Quartic, `T` is a template and doesn't yet have a value.  So `typename` is required.  Convention is to make an alias `template<typename T> using square_t = typename Square<T>::type;` As of C++20, the `typename` is not required in `using <alias> = typename <dependent type>`, but is still required within `<dependent type>`.  I.e. we can remove the first `typename` in `Quartic`, but not the second.

```
template<int x> struct Int { static constexpr int value = x; };

template<typename> struct Square {static_assert(false, "Called Square with non Int<x>"); };
template<int x> struct Square<Int<x>> { using type = Int<x*x>; };
template<typename T> using square_t = typename Square<T>::type;

template<typename, typename> struct Product;
template<int x, int y> struct Product<Int<x>, Int<y>> { using type = Int<x*y>; };
template<typename T, typename U> using product_t = typename Product<T, U>::type;

template<typename T, typename U> struct ProductOfSquares { using type = product_t<square_t<T>, square_t<U>>; };
template<typename T, typename U> using product_of_squares_t = typename ProductOfSquares<T, U>::type;

static_assert(std::is_same_v<product_of_squares_t<Int<5>, Int<2>>, Int<100>>);
// static_assert(std::is_same_v<product_of_squares_t<Int<5>, int>, Int<100>>); // uncomment to see errors
```
[Compiler Explorer](https://godbolt.org/z/G5oKqojGn)

### Flow control

Partial specialization can be used for if/switch.  There is no looping, so recursion is used instead.

```
template<int x> struct Int { static constexpr int value = x; };

template<typename, typename> struct Product;
template<typename T, typename U> using product_t = typename Product<T,U>::type;
template<int x, int y> struct Product<Int<x>, Int<y>> { using type = Int<x*y>; };

template<typename> struct Factorial;
template<typename T> using factorial_t = typename Factorial<T>::type;

template<> struct Factorial<Int<0>> { using type = Int<1>; };
template<int x> struct Factorial<Int<x>> { using type = product_t<Int<x>, factorial_t<Int<x-1>>>; };

static_assert(std::is_same_v<factorial_t<Int<4>>, Int<24>>);
```
[Compiler Explorer](https://godbolt.org/z/TY71Wqfcv)

### Lazy evaluation

Naming an incomplete type or one that has a failing `static_assert` does not cause an error unless it is instantiated.  This can be useful for lazy evaluation.  Types are only instantiated if we use it in some way, eg accessing a static member, calling a static member function, or constructing it.  Just using a type as a type parameter does not instantiate.  `std::conditional<bool c typename T, typename F>` has `type = T` if `c`, and `type = F` otherwise. The unselected type is not instantiated.  But the expression for `T` may cause an instantiation of an undesired type if care is not taken. 

```
template<int x> struct Int { static constexpr int value = x; };

template<typename> struct Square;
template<int x> struct Square<Int<x>> { using type = Int<x*x>; };
template<typename T> using square_t = typename Square<T>::type;

struct NotAnInt{};

template<typename> inline constexpr auto is_int_v = false;
template<int x> inline constexpr auto is_int_v<Int<x>> = true;

// We put square_t<T> in the conditional, forcing Square<T>'s instantiation
template<typename T> struct Eager {
    using type = std::conditional_t<is_int_v<T>, square_t<T>, NotAnInt>;
};
template<typename T> using eager_t = typename Eager<T>::type;

static_assert(std::is_same_v<eager_t<Int<2>>, Int<4>>);
// static_assert(std::is_same_v<eager_t<int>, NotAnInt>);
// error: invalid use of incomplete type 'struct Square<int>'

// std::type_identity<T> has type = T
// We don't call ::type on Square<T>, instead calling it on the result of the conditional
// So Square<T> only gets instantiated if the conditional is true
template<typename T> struct Lazy {
    using type = typename std::conditional_t<is_int_v<T>, Square<T>, std::type_identity<NotAnInt>>::type;
};
template<typename T> using lazy_t = typename Lazy<T>::type;

static_assert(std::is_same_v<lazy_t<Int<2>>, Int<4>>);
static_assert(std::is_same_v<lazy_t<int>, NotAnInt>);
```
[Compiler Explorer](https://godbolt.org/z/fGc1ennb4)

### Aliases and values

A helper metafunction with no specializations and whose body is just the result type can just be an alias instead.  This can save compile time if it's called frequently with unique types. Aliases aren't cached so this can be a pessimization if the metafunction is frequently called with the same argument.

```
#include<type_traits>

template<int x> struct Int { static constexpr int value = x; };

template<typename, typename> struct Product;
template<int x, int y> struct Product<Int<x>, Int<y>> { using type = Int<x*y>; };
template<typename T, typename U> using product_t = typename Product<T, U>::type;

template<typename T> struct Square { using type = product_t<T,T>; };
template<typename T> using square_t = typename Square<T>::type;
static_assert(std::is_same_v<square_t<Int<5>>, Int<25>>);

template<typename T> using square_alias_t = product_t<T,T>;
static_assert(std::is_same_v<square_alias_t<Int<5>>, Int<25>>);
```
[Compiler Explorer](https://godbolt.org/z/hajWEqjzM)

Similarly, if we want to return a (non-type) value, we could do `template<typename> struct ValueReturningMetafunction { static constexpr auto value = <value>; };`, but we can also just do `template<typename> inline constexpr auto value = <value>;`.  (See `is_int_v` in the Lazy evaluation example) Unlike aliases which can't be specialized, template variables can be, so complicated metafunctions can be created this way.  For leaf metafunctions where a value is always defined and always needed eagerly, the variable form is convenient.  The struct form supports lazy evaluation, SFINAE, and use with higher order metafunctions.  `if constexpr` and `requires` can remove the need for the former two.

## Parameter packs

List types are easily represented using parameter packs `template <typename...Ts> struct List;`.  Options for getting the `Ts`:

Partial specialization
```
template<typename...Ts> struct List;

template<typename, typename> struct Push;
template<typename L, typename T> using push_t = typename Push<L,T>::type;
template<typename...Ts, typename T> struct Push<List<Ts...>, T> { using type = List<T, Ts...>; };

static_assert(std::is_same_v<push_t<List<int, float>, double>, List<double, int, float>>);
```
[Compiler Explorer](https://godbolt.org/z/TbGY1djvK)

Within the list type
```
template<typename...Ts> struct List { template<typename T> using push_t = List<T, Ts...>; };

template<typename L, typename T> struct Push { using type = typename L::push_t<T>; };
template<typename L, typename T> using push_t = typename Push<L,T>::type;

static_assert(std::is_same_v<push_t<List<int, float>, double>, List<double, int, float>>);
```
[Compiler Explorer](https://godbolt.org/z/8qsbqzeod)

As the return type of a templated function
```
template<typename...Ts> struct List{};

template<typename L, typename T> struct Push { 
    template<typename...Ts> static List<T, Ts...> f(List<Ts...>);
    using type = decltype(f(std::declval<L>()));
 };
template<typename L, typename T> using push_t = typename Push<L,T>::type;

static_assert(std::is_same_v<push_t<List<int, float>, double>, List<double, int, float>>);
```
[Compiler Explorer](https://godbolt.org/z/dKn6d6Ejc)

Note that the first does not require instantiating the type while the other two do.
