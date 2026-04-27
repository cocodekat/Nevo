# everything must exist within a function, except for defining functions which CANNOT happen within a function

# functions

code starts at **\_main()** this cannot have parameters

to jump to function use **jump \_func()**
pass the paramaters as **variables** or **numbers**

to define a function use **\_func(p1, p2, p3)** <- start with \_

# variables:

define with **num var** = _variable / number / equation_

define scoped with **scoped num var** = _variable / number / equation_
this will only be available inside of the function

to call a scoped variable use **$var**

# printing

to print text use **print("text")**
to print a variable use **print(var)** or **print($var)** for scoped variables

# loops

to loop, use **loop <num / var / equation> {**
`everything in here gets looped`
**}**

# how it works

- step 1:
  advanced nevo gets transpiled to earlier/simpeler nevo lang for the compiler
- step 2:
  the simpeler lang gets transpiled into assembly
- step 3:
  the assembly is compiled into a mach-o executable
