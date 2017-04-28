#!/usr/bin/env python

import os
from sys import platform
from setuptools import setup
from setuptools.command.install import install
from distutils.command.build import build
from subprocess import call
from multiprocessing import cpu_count

BASEPATH = os.path.dirname(os.path.abspath(__file__))

class ifdhBuild(build):
    def run(self):
        # run original build code
        build.run(self)

        # build ifdh
        build_path = os.path.abspath(self.build_temp)

        cmd = [
            'make',
            'all',
            'install',
            'DESTDIR=%s' % build_path
        ]

        def compile():
            call(cmd)

        self.execute(compile, [], 'Compiling xcsoar')

        # copy resulting tool to library build folder
        self.mkpath(self.build_lib)
        bindir = self.build_lib + '/../bin'
        self.mkpath(bindir)

        target_files=('lib/python/_ifdh.so', 'lib/python/ifdh.py')
        bin_target_files=('bin/ifdh',)

        if not self.dry_run:
            for target in target_files:
                self.copy_file(target, self.build_lib)
            for target in bin_target_files:
                self.copy_file(target, bindir)


class ifdhInstall(install):
    def initialize_options(self):
        install.initialize_options(self)
        self.build_scripts = None

    def finalize_options(self):
        install.finalize_options(self)
        self.set_undefined_options('build', ('build_scripts', 'build_scripts'))

    def run(self):
        # run original install code
        install.run(self)

        # install ifdh executables
        self.copy_tree(self.build_lib, self.install_lib)
        print "install/run self has: ", self.__dict__.keys()

        self.copy_tree(self.build_lib + '/../bin', self.install_base + '/bin')


def read(fname):
    return open(os.path.join(os.path.dirname(__file__), fname)).read()


setup(
    name='ifdh',
    version='v2_0_5',
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

    headers= ['inc/ifdh.h', 'inc/WimpyConfigParser.h','inc/utils.h'],
    cmdclass={
        'build': ifdhBuild,
        'install': ifdhInstall,
        'install-headers': None
    }
)
