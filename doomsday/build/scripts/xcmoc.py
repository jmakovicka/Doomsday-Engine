#!/usr/bin/env python
# -*- coding: utf-8 -*-

#  xcmoc.py
#  Run Qt moc on all classes that contain Q_OBJECT
#
#  Created by Jaakko Keränen on 2017-05-07.
#  Copyright © 2017 Jaakko Keränen <jaakko.keranen@iki.fi>

import sys, os, re, subprocess, hashlib

def contains_qobject(fn):
    for line in open(fn, 'rt').readlines():
        if 'Q_OBJECT' in line:
            return True
    return False

def find_headers(dir_path):
    headers = []
    for fn in os.listdir(dir_path):
        file_path = os.path.join(dir_path, fn)
        if os.path.isdir(file_path):
            headers += find_headers(file_path)
        elif fn.endswith('.h'):
            if contains_qobject(file_path):
                headers.append(os.path.abspath(file_path))
    return headers
    
def find_source(name, dir_path = os.path.join(sys.argv[1], 'src')):
    for fn in os.listdir(dir_path):
        file_path = os.path.join(dir_path, fn)
        if os.path.isdir(file_path):
            found = find_source(name, file_path)
            if found: return found
        elif fn == name[:-2] + '.cpp':
            return os.path.abspath(file_path)
    return None
    
def md5sum(text):
    m = hashlib.md5()
    m.update(text.encode())
    return m.hexdigest()    

#print "Running moc in", sys.argv[1]
OUT_DIR = os.getenv('PROJECT_TEMP_DIR')
try:
    os.makedirs(OUT_DIR)
except:
    pass

headers = find_headers(sys.argv[1])

QT_DIR = os.getenv('QT_DIR')
defines = ['-D%s' % d for d in os.getenv('GCC_PREPROCESSOR_DEFINITIONS').split()]
includes = ['-I%s' % d for d in re.split(r"(?<=\w)\s", os.getenv('HEADER_SEARCH_PATHS'))]
args = ['%s/bin/moc' % QT_DIR] + defines + includes

compilation = ['/* This file is autogenerated by xcmoc.py */']

for header in headers:
    header_name = os.path.basename(header)
    dir_path = os.path.dirname(header)
    moc_name = '%s_moc_%s.cpp' % (md5sum(dir_path), header_name[:-2])
    moc_path = os.path.join(OUT_DIR, moc_name)

    compilation.append('#include "%s"' % moc_name)    

    # Check timestamps.
    if not os.path.exists(moc_path) or \
            os.path.getmtime(header) > os.path.getmtime(moc_path):    
        print 'Running moc:', header_name
        subprocess.check_output(args + [header, '-o', moc_path], stderr=subprocess.STDOUT)    

comp_path = os.path.join(OUT_DIR, '%s_moc_compilation.cpp' % os.getenv('TARGETNAME'))

open(comp_path, 'wt').write("\n".join(compilation) + "\n")

    