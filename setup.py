import os
from os.path import join as pjoin
from setuptools import setup
import subprocess
pkg='bullseye_mo'

def readme():
    with open('README.md') as f:
        return f.read()

def bullseye_pkg_dirs():
    """
    Recursively provide package_data directories for
    bullseye's measurement operator.
    """
    pkg_dirs = []

    #print '-'*80, '\n'
    
    path = pjoin(pkg, 'cbuild')
    subprocess.call(["mkdir",path])
    subprocess.check_call(["cd %s && cmake .. && make -j4" % path,""],shell=True)
    # Ignore
    exclude = ['docs', '.git', '.svn', 'CMakeFiles']

    # Walk 'bullseye_mo'
    for root, dirs, files in os.walk(path, topdown=True):
        #print '-'*20, 'ROOTS %s' % root
        #print '-'*20, 'DIRS %s' % dirs
        #print '-'*20, 'FILES %s' % files

        # Prune out everything we're not interested in
        # from os.walk's next yield.
        dirs[:] = [d for d in dirs if d not in exclude]

        for d in dirs:
            pkg_dirs.append(pjoin(root, d, '*.*'))


    print 'pkgdirs %s' % pkg_dirs

    return pkg_dirs


setup(name=pkg,
    version='0.0.1',
    description='Bullseye Measurement Operator',
    long_description=readme(),
    url='https://github.com/ratt-ru/bullseye.git',
    classifiers=[
        "Development Status :: 3 - Alpha",
        "Intended Audience :: Developers",
        "License :: Other/Proprietary License",
        "Operating System :: OS Independent",
        "Programming Language :: Python",
        "Topic :: Software Development :: Libraries :: Python Modules",
        "Topic :: Scientific/Engineering :: Astronomy",
    ],
    author='Benjamin Hugo',
    author_email='bennahugo@aol.com',
    license='MIT',
    packages=['bullseye_mo'],
    install_requires=['numpy','matplotlib','scipy'],
    package_data={pkg : bullseye_pkg_dirs()},
    include_package_data=True,
    zip_safe=False)
