# I Love Cookies

Signal & CV Sequencer.  Uses Text as input.  Because after all, don't we all love cookies?

Knobs are labeled with lowercase letters: a-z.  Inputs are labeled with uppercase letters A-Z.  Programming in the sequence: `abcd` will sequentially output the values of knobs a, b, c, and finally d.  It will then loop back to step 1: knob a again.  An exact voltage can be programmed by enclosing the value in square brackets.  For example: `<4.20>`.  Following a sequence of values (lowercase, uppercase, or exact value) with "@8" will loop the sequence after 8 steps.  8 is not a special value, any number is allowed after the "@" symbol.  Here is an example with some I Love Cookies inputs their equivalents:

~~~~
Input           Equivalent Input

a@3             aaa
ab@5            ababa
<1><0>@8        <1><0><1><0><1><0><1><0>
abcde@8         abcdeabc
abcdef@3        abc
~~~~

## Randomization

Enclosing values (lowercase letter, uppercase letter, or exact voltage) in curly braces {} will randomly select one of the values with equal probability.  For example, `{ab}` will choose either `a` or `b` at each clock step.  `{g<2.55>}` will output either the value of knob `g` or `2.55` volts with equal probability.  `{}` will choose one of the 26 knobs `a` thru `z`.

## Square Bracket Expansion

Enclosing comma-separated sequences with square brackets allows for even more complex patterns to be generated.

~~~~
Input           Equivalent Input          Comment

[ab,c]@4        ababcccc                  4 steps of "ab", then 4 steps of "c"
[A,cde]@5       AAAAAcdecd                5 steps from input "A", then 5 steps of "cde"
~~~~


=======

~~~~
Input           Equivalent Input

a@3             aaa
ab@5            ababa
<1><0>@8        <1><0><1><0><1><0><1><0>
abcde@8         abcdeabc
abcdef@3        abc
~~~~


All of the following are valid I Love Cookies programs:
~~~~
<4.20>
{abc}
ab(cd)
def@10
[abc,de]@6
{}@8,arphald
~~~~


~~~~
 ┭ ۳┭┭  ۳۳┭┭ ┭  ┭  ┭┭┭  ┭۳┭۳۳┭
۳  ۳┭ ۳۳۳┭۳  ┭  ┭  ┭ ┭۳  ┭   
┭┭   ┭۳ ┭ ┭ ┭ ┭┭   ۳۳۳┭۳ ۳┭┭  ۳ ┭۳ ۳

 ┭ ۳┭┭  ۳۳┭┭ ┭  ┭  ┭┭┭  ┭۳┭۳۳┭
۳  ۳┭ ۳۳۳┭۳  ┭  ┭  ┭ ┭۳  ┭   
┭┭   ┭۳ ┭ ┭ ┭ ┭┭   ۳۳۳┭۳ ۳┭┭  ۳ ┭۳ ۳

 ┭ ۳┭┭  ۳۳┭┭ ┭  ┭  ┭┭┭  ┭۳┭۳۳┭
۳  ۳┭ ۳۳۳┭۳  ┭  ┭  ┭ ┭۳  ┭   
┭┭   ┭۳ ┭ ┭ ┭ ┭┭   ۳۳۳┭۳ ۳┭┭  ۳ ┭۳ ۳
~~~~

