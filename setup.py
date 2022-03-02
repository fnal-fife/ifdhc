#!/usr/bin/env python

import os
from sys import platform
from setuptools import setup,Extension
from setuptools.command.install import install
from distutils.command.build_ext import build_ext
from subprocess import call
from multiprocessing import cpu_count

BASEPATH = os.path.dirname(os.path.abspath(__file__))

class MakeExtension(Extension):

    def __init__(self, name, sourcedir=''):
        Extension.__init__(self, name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir)

class ifdhBuild(build_ext):

    def build_extension(self, ext):
            
        # build ifdh
        build_path = os.path.abspath(
            os.path.dirname(self.get_ext_fullpath(ext.name)))

        print("build_temp = %s" % build_path)

        self.mkpath(self.build_temp)
        bindir = self.build_lib + '/../bin'
        self.mkpath(bindir)
        self.mkpath(self.build_temp)

        cmd = [
            'make',
            'DESTDIR=%s/ifdhc/' % build_path,
            'clean',
            'all',
            'install',
        ]

        def compile():
            call(cmd)
            build_path = os.path.abspath(
               os.path.dirname(self.get_ext_fullpath(ext.name)))
            call(['mv', '%s/ifdhc/lib/python/ifdh.so' % build_path, '%s/ifdh.so' % build_path])

        self.execute(compile, [], 'Compiling ifdhc')

        # copy resulting tool to library build folder


def read(fname):
    return open(os.path.join(os.path.dirname(__file__), fname)).read()

setup(
    name='ifdhc',
    version='v2_6_2',
    description='Intensity Frontier Data Handling',
    maintainer='Marc Mengel',
    maintainer_email='mengel@fnal.gov',
    license='BSD',
    classifiers=[
        'Development Status :: 3 - Alpha',
        'Intended Audience :: Developers',
        'Intended Audience :: Science/Research',
        'License :: OSI Approved :: BSD',
        'Operating System :: Unix',
        'Programming Language :: C++',
        'Topic :: Scientific/Engineering :: Information Analysis',
    ],

    headers= ['ifdh/ifdh.h', 'util/WimpyConfigParser.h','util/utils.h'],
    cmdclass={
        'build_ext': ifdhBuild,
    },
    ext_modules=[MakeExtension('ifdh')],
    scripts=[ 'ifdh/demo.sh', 'ifdh/ifdh_copyback.sh', 'ifdh/www_cp.sh', 'ifdh/xrdwrap.sh'],
    data_files=[
        ('etc',['ifdh.cfg']),
      ],
)
