/// pdpython.c : Pd external to bridge data in and out of Python
/// Copyright (c) 2014, Garth Zeglin.  All rights reserved.  Provided under the
/// terms of the BSD 3-clause license.
///
/// Updated to Python3 and modernized build methods by S. Alireza (shakfu)
/// Each Pd 'python' object represents a single instance of a Python class object.

/****************************************************************/
// Links to related reference documentation:

//   Python extensions: http://docs.python.org/2/extending/index.html
//   Python C type API: http://docs.python.org/2/c-api/concrete.html
//   Pd externals:      http://pdstatic.iem.at/externals-HOWTO/node9.html

/****************************************************************/
// Import the API for using Python as an extension language.  Python insists on
// being included before system headers, and indeed placing this here eliminates
// a warning.
#include <Python.h>

// import standard libc API
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Import the API for Pd externals.  This is provided by the Pd installation,
// and this may require manually configuring the Pd include path in the
// Makefile.
#include "m_pd.h"

/****************************************************************/ 
/// Data structure to hold the state of a single Pd 'python' object.
/// Each object represents one instance of a Python class.
typedef struct pdpython
{
  t_object x_ob;           ///< standard object header
  t_outlet *x_outlet;      ///< the outlet is the port on which return values are transmitted
  PyObject *py_object;     ///< Python class object represented by this pd object
} t_pdpython;

static t_class *pdpython_class;

/****************************************************************/
// Utility functions for object conversion.

/// Attempt to convert a Pd object to a Python object.  The caller must take
/// responsibility for releasing the Python object reference returned.
static PyObject *t_atom_to_PyObject( t_atom *atom ) 
{
  switch( atom->a_type ) {

  case A_FLOAT:
    return PyFloat_FromDouble( atom_getfloat(atom) );

  case A_SYMBOL:
    // symbols are returned as strings
    return PyUnicode_FromString( atom->a_w.w_symbol->s_name  );

  case A_NULL:
    Py_RETURN_NONE;

  default:
    // A_POINTER
    // A_SEMI
    // A_COMMA
    // A_DEFFLOAT
    // A_DEFSYM
    // A_DOLLAR
    // A_DOLLSYM
    // A_GIMME
    // A_CANT
    // A_BLOB
    post( "Warning: type %d unsupported for conversion to Python.", atom->a_type );
    Py_RETURN_NONE;
  }
}

/****************************************************************/
/// Convert a list of Pd atoms into a Python list.
static PyObject *t_atom_list_to_PyObject_list( int argc, t_atom *argv )
{
  PyObject *list = PyTuple_New( argc );
  int i;
  for (i = 0; i < argc; i++) {
    PyObject *value = t_atom_to_PyObject( &argv[i] );
    PyTuple_SetItem( list, i, value); // pass the value reference to the tuple
  }
  return list;
}

/****************************************************************/
/// Set a Pd atom structure to a representation of an object of atomic concrete Python type.
static void PyObject_to_atom( PyObject *value, t_atom *atom )
{
  // Python True and False are translated to 1 and 0; this is more in keeping with Pd style
  if (value == Py_True)            SETFLOAT( atom, 1.0 );
  else if (value == Py_False)      SETFLOAT( atom, 0.0 );
  else if ( PyFloat_Check(value))  SETFLOAT( atom, (float) PyFloat_AsDouble( value ));
  else if ( PyLong_Check(value))   SETFLOAT( atom, (float) PyLong_AsLong( value ));
  else if ( PyUnicode_Check(value)) SETSYMBOL( atom, gensym( PyUnicode_AsUTF8(value) ));
  else SETSYMBOL( atom, gensym("error"));
}
/****************************************************************/
/// Create a newly allocated list of Pd atoms from a Python list.  The caller is
/// responsible for freeing the memory block allocated with malloc() and
/// returned in *argv.  Pd lists cannot be nested, e.g,. they are only 1D
/// arrays, so an error token is substituted for any non-atomic object.
static void new_list_from_sequence( PyObject *seq, int *argc, t_atom **argv )
{
  Py_ssize_t len = 0;
  Py_ssize_t i;

  if ( PyList_Check(seq)) {
    len = PyList_Size(seq);
    *argv = (t_atom *) malloc( len*sizeof(t_atom));
    for (i = 0; i < len; i++) {
      PyObject *elem = PyList_GetItem( seq, i );
      PyObject_to_atom( elem, (*argv) + i );
    }
  }
  *argc = (int) len;
}
/****************************************************************/
/// Emit a Python object as an outlet message.  Tuples generate multiple
/// messages and are handled separately.
static void emit_outlet_message( PyObject *value, t_outlet *x_outlet )
{
  // Python True and False are translated to 1 and 0; this is more in keeping with Pd style
  if (value == Py_True)           outlet_float( x_outlet, 1.0 );
  else if (value == Py_False)     outlet_float( x_outlet, 0.0 );

  // scalar numbers of various types come out as float
  else if ( PyFloat_Check(value))  outlet_float(  x_outlet, (float) PyFloat_AsDouble( value ));
  else if ( PyLong_Check(value))   outlet_float(  x_outlet, (float) PyLong_AsLong( value ));
  else if ( PyUnicode_Check(value)) outlet_symbol( x_outlet, gensym( PyUnicode_AsUTF8(value) ));

  else if ( PyList_Check(value) ) {
    // Create an atom array representing a 1D Python list.
    t_atom *argv = NULL;
    int argc = 0;
    new_list_from_sequence( value, &argc, &argv );

    if (argc > 0) {
      // Follow the Pd rules for interpreting lists.  If the first element is a symbol, then treat it as
      // the 'selector', otherwise treat all elements as data.
      if ( argv[0].a_type == A_SYMBOL) {
	outlet_anything( x_outlet, atom_getsymbol( &argv[0] ), argc-1, argv+1);      
      } else {
	outlet_list( x_outlet, &s_list, argc, argv );
      }
    }
    if (argv) free (argv);
  }
}

/****************************************************************/
/// Call a method of the associated Python object based on the inlet value.
///
/// Message types are handled as follows:
///   bang                : obj.bang()               example: [ bang ]   calls obj.bang() 
///   float               : obj.float(number)        example: [ 1.0 ]    calls obj.float( 1.0 )
///   number list         : obj.list( a1, a2, ...)   example: [ 1 2 3 ]  calls obj.list( 1.0, 2.0, 3.0 )
///   list with selector  : obj.$selector(string)    example: [ goto 4 ] calls obj.goto( 4.0 )
///
/// more examples:
///  [ blah ]      is a lsit with a selector and null values, calls obj.blah()
///  [ goto home ] is a list with a selector and a symbol, calls obj.blah( "home" )
///
/// a weird special case:
///  symbol  : obj.symbol(string)      example: [ symbol ] calls obj.symbol("symbol")

static void pdpython_eval( t_pdpython *x, t_symbol *selector, int argcount, t_atom *argvec )
{
  // post ("pdpython_eval called with %d args, selector %s\n", argcount, selector->s_name );

  PyObject *func = NULL;
  PyObject *args = NULL;
  PyObject *value = NULL;    

  if (x->py_object == NULL) {
    post("Warning: message sent to uninitialized python object.");
    return;
  }

  // Process the selector symbol.  This needs to be treated specially; some values
  // indicate atom messages and should not be included in the form.
  // if ( selector == &s_bang )  {
  //   func = PyObject_GetAttrString( x->py_object, "bang" );
  // } else {
  //   func = PyObject_GetAttrString( x->py_object, selector->s_name );
  //   args = t_atom_list_to_PyObject_list( argcount, argvec );
  // }

  func = PyObject_GetAttrString( x->py_object, selector->s_name );
  args = t_atom_list_to_PyObject_list( argcount, argvec );
  
  if (!func) {
    post("Warning: no Python function found for selector %s.", selector->s_name );
  } else {
    if (!PyCallable_Check( func )) {
      post("Warning: Python attribute for selector %s is not callable.", selector->s_name );
    } else {
      value = PyObject_CallObject( func, args );
    }
    Py_DECREF( func );
  }

  if (args) Py_DECREF( args );

  if (value == NULL) {
    post("Warning: Python call for selector %s failed.", selector->s_name );

  } else {
    if (PyTuple_Check(value)) {
      // A tuple generates a sequence of outlet messages, one per item.
      int i, len = (int) PyTuple_Size( value );
      for (i = 0; i < len; i++) {
	PyObject *elem = PyTuple_GetItem( value, i );
	emit_outlet_message( elem, x->x_outlet );
      }
    } else {
      emit_outlet_message( value, x->x_outlet );
    }

    Py_DECREF( value );
  }
}

/****************************************************************/
/// Create an instance of a Pd 'python' object.
///
/// The creation arguments are treated as follows:
///    module_name function_name [arg]*
///
/// The Python function must return a Python callable object which can be called
/// with messages.  It is typically a class allocator.

static void *pdpython_new(t_symbol *selector, int argcount, t_atom *argvec)
{
  t_pdpython *x = (t_pdpython *) pd_new(pdpython_class);
  x->py_object = NULL;

  // post("pdpython_new called with selector %s and argcount %d", selector->s_name, argcount );

  if (argcount < 2) {
    post("Error: python objects require a module and function specified in the creation arguments.");

  } else {
    // Add the current canvas path to the Python load path if not already
    // present.  This will help the module import to find Python modules
    // located in the same folder as the patch.
    t_symbol *canvas_path = canvas_getcurrentdir();
    PyObject* modulePath = PyUnicode_FromString( canvas_path->s_name );
    post("modulepath: %s", canvas_path->s_name);
    
    PyObject *sysPath = PySys_GetObject((char *)"path"); // borrowed reference

    if ( !PySequence_Contains( sysPath, modulePath )) {
      post("Appending current canvas path to Python load path: %s", canvas_path->s_name );
      PyList_Append(sysPath, modulePath );
    }
    Py_DECREF( modulePath );


    // try loading the module
    PyObject *module_name   = t_atom_to_PyObject( &argvec[0] );
    PyObject *module        = PyImport_Import( module_name );
    Py_DECREF( module_name );

    if ( module == NULL ) {
      post("Error: unable to import Python module %s.", argvec[0].a_w.w_symbol->s_name );

    } else {
      PyObject *func = PyObject_GetAttrString( module, argvec[1].a_w.w_symbol->s_name );

      if ( func == NULL) {
	post("Error: Python function %s not found.", argvec[1].a_w.w_symbol->s_name );

      } else {
	if (!PyCallable_Check( func )) {
	  post("Error: Python attribute %s is not callable.", argvec[1].a_w.w_symbol->s_name );

	} else {
	  PyObject *args = t_atom_list_to_PyObject_list( argcount-2, argvec+2 );
	  x->py_object   = PyObject_CallObject( func, args );
	  Py_DECREF( args );
	}
	Py_DECREF( func );
      }
      Py_DECREF( module );
    }
  }

  // create an outlet on which to return values
  x->x_outlet = outlet_new( &x->x_ob, NULL );
  return (void *)x;
}
/****************************************************************/
/// Release an instance of a Pd 'python' object.
static void pdpython_free(t_pdpython *x)
{
  post("python freeing object");
  if (x) {
    outlet_free( x->x_outlet );
    if (x->py_object) Py_DECREF( x->py_object );
    x->x_outlet = NULL;
    x->py_object = NULL;
  }
}
/****************************************************************/
/// Define an internal 'pdgui' module with a post() method to allow Python
/// programs to print messages on the Pd console.  This function is the C
/// wrapper function called by Python for the Pd post() function.  This is the
/// only means for Python to make calls back into Pd.
///
/// The argument must be a single string.  Formatting can be handled in Python.

static PyObject* pdgui_post( PyObject *self __attribute__((unused)), PyObject *args )
{
  char *text;
  if( !PyArg_ParseTuple(args, "s", &text )) {
    post("Warning: unprintable object posted to the console from a python object.");
    return NULL;
  } else {
    post( text );
    Py_RETURN_NONE;
  }
}

/// Define the pdgui module for C callbacks from Python to the Pd system.
static PyMethodDef pdgui_methods[] = {
  { "post", pdgui_post, METH_VARARGS, "Print a string to the Pd console." },
  { NULL, NULL, 0, NULL }
};

static struct PyModuleDef pdguimodule = {
    PyModuleDef_HEAD_INIT,
    "pdgui", /* name of module */
    NULL,    /* module documentation, may be NULL */
    -1,      /* size of per-interpreter state of the module,
                 or -1 if the module keeps state in global variables. */
    pdgui_methods, /* A pointer to a table of module-level functions */
    NULL, /* When using single-phase initialization, m_slots must be NULL */
    NULL, /* traversal function to call during GC traversal of the module object*/
    NULL, /* clear func to call during GC clearing of module object, or NULL if not needed.*/
    NULL  /* func to call during deallocation of module object, or NULL if not needed. */
};

PyMODINIT_FUNC
PyInit_pdgui(void)
{
  return PyModule_Create(&pdguimodule);
}
/****************************************************************/
/// Initialization entry point for the Pd 'python' external.  This is
/// automatically called by Pd after loading the dynamic module to initialize
/// the class interface.
void python_setup(void)
{
  // specify "A_GIMME" as creation argument for both the creation
  // routine and the method (callback) for the "eval" message.

  pdpython_class = class_new( gensym("python"),              // t_symbol *name
			      (t_newmethod) pdpython_new,    // t_newmethod newmethod
			      (t_method) pdpython_free,      // t_method freemethod
			      sizeof(t_pdpython),            // size_t size
			      0,                             // int flags
			      A_GIMME, 0);                   // t_atomtype arg1, ...

  // every input will be directly interpreted by Python; there need only be one
  // inlet-callback function.
  class_addanything( pdpython_class, (t_method) pdpython_eval);   // (t_class *c, t_method fn)

  wchar_t *program;
  program = Py_DecodeLocale("py", NULL);
  if (program == NULL) {
      exit(1);
  }

  Py_SetProgramName(program);

  // make the internal pdgui wrapper module available for Python->C callbacks
  if (PyImport_AppendInittab("pdgui", PyInit_pdgui) == -1)
  {
    post("Error: unable to create the pdgui module.");
  }

  // static initialization follows
  // Py_SetProgramName("pd");
  Py_Initialize();

  // Make sure that sys.argv is defined.
  // static char *arg0 = NULL;
  static wchar_t *arg0 = NULL;
  PySys_SetArgv( 0, &arg0 );

}

/****************************************************************/

