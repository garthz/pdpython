# pdgui.py : trivial emulation of the pdgui module built into the pdpython external

# The pdpython external for Pure Data directly provides a pdgui module for
# callbacks into Pd itself.  This emulates those functions when testing Python
# modules outside of Pd.  This is an effective way to develop Python to connect
# with Pd; all messages passed to and from Pd use normal Python native types, so
# it is straightforward to write Python test code to emulate Pd messages during
# unit tests.

def post(string):
    """Simulate posting a string to the Pd console by simply printing it."""
    print string
