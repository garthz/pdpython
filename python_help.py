# python_help.py : Python class to support the help patch for the [python] object.

# Import the callback module built in to the external.
import pdgui

# Define a class which can generate objects to be instantiated in Pd [python] objects.
# This would be created in Pd as [python demo_class DemoClass].

class PyHelpClass:

    # The class initializer receives the creation arguments specified in the Pd [python] object.
    def __init__( self, *creation ):
        pdgui.post( "Creating PyHelpClass object with creation args: %s." % str(creation))
        return

    # The following methods are called in response to Pd basic data messages.
    def bang(self):
        pdgui.post( "Python PyHelpClass object received bang.")
        return True

    def float(self, number):
        pdgui.post( "Python PyHelpClass object received %f." % number)
        return number

    def symbol(self, string ):
        pdgui.post( "Python PyHelpClass object received symbol: '%s'." % string)
        return string

    def list(self, *args ):
        # note that the *args form provides a tuple
        pdgui.post( "Python PyHelpClass object received list: %s." % str(args))
        return list(args)


    # Messages with selectors are analogous to function calls, and call the
    # corresponding method.  E.g., the following could called by sending the Pd
    # message [goto 33(.
    def goto(self, location ):
        pdgui.post( "Python PyHelpClass object received goto message:" + location)
        return location

    # This method demonstrates returning a list, which returns as a Pd list.
    def moveto( self, x, y, z ):
        pdgui.post( "Python PyHelpClass object received move message with %f, %f, %f." % ( x, y, z))
        return [x, y, z]

    def blah(self):
        pdgui.post( "Python PyHelpClass object received blah message.")
        return 42.0

    # This method demostrates returning a tuple, each element of which generates
    # a separate Pd outlet message.
    def tuple(self):
        pdgui.post( "Python PyHelpClass object received tuple message.")
        return ( ['element', 1], ['element', 2], ['element', 3],[ 'element',4 ] )
        
