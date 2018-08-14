#import unittest2 as unittest
import unittest
import ifdh
import socket
import os
import time

from retryTests import retrycases

from argParse import argparsecases

from projTests import SAMCases

from cpTests import ifdh_cp_cases

from lockTests import ifdh_lock_cases

from auxFiles import aux_file_cases

from exitCodes import exitcodecases

from XProto import xproto_cases

from timeoutTests import TimeoutCases

def suite():
    os.environ['IFDH_CP_MAXRETRIES'] = '0'
    basesuite = unittest.TestLoader().loadTestsFromTestCase(SAMCases)
    basesuite2 = unittest.TestLoader().loadTestsFromTestCase(SAMCases)
    basesuite3 = unittest.TestLoader().loadTestsFromTestCase(ifdh_cp_cases)
    #basesuite4 = unittest.TestLoader().loadTestsFromTestCase(ifdh_lock_cases)
    basesuite5 = unittest.TestLoader().loadTestsFromTestCase(aux_file_cases)
    basesuite6 = unittest.TestLoader().loadTestsFromTestCase(exitcodecases)
    #basesuite7 = unittest.TestLoader().loadTestsFromTestCase(xproto_cases)
    basesuite8 = unittest.TestLoader().loadTestsFromTestCase(TimeoutCases)
    basesuite9 = unittest.TestLoader().loadTestsFromTestCase(argparsecases)
    basesuite10 = unittest.TestLoader().loadTestsFromTestCase(retrycases)
    #thissuite = unittest.TestSuite( [basesuite,basesuite2,basesuite3,basesuite4,basesuite5,basesuite6,basesuite7,basesuite8,basesuite9] )
    thissuite = unittest.TestSuite( [basesuite,basesuite2,basesuite3,basesuite5,basesuite6,basesuite8,basesuite9] )
    return thissuite

if __name__ == '__main__':
    #runner = unittest.TextTestRunner(verbosity=2, buffer=True)
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite())

