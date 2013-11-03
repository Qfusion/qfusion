/**

\page doc_script_retref Returning a reference


A function is allowed to return a reference, just like any other type, however due to the 
need to guarantee that the reference is valid even after the function returns to the caller
there exist certain restrictions. You don't need to try to remember these restrictions as 
the compiler will give an error if you they are violated, but if you do encounter a compile
error when returning a reference it will be good to understand why it is happening so that
you can determine how to avoid it.

\section doc_script_retref_global References to global variables are allowed

As a global variable is in the global scope, the lifetime of the variable is longer than the
scope of the function. A function can thus return a reference to a global variable, or even member 
of an object reached through a global variable.

\section doc_script_refref_member References to class members are allowed

A class method can return a reference to a class property of the same object, because the caller
is required to hold a reference to the object it is known that the member will exist even after the
method returns. 

The class method is also allowed to return reference to global variables, just like any other function.

\section doc_script_retref_local Can't return reference to local variables

Because local variables must be freed when the function exits, it is not allowed to return
a reference to them. The same is also true for any parameters that the function received. The
parameters are also cleaned up when the function exits, so they too cannot be returned by 
reference.

\section doc_script_retref_deferred Can't use expressions with deferred parameters

For some function calls with arguments, there may a necessary processing of the arguments
after the function call returned, e.g. to clean up the input object, or to assign the output 
parameters. If the function that was called is returning a reference, then that reference cannot
in turn be returned again, as it may be invalidated by the deferred evaluation of the arguments.

\section doc_script_refref_cleanup Can't use expressions that rely on local objects

All local objects must be cleaned up before a function exits. For functions that return references
this clean-up happens before the return expression is evaluated, otherwise the cleanup of the
local objects may accidentally invalidate the reference. For this reason it is not possible
to use expressions that rely on local objects to evaluate the reference that should be returned.

Primitive values can be used though, as these do not require cleanup upon exit.







*/
