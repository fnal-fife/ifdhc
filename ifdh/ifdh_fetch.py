#!/bin/sh

from ifdh import ifdh
import os
import re

def ifdh_fetch( *flist, **kwargs ):
     ifdh_handle = ifdh()
     if kwargs.has_key("dims"):
          flist.extend( ifdh_handle.translateConstraints(kwargs["dims"]))

     res = os.system("ifdh_fetch '" + "' '".join(flist) + "'")
     return res == 0
