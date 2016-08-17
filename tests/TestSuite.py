import unittest2 as unittest
import ifdh
import socket
import os
import time

from projTests import SAMCases

from cpTests import ifdh_cp_cases

from lockTests import ifdh_lock_cases

from auxFiles import aux_file_cases

from exitCodes import exitcodecases

from XProto import xproto_cases

def suite():
    os.environ['IFDH_CP_MAXRETRIES'] = '1'
    basesuite = unittest.TestLoader().loadTestsFromTestCase(SAMCases)
    basesuite2 = unittest.TestLoader().loadTestsFromTestCase(SAMCases)
    basesuite3 =  unittest.TestLoader().loadTestsFromTestCase(ifdh_cp_cases)
    basesuite4 =  unittest.TestLoader().loadTestsFromTestCase(ifdh_lock_cases)
    basesuite5 =   unittest.TestLoader().loadTestsFromTestCase(aux_file_cases)
    basesuite6 =   unittest.TestLoader().loadTestsFromTestCase(exitcodecases)
    basesuite7 =   unittest.TestLoader().loadTestsFromTestCase(xproto_cases)
    suite = unittest.TestSuite( [basesuite,basesuite2,basesuite3,basesuite4,basesuite5,basesuite6,basesuite7] )
    return suite

if __name__ == '__main__':
    runner = unittest.TextTestRunner(verbosity=2, buffer=True)
    result = runner.run(suite())

