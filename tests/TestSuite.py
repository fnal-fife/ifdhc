import unittest
import ifdh
import socket
import os
import time

from projTests import SAMCases

from cpTests import ifdh_cp_cases

def suite():
    basesuite = unittest.TestLoader().loadTestsFromTestCase(SAMCases)
    basesuite2 = unittest.TestLoader().loadTestsFromTestCase(SAMCases)
    basesuite3 =  unittest.TestLoader().loadTestsFromTestCase(ifdh_cp_cases)
    suite = unittest.TestSuite( [basesuite,basesuite2,basesuite3] )
    return suite

if __name__ == '__main__':
    runner = unittest.TextTestRunner()
    result = runner.run(suite())

