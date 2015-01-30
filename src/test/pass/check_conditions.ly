var ok = 0
var a = 10

# Lily has two kind of if statements: single-line, and multi-line.
# A single-line statement only allows one line before needing either a
# close or an elif/else.

if a == 10:
    a = 11
elif a == 11:
    ok = 0
else:
    ok = 0

# Multi-line ifs begin by putting a { after the : of the if.
# Unlike most curly brace languages, the } is only required
# at the end of the if.
if a == 11: {
    # Multi-line ifs can contain single-line ifs.
    a = 12
    # This is a single-line if. It closes after 'ok = 0'
    if a == 12:
        a = 13
    else:
        ok = 0
    ok = 1
# Lily deduces that this else is multi-line, because it hasn't found
# a } for the {.
else:
    # Lily deduces that this else is multi-line
    ok = 0
    ok = 0
    ok = 0
}

# Now, if with some different things used for the condition.
if 1.1 != 1.1:
    ok = 0

if "1" != "1":
    ok = 0

if ok != 1:
    print("Failed.\n")
