'''
Ports features from abc to Python 3.
'''

# In order to import the real abc package, we need to use
# the following pragma.
from __future__ import absolute_import

import abc

class ABC(object):
    __metaclass__ = abc.ABCMeta

abstractmethod = abc.abstractmethod
abstractproperty = abc.abstractproperty
