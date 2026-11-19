## Starting Point

The initial ct matcher takes a parameter pack representing remaining nodes to match and has a very concise implementation (templating condensed for space, star normalization elided)
```

auto match<> (auto text) {
  return text.empty();
}

auto match<Literal<e, c>, Rs...> (auto text) {
  return (!text.empty() && text.front == c && match<Rs...>(text.substr(1)));
}

auto match<Sequence<Ras...>, Rbs...> (auto text) {
  return match<Ras..., Rbs...>(text);
}

auto match<Alternatives<Ras...>, Rbs...> (auto text) {
  return (match<Ras, Rbs...>(text) || ...);
}

auto match<Group<R>, Rs...> (auto text) {
  return match<R, Rs...>(text);
}

auto match<Repetition<R, '?'>, Rs...> (auto text) {
  return match<R, Rs...>(text) || match<Rs...>(text);
}

auto match<Repetition<R, '*'>, Rs...> (auto text) {
  return match<R, Repetition<R, '*'>, Rs...>(text) || match<Rs...>(text);
}

auto match<Repetition<R, '+'>, Rs...> (auto text) {
  return match<R, Repetition<R, '*'>, Rs...>(text);
}
```

## Capture Groups

Match needs to record where groups start and end somehow.  The actual value recorded can just be `text.size()` and the wrapping `match<RegexString>()` can subtract from the original length to get the index.

### Submatches

`match<Group<R>, Rs...>` could call `match<R>`, record the length, then call `match<Rs...>` on the remainder.  This would require matching to convert to partial matching.  This is likely the approach I'll use for atomic groups and possessive quantifiers, but for regular groups it has the problem of backtracking.  If the subsequent matching is done in the parent `match`, then the child needs to provide a way to iterate through its own possible matches.  Easier would be to pass the continuation to the child, either as a lambda or as a parameter.

This is precisely the design question in the original setup, and I selected type lists as the method of continuation passing over lambdas because chaining is just pack concatenation.  I could do `submatch<First<Rs...>, Then<Ss...>`, but then I'd need to duplicate all of the above.  Easier to just stick with a flat `Match<Rs...>` and mark the end of the group.

This leads to (group number elided, can just be a NTTP on `Group`)

```
auto match<Group<R>, Rs...>(auto text) {
  recordGroupStart();
  return match<R, EndGroup, Rs...>(text);
}

auto match<EndGroup, Rs...>(auto text) {
  recordGroupEnd();
  return match<Rs...>(text);
}
```

### Recording

Group start and end points need to live somewhere, and have to behave well with backtracking (we can't just use the last seen values, `(a)b|ac` matched on `ac` will first successfully match the `(a)` and record it, then backtrack when failing to match `b`, and successfully match the whole string, while holding a last recorded match of group 1 that is invalid).

The first option is using value semantics, and pass groups through all the match functions and return them.  Backtracking is not an issue, because failed paths don't affect the groups at the backtrack points.  The cost is a copy on every call (todo: when the compiler inlines everything, are all the unnecessary copies elided?) and verbosity carrying extra parameters that are only needed in two specializations.

The second option is using reference semantics and having all the `match` functions record locations in a shared data structure.  Now we need to deal with backtracking, but that can be confined to the `Group` and `EndGroup` specializations which could just be

```
auto match<Group<R>, Rs...>(auto text) {
  if(!match<R, EndGroup, Rs...>(text)) return false;
  recordGroupStart();
  return true;
}

auto match<EndGroup, Rs...>(auto text) {
  if(!match<Rs...>(text)) return false;
  recordGroupEnd();
  return true;
}
```

This would record the first successful group in a repetition, which is not standard behavior.  It can record the last by instead recording the original value and replacing, calling the next `match`, and resetting if it failed.

#### `||` shortcircuiting for value semantics

The value semantics version needs to change the return type, which breaks the `||` chaining in `Alternatives` and `Repetition`.

The return type could overload the `||` operator, but only the builtin gets short circuiting, so doing so without other changes would cause the matcher to find every possible match, not just the first - a potentially exponential pessimization.

The `||` could be replaced by a manual shortcircuit, breaking the fold in `match<Alternatives>` and requiring a recursive approach, or `||` could produce a lazy data structure, e.g. it returns a function that takes the `text` and tries each in turn.  Much like the submatch design, the latter would be making a new type that does what `match<Alternatives>` already is doing.  I'd expect that the recursive approach has no runtime cost as the compiler will inline it all the same as the current fold, but this hasn't been tested.

A better alternative is to use the builtin `||` and abuse assignment being an expression evaluating to the assigned value.  Then the result just needs to be convertible to bool and we keep shortcircuiting.

```
struct MatchResult {
  bool success{};
  T captureData;

  operator bool() { return success; }
};

auto match<Alternatives<Ras...>, Rbs...>(auto text) {
  auto b = MatchResult{};
  ((b = match<Ras, Rbs...>(text)) || ...);
  return b;
}
```

#### Nonstatic data for reference semantics

For the reference semantics version, the shared structure could be a class member and remove the need to add it to the signature.  Using the current partial specialization approach, it doesn't save much typing.  The actual shape is

```
struct Matcher<R, Rs...> {
  static auto match(auto text) {
    <specific R action>;
    Matcher<Rs...>::match(text);
  }
};
```

This is required since functions can't be partially specialized.  Turning them into member functions looks about the same as explicitly passing the capture data, since every specialization of match is in its own type.

```
struct MatcherMember<R, Rs...> {
  T* captureData;
  auto match(auto text) {
    <specific R action>
    MatcherMember<Rs...>{captureData}.match(text);
  }
};

struct MatcherStatic<R, Rs...> {
  auto match(auto text, T* captureData) {
    <specific R action>
    MatcherStatic<Rs...>::match(text, captureData);
  }
};
```

Another option is to use overloading to put all the match functions into one class, then `captureData` does not need to be passed.  The parameter pack becomes the type of an argument and deduction + overloading replaces partial specialization.

```
struct Matcher {
  T captureData;

  auto match(auto text, List<Group<R>, Rs...>) {
    if(!match(text, List<R, EndGroup, Rs...>{})) return false;
    recordGroupStart();
    return true;
  }

  auto match(auto text, List<EndGroup, Rs...>) {
    if(!match(text, List<Rs...>{})) return false;
    recordGroupEnd();
    return true;
  }
};
```

The cost of this is at compile time.  In other tests, the overload resolution was more expensive than partial specialization, but this specific change hasn't been tested.

None of the options are clear winners, I've opted for the explicit passing with reference semantics solely for stylistic reasons.  TODO: try the others and measure performance.

## Repetitions

The backtracking matcher needs a way to deal with repetition on expressions that can match empty (such an expression is called nullable).  Eg `(a?)*`, without modification, will overflow the stack matching nothing on `b` - `a?` matches the empty string, and it can do so arbitrarily many times.  The first approach I used to solve this was to replace the expression with a non-nullable expression that matches the same words when repeated, eg `(a)*` matches the same words as `(a?)*`.

### Star normalization

The desired properties are `!nullable(star_normal(E))` and `(E)* = (star_normal(E))*` where equality here means matching the same words.  Both nullable and star_normal are defined structurally (lowercase letters indicate literal characters, upper case indicate expressions)

```
nullable(c) = false;
nullable(E*) = true;
nullable(E?) = true;

nullable(E+) = nullable(E)
nullable((E)) = nullable(E)

nullable(EF) = nullable(E) && nullable(F)
nullable(E|F) = nullable(E) || nullable(F)

star_normal(E) = E                                  if !nullable(E)
star_normal((E)) = (star_normal(E))
star_normal(EF) = star_normal(E) | star_normal(F)   if nullable(EF)
star_normal(E|F) = star_normal(E) | star_normal(F)
star_normal(E*) = star_normal(E)
star_normal(E+) = star_noraml(E)
star_normal(E?) = star_normal(E)
```

Unfortunately, the sequence case breaks capture groups. `((a?)b?)*` and `((a)|b)*` both match any word consisting solely of `a` and `b`, but when matching `b`, the former captures an empty string at the start, while the latter captures nothing.

### Progress tracking

Instead of changing the regex, the matcher can record where it is in the text each time it enters a repetition and reject if it hasn't consumed at least one character.

Note this does still affect captures, it rejects an empty capture if there are any nonempty ones, but this is probably desired (TODO: Explicitly specify which capture is selected in repetitions).  E.g., `(a?)*` matching `a` could say this is `n` matches of `epsilon`, `1` match of `a`, then `m` matches of epsilon.  If, say, we record the last capture, such an interpretation would capture an empty string after the `a`, while the progress tracking version always captures `a`.  It also rejects an empty capture when there are no non-empty captures, and this probably should be addressed (TODO).

Each repetition could get its own slot, akin to `captureData`.  Store and restore on failure is still needed for nested repetitions.  But if the store and restore is happening, only one slot is needed provided the repetitions don't interleave.  Sequential and alternative repetitions can't interleave, each case is fully processed before moving to the next in sequence or the next alternative.  Nested repetitions do interleave though, with the inner repetition beginning again after the outer repetition repeats the first time.  However, this restarting doesn't need to be progressed from the previous - the outer repetition consumed a character else it wouldn't repeat, so the match is progressing.  So we only need to check that progress has been made the second (and more) time we enter the repetition.

```
auto match<Repetition<E>, Rs...>(auto text) {
  recordProgress();
  auto rest = match<E, ProgressedRepetition<E>, Rs>(text);
  if(rest) return true;
  resetProgress();
  return match<Rs...>(text);
}

auto match<ProgressedRepetition<E>, Rs...>(auto text) {
  return checkProgress() && match<Repeption<E>, Rs...>(test);  
}
```
