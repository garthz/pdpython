# reloader.py : Python class to manage reloading other modules during a single Pd session

# import any modules which might need to be reloaded
import demo_module

# simple object to handle the reload
class Reloader:
    def __init__(self):
        pass

    def reload_demo_module(self):
        reload(demo_module)
        return
