# Accept/reject tests for the compile-time LL(1) parser.
#
# Each regex is written into its own tiny translation unit and checked with
# `<compiler> -fsyntax-only` as a separate ctest entry. That way a case that
# gives the wrong result *or* fails to compile at all only fails that one test,
# instead of breaking the build of everything else.
#
# Nothing here is built by `cmake --build`; the compiler runs when ctest runs,
# against the current headers. Run just these with:
#   ctest --test-dir <build> -L ct_parser --output-on-failure
# or one category with e.g. -L ct_escape, or by name with -R.
#
# Regexes are passed as bracket arguments ([=[ ... ]=]) so CMake never
# interprets backslashes, parentheses or semicolons in them.

set(CT_PARSE_DIR ${CMAKE_CURRENT_BINARY_DIR}/ct_parse)
file(MAKE_DIRECTORY ${CT_PARSE_DIR})

set(CT_PARSE_FLAGS
  -std=c++${CMAKE_CXX_STANDARD}
  -fsyntax-only
  -Wall -Wextra -Wpedantic -Werror
  -I${PROJECT_SOURCE_DIR}/include
)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  list(APPEND CT_PARSE_FLAGS -fmax-errors=1)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  list(APPEND CT_PARSE_FLAGS -ferror-limit=1)
endif()

set(CT_PARSE_INDEX 0)

# ct_parse_case(<accept|reject> <category> <regex>)
function(ct_parse_case expect category regex)
  math(EXPR idx "${CT_PARSE_INDEX} + 1")
  set(CT_PARSE_INDEX ${idx} PARENT_SCOPE)
  string(LENGTH "${idx}" len)
  if(len EQUAL 1)
    set(idx "00${idx}")
  elseif(len EQUAL 2)
    set(idx "0${idx}")
  endif()

  set(CT_REGEX "${regex}")
  set(CT_EXPECT "${expect}")
  set(CT_CATEGORY "${category}")
  if(expect STREQUAL "reject")
    set(CT_EXPECT_REJECT 1)
  else()
    set(CT_EXPECT_REJECT 0)
  endif()

  set(file ${CT_PARSE_DIR}/case_${idx}.cpp)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ct_parse_case.cpp.in ${file} @ONLY)

  # Readable test name; keep control characters out of it.
  string(REPLACE "\t" "\\t" shown "${regex}")
  if(shown STREQUAL "")
    set(shown "<empty>")
  endif()
  string(LENGTH "${shown}" shown_len)
  if(shown_len GREATER 40)
    string(SUBSTRING "${shown}" 0 37 shown)
    set(shown "${shown}...")
  endif()

  set(name "CT parse ${idx} ${expect} ${category}: ${shown}")
  add_test(NAME "${name}"
    COMMAND ${CMAKE_CXX_COMPILER} ${CT_PARSE_FLAGS} ${file})
  set_tests_properties("${name}" PROPERTIES
    LABELS "ct_parser;ct_${expect};ct_${category}")
endfunction()

# ---------------------------------------------------------------------------
# Accepted
# ---------------------------------------------------------------------------

foreach(re a z A 0 9 _ - . ^ $ { } , /)
  ct_parse_case(accept literal "${re}")
endforeach()
ct_parse_case(accept literal [=[ ]=])
ct_parse_case(accept literal "\t")
ct_parse_case(accept literal [=[[]=])
ct_parse_case(accept literal [=[]]=])

ct_parse_case(accept sequence [=[ab]=])
ct_parse_case(accept sequence [=[abc]=])
ct_parse_case(accept sequence [=[hello]=])
ct_parse_case(accept sequence [=[hello world]=])
ct_parse_case(accept sequence [=[a.b]=])
ct_parse_case(accept sequence [=[...]=])
ct_parse_case(accept sequence [=[a-z]=])
ct_parse_case(accept sequence [=[[abc]]=])
ct_parse_case(accept sequence [=[{1,2}]=])

ct_parse_case(accept alt [=[a|b]=])
ct_parse_case(accept alt [=[a|b|c]=])
ct_parse_case(accept alt [=[a|b|c|d|e]=])
ct_parse_case(accept alt [=[ab|cd]=])
ct_parse_case(accept alt [=[ab|cd|ef]=])
ct_parse_case(accept alt [=[a|bc]=])
ct_parse_case(accept alt [=[abc|d]=])
ct_parse_case(accept alt [=[.|.]=])

ct_parse_case(accept group [=[(a)]=])
ct_parse_case(accept group [=[((a))]=])
ct_parse_case(accept group [=[(((a)))]=])
ct_parse_case(accept group [=[(ab)]=])
ct_parse_case(accept group [=[(a)(b)]=])
ct_parse_case(accept group [=[(a)(b)(c)]=])
ct_parse_case(accept group [=[a(b)c]=])
ct_parse_case(accept group [=[(a)b]=])
ct_parse_case(accept group [=[a(b)]=])
ct_parse_case(accept group [=[x(y(z))]=])

ct_parse_case(accept group_alt [=[(a|b)]=])
ct_parse_case(accept group_alt [=[(a|b|c)]=])
ct_parse_case(accept group_alt [=[(a|b)|c]=])
ct_parse_case(accept group_alt [=[a|(b|c)]=])
ct_parse_case(accept group_alt [=[(a|b)(c|d)]=])
ct_parse_case(accept group_alt [=[((a|b)c|d)e]=])
ct_parse_case(accept group_alt [=[x(a|b)y]=])
ct_parse_case(accept group_alt [=[((a|b)|(c|d))]=])

ct_parse_case(accept rep [=[a*]=])
ct_parse_case(accept rep [=[a+]=])
ct_parse_case(accept rep [=[a?]=])
ct_parse_case(accept rep [=[ab*]=])
ct_parse_case(accept rep [=[a*b]=])
ct_parse_case(accept rep [=[a*b*]=])
ct_parse_case(accept rep [=[a?b?c?]=])
ct_parse_case(accept rep [=[a+b+c+]=])
ct_parse_case(accept rep [=[xa*y]=])
ct_parse_case(accept rep [=[.*]=])
ct_parse_case(accept rep [=[a.*b]=])

ct_parse_case(accept rep_group [=[(a)*]=])
ct_parse_case(accept rep_group [=[(ab)*]=])
ct_parse_case(accept rep_group [=[(ab)+]=])
ct_parse_case(accept rep_group [=[(ab)?]=])
ct_parse_case(accept rep_group [=[(a|b)*]=])
ct_parse_case(accept rep_group [=[(a|b)+]=])
ct_parse_case(accept rep_group [=[(a*)*]=])
ct_parse_case(accept rep_group [=[((a)+)?]=])
ct_parse_case(accept rep_group [=[(a)*b]=])
ct_parse_case(accept rep_group [=[a(b)*]=])
ct_parse_case(accept rep_group [=[(a)*(b)+]=])

ct_parse_case(accept rep_alt [=[a*|b]=])
ct_parse_case(accept rep_alt [=[a|b*]=])
ct_parse_case(accept rep_alt [=[a*|b+]=])
ct_parse_case(accept rep_alt [=[a?|b?|c?]=])
ct_parse_case(accept rep_alt [=[ab*|cd+]=])

ct_parse_case(accept rep_close [=[(a*)]=])
ct_parse_case(accept rep_close [=[(a+)]=])
ct_parse_case(accept rep_close [=[(a?)]=])
ct_parse_case(accept rep_close [=[(ab*)]=])
ct_parse_case(accept rep_close [=[(a|b*)]=])
ct_parse_case(accept rep_close [=[(a*|b)]=])
ct_parse_case(accept rep_close [=[((a*))]=])
ct_parse_case(accept rep_close [=[(a*)b]=])
ct_parse_case(accept rep_close [=[x(a+)y]=])

ct_parse_case(accept escape [=[\*]=])
ct_parse_case(accept escape [=[\+]=])
ct_parse_case(accept escape [=[\?]=])
ct_parse_case(accept escape [=[\(]=])
ct_parse_case(accept escape [=[\)]=])
ct_parse_case(accept escape [=[\\]=])
ct_parse_case(accept escape [=[\|]=])
ct_parse_case(accept escape [=[\a]=])
ct_parse_case(accept escape [=[\.]=])
ct_parse_case(accept escape [=[\n]=])
ct_parse_case(accept escape [=[\0]=])
ct_parse_case(accept escape [=[\ ]=])
ct_parse_case(accept escape [=[\[]=])

ct_parse_case(accept escape_context [=[\(hello]=])
ct_parse_case(accept escape_context [=[hello\)]=])
ct_parse_case(accept escape_context [=[\(\)]=])
ct_parse_case(accept escape_context [=[a\|b]=])
ct_parse_case(accept escape_context [=[a\*b]=])
ct_parse_case(accept escape_context [=[\\\\]=])
ct_parse_case(accept escape_context [=[\a\b\c]=])
ct_parse_case(accept escape_context [=[a|\|]=])
ct_parse_case(accept escape_context [=[\||a]=])
ct_parse_case(accept escape_context [=[(\()]=])
ct_parse_case(accept escape_context [=[(\))]=])
ct_parse_case(accept escape_context [=[(\|)]=])
ct_parse_case(accept escape_context [=[(a\)b)]=])
ct_parse_case(accept escape_context [=[(\\)]=])
ct_parse_case(accept escape_context [=[(\a|\b)]=])

ct_parse_case(accept escape_rep [=[\**]=])
ct_parse_case(accept escape_rep [=[\++]=])
ct_parse_case(accept escape_rep [=[\??]=])
ct_parse_case(accept escape_rep [=[\(*]=])
ct_parse_case(accept escape_rep [=[\)+]=])
ct_parse_case(accept escape_rep [=[\\*]=])
ct_parse_case(accept escape_rep [=[\|?]=])
ct_parse_case(accept escape_rep [=[\a*]=])
ct_parse_case(accept escape_rep [=[(\a*)]=])
ct_parse_case(accept escape_rep [=[(\)*)]=])
ct_parse_case(accept escape_rep [=[\a*|b]=])

ct_parse_case(accept composite [=[(a|b)*abb]=])
ct_parse_case(accept composite [=[a(b|c)*d]=])
ct_parse_case(accept composite [=[((ab)*|c+)?d]=])
ct_parse_case(accept composite [=[(a(b(c)*)+)?]=])
ct_parse_case(accept composite [=[x*(y|z)+w?]=])
ct_parse_case(accept composite [=[(\(|\))*]=])
ct_parse_case(accept composite [=[(0|1(01*0)*1)*]=])
ct_parse_case(accept composite [=[ab|c(d|e)*f?]=])

# Growing sizes, to see where instantiation depth or compile time gives out.
foreach(n 8 32 128)
  string(REPEAT "a" ${n} re)
  ct_parse_case(accept stress "${re}")

  string(REPEAT "|a" ${n} re)
  ct_parse_case(accept stress "a${re}")

  string(REPEAT "a*" ${n} re)
  ct_parse_case(accept stress "${re}")
endforeach()
foreach(n 4 16 64)
  string(REPEAT "(" ${n} open)
  string(REPEAT ")" ${n} close)
  ct_parse_case(accept stress "${open}a${close}")
endforeach()

# ---------------------------------------------------------------------------
# Rejected
# ---------------------------------------------------------------------------

ct_parse_case(reject empty "")

ct_parse_case(reject unbalanced [=[(]=])
ct_parse_case(reject unbalanced [=[)]=])
ct_parse_case(reject unbalanced [=[(a]=])
ct_parse_case(reject unbalanced [=[a)]=])
ct_parse_case(reject unbalanced [=[((a)]=])
ct_parse_case(reject unbalanced [=[(a))]=])
ct_parse_case(reject unbalanced [=[)(]=])
ct_parse_case(reject unbalanced [=[)a(]=])
ct_parse_case(reject unbalanced [=[a)(b]=])
ct_parse_case(reject unbalanced [=[(a|b]=])
ct_parse_case(reject unbalanced [=[a|b)]=])
ct_parse_case(reject unbalanced [=[(a)b)]=])
ct_parse_case(reject unbalanced [=[((a)|b]=])
ct_parse_case(reject unbalanced [=[hello(]=])

ct_parse_case(reject empty_group [=[()]=])
ct_parse_case(reject empty_group [=[(())]=])
ct_parse_case(reject empty_group [=[a()]=])
ct_parse_case(reject empty_group [=[()a]=])
ct_parse_case(reject empty_group [=[a()b]=])
ct_parse_case(reject empty_group [=[()*]=])
ct_parse_case(reject empty_group [=[(()a)]=])

ct_parse_case(reject empty_alt [=[|]=])
ct_parse_case(reject empty_alt [=[||]=])
ct_parse_case(reject empty_alt [=[a|]=])
ct_parse_case(reject empty_alt [=[|a]=])
ct_parse_case(reject empty_alt [=[a||b]=])
ct_parse_case(reject empty_alt [=[a|b|]=])
ct_parse_case(reject empty_alt [=[|a|b]=])
ct_parse_case(reject empty_alt [=[(|a)]=])
ct_parse_case(reject empty_alt [=[(a|)]=])
ct_parse_case(reject empty_alt [=[(|)]=])
ct_parse_case(reject empty_alt [=[(a||b)]=])
ct_parse_case(reject empty_alt [=[a|()]=])
ct_parse_case(reject empty_alt [=[(a|)b]=])

ct_parse_case(reject no_operand [=[*]=])
ct_parse_case(reject no_operand [=[+]=])
ct_parse_case(reject no_operand [=[?]=])
ct_parse_case(reject no_operand [=[*a]=])
ct_parse_case(reject no_operand [=[+a]=])
ct_parse_case(reject no_operand [=[?a]=])
ct_parse_case(reject no_operand [=[(*)]=])
ct_parse_case(reject no_operand [=[(*a)]=])
ct_parse_case(reject no_operand [=[(+a)]=])
ct_parse_case(reject no_operand [=[a|*b]=])
ct_parse_case(reject no_operand [=[a|+]=])
ct_parse_case(reject no_operand [=[|*]=])
ct_parse_case(reject no_operand [=[(a|*)]=])
ct_parse_case(reject no_operand [=[a(*b)]=])

ct_parse_case(reject stacked_rep [=[a**]=])
ct_parse_case(reject stacked_rep [=[a++]=])
ct_parse_case(reject stacked_rep [=[a??]=])
ct_parse_case(reject stacked_rep [=[a*+]=])
ct_parse_case(reject stacked_rep [=[a+*]=])
ct_parse_case(reject stacked_rep [=[a*?]=])
ct_parse_case(reject stacked_rep [=[a+?]=])
ct_parse_case(reject stacked_rep [=[a?*]=])
ct_parse_case(reject stacked_rep [=[(a)**]=])
ct_parse_case(reject stacked_rep [=[(a)*+]=])
ct_parse_case(reject stacked_rep [=[a***]=])
ct_parse_case(reject stacked_rep [=[\a**]=])
ct_parse_case(reject stacked_rep [=[(a**)]=])

ct_parse_case(reject dangling_escape [=[\]=])
ct_parse_case(reject dangling_escape [=[a\]=])
ct_parse_case(reject dangling_escape [=[\\\]=])
ct_parse_case(reject dangling_escape [=[(\]=])
ct_parse_case(reject dangling_escape [=[(a\]=])
ct_parse_case(reject dangling_escape [=[a|\]=])

ct_parse_case(reject trailing_garbage [=[a)b]=])
ct_parse_case(reject trailing_garbage [=[(a))b]=])
ct_parse_case(reject trailing_garbage [=[ab)]=])
ct_parse_case(reject trailing_garbage [=[a|b)c]=])
ct_parse_case(reject trailing_garbage [=[(a)*)]=])
