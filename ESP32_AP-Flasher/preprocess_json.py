#!/usr/bin/python3

import sys
import os
import subprocess
import glob
import json

content_defines = []

def preprocess_files(source_folder, destination_folder):
    # Create the destination folder if it doesn't exist
    if not os.path.exists(destination_folder):
        os.makedirs(destination_folder)

    # Get a list of all files in the source folder
    files = glob.glob(f'{source_folder}/*.json')

    for source_file_path in files:
        file = os.path.basename(source_file_path)
        destination_file_path = os.path.join(destination_folder, file)

        #print(f"preprocessing: {file}")
        cmd_line = [ 'cpp', '-Werror','-P',f'{source_file_path}','-o' f'{destination_file_path}' ]
        cmd_line += content_defines

        #print(f'Running {cmd_line}')
        result = subprocess.run(cmd_line)

        if result.returncode != 0:
            sys.exit(1)

    # Make sure we can parse the json file
        with open(destination_file_path,"r") as file:
            try:
                tagdata = json.load(file)

            except json.decoder.JSONDecodeError as err:
                print(f'Error parsing {destination_file_path} (generated from {source_file_path}):')
                print(f'{err}')
                sys.exit(1)

if 'Import' in globals() and callable(Import):
    Import("env")
    for define in env.get('BUILD_FLAGS', []):
        if define.startswith('-D CONTENT'):
            content_defines.append(define)

print('preprocessing json files with ',end = '')
first = True
for define in content_defines:
    if not first:
        print(', ',end = '')
    print(define,end = '')
    first = False
print('.')

# create runtime wwwroot/*.json from src/*.json
preprocess_files('json/wwwroot','wwwroot')
# create runtime resources/tagtypes/ from src/tagtypes/*.json
# NB: the contents of resources/tagtypes/ are normally downloaded by the AP as 
# needed, they are not part of the FS image.
preprocess_files('json/tagtypes','../resources/tagtypes/')


