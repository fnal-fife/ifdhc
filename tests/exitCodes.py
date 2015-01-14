import unittest
import ifdh
import socket
import os
import time
import sys

class ExitCodeCases(unittest.TestCase):
    def test_ls_noexist(self, force="", experiment="nova"):
        res = os.system("EXPERIMENT=%s ifdh ls /pnfs/nova/scratch/users/nosuchuser 0 %s" % (experiment, force)
        self.assertEqual(res, 256)
`
    def test_ls_noexist_gridftp(self):
        self.test_ls_noexist(force="--force=gridftp")

    def test_ls_noexist_srm(self):
        self.test_ls_noexist(force="--force=srm")

    def test_ls_noexist_local(self):
        res = os.system("ifdh ls /tmp/nosuchdir"
        self.assertEqual(res, 256)

    def test_ls_exist(self, force="", experiment="nova"):
        res = os.system("EXPERIMENT=%s ifdh ls /pnfs/nova/scratch/users 0 %s" % (experiment, force)
        self.assertEqual(res,0) 
`
    def test_ls_exist_gridftp(self):
        self.test_ls_noexist(force="--force=gridftp")

    def test_ls_exist_srm(self):
        self.test_ls_noexist(force="--force=srm")

    def test_ls_exist_local(self):
        res = os.system("ifdh ls /tmp"
        self.assertEqual(res, 0)

    def test_cp_noexist_src(self, force="", experiment="nova"):
        res = os.system("EXPERIMENT=%s ifdh cp /pnfs/nova/scratch/users/nosuchuser/nosuchfile /tmp/nosuchfile %s" % (experiment, force)
        self.assertEqual(res, 256)
   
    def test_cp_noexist_dst(self, force="", experiment="nova"):
        fname = "/tmp/testfile%d" % os.getpid()
        f = open(fname, "w")
        f.write("hello world\n")
        f.close()
        res = os.system("EXPERIMENT=%s ifdh cp /pnfs/nova/scratch/users/nosuchuser/nosuchfile %s %s" % (experiment, fname, force))
        self.assertEqual(res, 256)
   
    def test_cp_noexist_src_gridftp(self):
        self.test_cp_noexist_dst(force="--force=gridftp")

    def test_cp_noexist_src_expgridftp(self):
        self.test_cp_noexist_dst(force="--force=expgridftp")

    def test_cp_noexist_src_srm(self):
        self.test_cp_noexist_dst(force="--force=srm")

    def test_cp_noexist_dst_gridftp(self):
        self.test_cp_noexist_dst(force="--force=gridftp")

    def test_cp_noexist_dst_expgridftp(self):
        self.test_cp_noexist_dst(force="--force=expgridftp")

    def test_cp_noexist_dst_srm(self):
        self.test_cp_noexist_dst(force="--force=srm")

    def test_cp_noexist_src_local(self):
        fname = "/tmp/testfile%d" % os.getpid()
        f = open(fname, "w")
        f.write("hello world\n")
        f.close()
        res = os.system("EXPERIMENT=%s ifdh cp %s /tmp/nosuchdir/nosuchfile %s %s" % (experiment, fname, force))
        self.assertEqual(res, 256)
   
    def test_cp_noexist_dst_local(self, force="", experiment="nova"):
        fname = "/tmp/testfile%d" % os.getpid()
        f = open(fname, "w")
        f.write("hello world\n")
        f.close()
        res = os.system("EXPERIMENT=%s ifdh cp /tmp/nosuchuser/nosuchfile %s" % (experiment, fname, force))
        self.assertEqual(res, 256)
