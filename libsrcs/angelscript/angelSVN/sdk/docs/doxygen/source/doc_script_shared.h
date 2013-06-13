/**


\page doc_script_shared Shared script entities

If the application uses multiple script modules to control different parts 
of the application, it may sometimes be beneficial to allow parts of
the scripts to be shared between the script modules. The main benefits of 
shared entities is the reduced memory consumption, and the fact that the
type of the shared entity is the same for the modules, thus simplifying the
information exchange between modules where this is used.

Shared entities have a restriction in that they cannot access non-shared
entities because the non-shared entities are exclusive to the script module 
in which they were compiled.

\section doc_script_shared_1 How to declare shared entities

To declare a shared entity simply put the keyword 'shared' before the ordinary
declaration, e.g.

<pre>
  shared class Foo
  {
    void MethodInFoo(int b) { bar = b; }
    int bar;
  }
</pre>

If the script tries to access any non-shared entity from within the shared
entity, then the compiler will give an error message. 

Obviously, in order to work the scripts in all modules that share the entity
must implement the entity the same way. If this is not done, the compiler will
give an error in the scripts that are compiled after the first script that 
implemented the shared entity.

The easiest way to guarantee that the implementation is the same is by using the
same source file, but this is not a requirement. 

\section doc_script_shared_2 What can be shared

Currently only the \ref doc_script_class "class" and \ref doc_global_interface "interface"
entities can be shared. 

Future versions may allow more entities, for example global 
\ref doc_global_function "functions" and \ref doc_global_variable "variables".

*/
