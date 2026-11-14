"""LLDB pretty-printers for toyctre's parse-tree types.

Loaded via the repo's .lldbinit:  command script import tools/lldb/toyctre_formatters.py

Displays values the way you'd write them as aggregate initializers, e.g.
    Repetition{{}, 'x', false, true}
and makes std::variant<toyctre::...> show the active member directly.
"""

import lldb

MAX_ELEMS = 16  # elements shown per vector before eliding with "..."
MAX_LEN = 400   # overall summary length cap


# ---------------------------------------------------------------- helpers ---

def _raw(v):
    return v.GetNonSyntheticValue() if v.IsSynthetic() else v


def _vector_elems(vec):
    """Elements of a libstdc++ std::vector, read straight from _M_impl."""
    vec = _raw(vec)
    impl = vec.GetChildMemberWithName("_M_impl")
    start = impl.GetChildMemberWithName("_M_start")
    finish = impl.GetChildMemberWithName("_M_finish")
    if not start.IsValid() or not finish.IsValid():
        return [], 0
    elem_t = start.GetType().GetPointeeType()
    size = elem_t.GetByteSize()
    if size == 0:
        return [], 0
    begin, end = start.GetValueAsUnsigned(), finish.GetValueAsUnsigned()
    n = (end - begin) // size if end >= begin else 0
    out = [
        vec.CreateValueFromAddress("[%d]" % i, begin + i * size, elem_t)
        for i in range(min(n, MAX_ELEMS))
    ]
    return out, n


def _char(v):
    c = v.GetValueAsUnsigned() & 0xFF
    if c == 0:
        return "0"
    if 32 <= c < 127 and c not in (ord("'"), ord("\\")):
        return "'%s'" % chr(c)
    return {ord("'"): r"'\''", ord("\\"): r"'\\'", 10: r"'\n'", 9: r"'\t'"}.get(
        c, "'\\x%02x'" % c
    )


def _bool(v):
    return "true" if v.GetValueAsUnsigned() else "false"


def _list(vec, fmt, depth):
    elems, n = _vector_elems(vec)
    if n == 0:
        return "{}"
    parts = [fmt(e, depth + 1) for e in elems]
    if n > len(elems):
        parts.append("...(%d total)" % n)
    return "{" + ", ".join(parts) + "}"


# ------------------------------------------------------------ type bodies ---
# Each fmt_* returns the full "Type{...}" text; depth guards runaway recursion.

def fmt_repetition(v, depth=0):
    v = _raw(v)
    alt = v.GetChildMemberWithName("altenative")  # (sic) matches lib.hpp
    alt_s = _alt_body(alt, depth)
    fields = [
        alt_s,
        _char(v.GetChildMemberWithName("literal")),
        _bool(v.GetChildMemberWithName("isNested")),
        _bool(v.GetChildMemberWithName("wasEscaped")),
    ]
    rep = v.GetChildMemberWithName("rep")
    if rep.GetValueAsUnsigned() & 0xFF:
        fields.append(_char(rep))
    return "Repetition{" + ", ".join(fields) + "}"


def _alt_body(v, depth):
    """Alternative as it would appear nested: {} when empty."""
    v = _raw(v)
    if depth > 8:
        return "{...}"
    cats = v.GetChildMemberWithName("concatenations")
    _, n = _vector_elems(cats)
    if n == 0:
        return "{}"
    return fmt_alternative(v, depth)


def fmt_alternative(v, depth=0):
    v = _raw(v)
    if depth > 8:
        return "Alternative{...}"
    return "Alternative{" + _list(
        v.GetChildMemberWithName("concatenations"), fmt_concatenation, depth
    ) + "}"


def fmt_concatenation(v, depth=0):
    v = _raw(v)
    if depth > 8:
        return "Concatenation{...}"
    return "Concatenation{" + _list(
        v.GetChildMemberWithName("repetitions"), fmt_repetition, depth
    ) + "}"


def fmt_regex(v, depth=0):
    v = _raw(v)
    return "Regex{" + fmt_alternative(v.GetChildMemberWithName("alternative"), depth + 1) + "}"


def _cap(s):
    return s if len(s) <= MAX_LEN else s[: MAX_LEN - 3] + "..."


def repetition_summary(v, _d):
    return _cap(fmt_repetition(v))


def alternative_summary(v, _d):
    return _cap(fmt_alternative(v))


def concatenation_summary(v, _d):
    return _cap(fmt_concatenation(v))


def regex_summary(v, _d):
    return _cap(fmt_regex(v))


# ---------------------------------------------------------- std::variant ---

def _variant_active(v):
    """Return the active member of a libstdc++ std::variant, or None."""
    v = _raw(v)
    idx = v.GetChildMemberWithName("_M_index")
    storage = v.GetChildMemberWithName("_M_u")
    if not idx.IsValid() or not storage.IsValid():
        return None
    i = idx.GetValueAsUnsigned()
    t = v.GetType().GetCanonicalType()
    if i >= t.GetNumberOfTemplateArguments():
        return None  # valueless_by_exception
    alt_t = t.GetTemplateArgumentType(i)
    # Every union member lives at the same address as _M_u.
    return v.CreateValueFromAddress("value", storage.GetLoadAddress(), alt_t)


def variant_summary(v, _d):
    a = _variant_active(v)
    if a is None:
        return "valueless"
    s = a.GetSummary()
    return s if s else a.GetTypeName()


class VariantChildren:
    """Expand a variant straight into the active member's fields."""

    def __init__(self, v, _d):
        self.v = v
        self.active = None

    def update(self):
        self.active = _variant_active(self.v)
        return False

    def num_children(self):
        return self.active.GetNumChildren() if self.active else 0

    def get_child_index(self, name):
        return self.active.GetIndexOfChildWithName(name) if self.active else -1

    def get_child_at_index(self, i):
        return self.active.GetChildAtIndex(i) if self.active else None

    def has_children(self):
        return True


# ------------------------------------------------------------- register ---

def __lldb_init_module(debugger, _d):
    mod = __name__
    run = debugger.HandleCommand
    cat = "toyctre"
    for t, fn in [
        ("toyctre::Repetition", "repetition_summary"),
        ("toyctre::Alternative", "alternative_summary"),
        ("toyctre::Concatenation", "concatenation_summary"),
        ("toyctre::Regex", "regex_summary"),
    ]:
        run('type summary add -w %s -F %s.%s "%s"' % (cat, mod, fn, t))
    var_re = r"^std::variant<toyctre::.*>$"
    run("type summary add -w %s -F %s.variant_summary -x '%s'" % (cat, mod, var_re))
    run("type synthetic add -w %s -l %s.VariantChildren -x '%s'" % (cat, mod, var_re))
    run("type category enable %s" % cat)
