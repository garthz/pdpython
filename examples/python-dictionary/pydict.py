# pydict.py : Python class to provide Pd access to a Python dictionary object.

# This class demonstrates a method for providing Pd access via named objects.
# The class keeps a global table of named dictionaries so that Pd can refer to a
# Python object by name.  This works around the limitation of the type system in
# which Python objects cannot be natively passed through Pd.

class PdDict:
    """A simple Pd wrapper around a Python dictionary.
    
    The dictionary can optionally be named at creation.  Subsequent attempts to
    create a dictionary of the same name will create an object referencing the
    same underlying dictionary.
    """

    # a static class variable to hold the named dictionaries
    named_dicts = dict()

    def __init__(self, name = None):
        self.name = name
        if name is None:
            # create an anonymous dictionary
            self.dict = dict()

        else:
            # try fetching an existing named dictionary; if that fails, create a
            # new named dict
            try:
                self.dict = PdDict.named_dicts[name]
            except KeyError:
                self.dict = dict()
                PdDict.named_dicts[name] = self.dict
        return

    def names( self ):
        """Return a list of names of named dictionaries."""
        return PdDict.named_dicts.keys()

    def set( self, key, value ):
        """Set a key-value pair."""
        self.dict[key] = value

    def get( self, key ):
        """Fetch the value for a given key."""
        return self.dict[key]

    def keys( self ):
        """Return a list of all keys."""
        return self.dict.keys()
