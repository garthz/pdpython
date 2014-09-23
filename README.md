pdpython
========

This repository contains an 'external' (plugin) for [Pure
Data](http://puredata.info) to allow embedding Python programs within Pd program
graphs.  The main 'python' object provided enables loading Python modules,
instantiating Python objects, calling object methods, and receiving method return
values.  

The methods in objects called from Pd are expected to follow a particular naming
convention.  In general practice, Pd-oriented interface classes need to be
defined for a given application.  However, since Pd has a limited vocabulary of
data types sent and returned from Python, these classes can be developed and
tested separately from Pd.

All data is converted between native primitive types for Pd and Python.  Native
Python objects cannot be passed through Pd but this can in practice be
surmounted with some clever bookkeeping on the Python side which uses name
tokens passed back and forth. Tuples returned from Python generate a separate Pd
message per element, so with a little routing on the Pd side it is not hard to
distribute data into multiple points in the Pd graph.

Note that data returned from Python is passed to the Pd object outlet but that
the outlet function is not called directly from Python.  This avoids potential
problems with recursive calls back into Python, but does make it more difficult
for Python objects to actively query the Pd graph; th by. 

Note that no support is currently provided for reloading a module, so modifying
the Python generally requires restarting Pd.

This is not the only Python plug-in for Pd, for example, see
[py/pyext](http://grrrr.org/research/software/py/).  However, I believe this one
is simpler and perhaps easier to compile and install, although less fully
featured.


Installation
------------


Reference
---------

Each Pd [python] object represents a single instance of a Python class object.

### Creation Arguments ###

The Pd 'python' object requires a minimum of two arguments at creation
specifying the module name and the class name, followed by optional arguments
passed to the object initializer:

    [python module_name function_name [arg]*]
    [python my_lib MyClassWithNoInitArgs]
    [python my_lib MyClass 1 2 3]
    [python my_lib.my_module MyClass 1 2 3]


### Input Types ###

The supported atomic types in Pd are converted symmetrically to and from Python native types:

| Pd atomic type              | Python atomic type |
------------------------------|--------------------|
| float                       | float              |
| symbol                      | string             |


Pd lists, bangs, and messages received by a [python] object representing a
Python object instance are mapped to a method call on that object `obj` as
follows:

| Pd type              | Method call               |  Example Pd Message | Example Python Call
-----------------------|---------------------------|---------------------|---------------------------
|  bang                | `obj.bang()`              | [ bang ]       	 | `obj.bang()`
|  float               | `obj.float(number)`       | [ 1.0 ]        	 | `obj.float( 1.0 )`
|  symbol              | `obj.symbol(string)`      | [ symbol foo ]   	 | `obj.symbol( "foo" )`
|  number list         | `obj.list( a1, a2, ...)`  | [ 1 2 3 ]      	 | `obj.list( 1.0, 2.0, 3.0 )`
|  list with selector  | `obj.$selector( a1, ...)` | [ goto 4 ]     	 | `obj.goto( 4.0 )`


This follows a general Pd convention that messages with selectors are analogous
to a function call within an object.  E.g. the message [set 1 2 3( passed to a
general message box will set the value of that box.

However, note that normal Python argument syntax makes receives it easy to
receive a list as a Python tuple rather than values distributed over arguments:

```python
def reverse_list( *args ):
    """Return a list of all arguments in reverse order.
    If passed a Pd list object, will return a reversed Pd list."""
    return list( args[::-1])  # use a slice operator to generate the reversed list
```

### Output Types ###

The float and string types returned from Python are converted to Pd floats and
symbols.  Lists are returned as Pd messages or lists.

Tuples are treated differently: a tuple returned from Python generates a
separate Pd outlet output message per element.  With a little routing on the Pd
side it is not hard to distribute data into multiple points in the Pd graph.
