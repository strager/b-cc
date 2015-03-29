'''
Defines long as int for Python 3.
'''
try:
    # Python 2.
    long = long
except NameError:
    # Python 3.
    long = int
