#############################################################
# Created by Leonid - lganzzzo@gmail.com
# 29 Jul 2021
#############################################################

import subprocess
import os
from pathlib import Path
from argparse import ArgumentParser
from shutil import copyfile

class Library:
  def __init__(self, name, path, deps, rels):
    self.name = name
    self.path = path
    self.deps = deps
    self.rels = rels

class LibPath:
  def __init__(self, name, isLink, path, target):
    self.name = name
    self.isLink = isLink
    self.path = path
    self.target = target

class RelPath:
  def __init__(self, name, relPath, targetPath, relPathOnly):
    self.name = name
    self.relPath = relPath
    self.targetPath = targetPath
    self.relPathOnly = relPathOnly

IGNORE_PATTERNS = [
  'QtSvg',
  'QtMacExtras',
  'QtWidgets',
  'QtGui',
  'QtCore'
]

##
# Should Library be igonored
##
def isInIgnoreList(ignoreList, filename):

  for entry in ignoreList:

    if entry in filename:
      return True

  return False

##
# Scan binary with otool and take only those that are at /urs/local/
##
def scanBinary(binary):

  realBinary = str(Path(binary).resolve())

  output = subprocess.check_output(['otool', '-L', realBinary]).decode('utf8')
  libsList = output.split(':\n\t')[1].split('\n\t')


  filename = realBinary.split('/')[-1]
  filepath = realBinary
  deps = {}
  rels = {}

  for libEntry in libsList:

    lib = libEntry.split(' (')[0]

    if isInIgnoreList(IGNORE_PATTERNS, lib):
      continue

    if lib.startswith('@rpath'):
      
      currPath = os.path.dirname(realBinary)

      relPath = lib.replace('@rpath/', '')
      relPathOnly = os.path.dirname(relPath)

      relFullPath = lib.replace('@rpath', currPath)
      targetPath = str(Path(relFullPath).resolve())
      relName = targetPath.split('/')[-1]

      if relName != filename:
        rels[relName] = RelPath(relName, lib, targetPath, relPathOnly)

    else:

      isLink = os.path.islink(lib)
      target = lib

      if isLink:
        target = str(Path(lib).resolve())

      if target.startswith('/usr/local/'):
        libname = target.split('/')[-1]
        deps[target] = LibPath(libname, isLink, lib, target)

  return Library(filename, filepath, deps, rels)

##
# Get all binary dependencies recursively
##
def scanBinrayRecursive(libs, binary):

  newLib = scanBinary(binary)
  if newLib not in libs:
    libs[newLib.name] = newLib

    for deplib in newLib.deps:
      filename = deplib.split('/')[-1]
      if filename not in libs:
        scanBinrayRecursive(libs, deplib)

##
# Get all binary dependencies recursively
##
def getFullDepsTree(binary):

  libs = {}
  scanBinrayRecursive(libs, binary)

  return libs


##
# Print all dependencies
##
def prtinDepsTree(libs):

  print(len(libs))

  for libname in libs:

    lib = libs[libname]

    print('---')
    print(lib.name + ': ' + lib.path)

    print('{')

    for dep in lib.deps:
      libPath = lib.deps[dep]
      if libPath.isLink:
        print('  [->] ' + libPath.path + ': ' + libPath.target)
      else:
        print('  [  ] ' + libPath.target)

    print('}')


def packToDestination(libs, dest, packPath):

  print('')
  print(str(len(libs)) + ' dependant libraries to pack')
  print('')
  print('packing...')
  print('')

  for libname in libs:

    lib = libs[libname]
    src = lib.path
    dst = dest + '/' + libname

    copyfile(src, dst)

    print(libname + ': {')

    if len(lib.rels) > 0:
      print('  relatives: {')

      for rel in lib.rels:
        relLib = lib.rels[rel]
        if relLib.relPathOnly:
          print('    ' + relLib.relPathOnly + '/' + relLib.name)
        else:
          print('    ' + relLib.name)

      print('  },')

    print('  dependencies: {')

    ## Change ID
    output = subprocess.check_output(['install_name_tool', '-id', packPath + '/' + lib.name, dst]).decode('utf8')
    if output:
      print(output)

    for dep in lib.deps:

      libPath = lib.deps[dep]
      print('    ' + libPath.path + ' -> ' + packPath + '/' + libPath.name)
      output = subprocess.check_output(['install_name_tool', '-change', libPath.path, packPath + '/' + libPath.name, dst]).decode('utf8')
      if output:
        print(output)

    print('  }')
    
    print('}')


#################################################################
#################################################################

parser = ArgumentParser()
parser.add_argument("-f", "--file", help="lib to pack")
parser.add_argument("-d", "--dest", help="destination folder")
parser.add_argument("-p", "--pack", help="package path")
args = parser.parse_args()

print("Pack library")
print("Arguments:")

if os.path.islink(args.file):
  realFile = str(Path(binary).resolve())
  print('Library: ' + args.file + ': ' + realFile)
else:
  print("Library: " + args.file)

print('Destination: ' + args.dest)
print('Package path: ' + args.pack + '/<dependency-lib>')

os.makedirs(args.dest, exist_ok=True)

libs = getFullDepsTree(args.file)
#prtinDepsTree(libs)
packToDestination(libs, args.dest, args.pack)




