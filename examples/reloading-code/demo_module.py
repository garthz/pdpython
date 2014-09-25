# demo_module.py : trivial Python module to demonstrate reloading pure Python code into a running Pd

class TestClass:

    def __init__(self):
        pass

    def get_value(self):
        # some trivial bit of code to change while running

        return 'hello'
        # return 'hello_changed'

    
