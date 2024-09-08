#!/usr/bin/env python

import os
import argparse
from PIL import Image

known_resolutions = [
    [128,296],
    [144,200],
    [152,152],
    [200,200],
    [250,128],
    [250,132],
    [256,128],
    [264,176],
    [296,160],
    [296,152],
    [360,184],
    [384,168],
    [384,184],
    [400,300],
    [522,152],
    [600,448],
    [640,384],
    [648,480],
    [720,256],
    [960,640],
    [960,672],
    [960,768]
]

# OEPL raw image converter
def convert_file(file_in,file_out):
    print(f'Converting {file_in} to {file_out}')

    file_size = os.path.getsize(file_in)

    entries = {}
    bpp = 0;
    for x in known_resolutions:
        x.append(1)
        size = x[0] * x[1] / 8;
        entries[size] = x
        size *= 2
        y = x.copy()
        y[2] = 2
        entries[size] = y

    if not file_size in entries:
        print('Error: unknown file size, resolution can not be inferred.')
        return

    width = entries[file_size][0]
    height = entries[file_size][1]
    bbp = entries[file_size][2]
    print(f'Image format is {width} x {height}, {bbp} bbp')
    with open(file_in,"rb") as file:
       bin = bytearray(file.read())

    rgb_data = []
    offset = 0

    if bbp == 1:
        plane_size = file_size
    else:
        plane_size = file_size/2

    while offset < plane_size:
        mask = 0x80
        while mask != 0:
            if bin[offset] & mask:
                red = 0
                green = 0
                blue = 0
            else:
                red = 0xff
                green = 0xff
                blue = 0xff

            rgb_data.append((red,green,blue))
            mask >>= 1
        offset += 1

    #print('rgb_data:')
    #print(rgb_data)

    if bbp == 2:
    # merge in second color
        ByteOffset = 0
        while offset < file_size:
            mask = 0x80
            while mask != 0:
                if bin[offset] & mask:
                    red = 0xff
                    green = 0
                    blue = 0
                    #print(f'ByteOffset {ByteOffset}')
                    rgb_data[ByteOffset] = (red,green,blue)
                mask >>= 1
                ByteOffset += 1
            offset += 1

    #print('rgb_data:')
    #print(rgb_data)
    image = Image.new('RGB', (width,height))
    image.putdata(rgb_data,1.0,0)

    try:
        print(f'\nsaving to {file_out}')
        image.save(file_out,quality="maximum")

    except OSError:
        print(f'Could not save file {file_out}')

def main():
    parser = argparse.ArgumentParser(description="convert a OEPL .raw or .pending file to png or jpeg")
    parser.add_argument("input", help="Binary file to convert")
    parser.add_argument("output", help="output file (.png)")

    args = parser.parse_args()



    convert_file(args.input,args.output)


if __name__ == "__main__":
    main()

