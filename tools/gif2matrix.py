#!/usr/bin/env python

import struct
import sys
from intelhex import IntelHex
from PIL import Image, ImageSequence

data_offset = 3

def main(args):
    if len(args) == 1:
        img = Image.open(sys.stdin)
    else:
        img = Image.open(args[1])
    
    if img.mode not in ('1', 'P'):
        sys.exit("Expected a 1-bit image, got %s" % img.mode)
    
    if img.size != (8, 8):
        sys.exit("Expected an 8x8 pixel image")
    
    if 'duration' not in img.info:
        sys.exit("Expected an animation")
    
    out = IntelHex()
    out.puts(0x00, struct.pack("BBB", 1, img.info['duration'] / 10, 0))
    
    for idx, frame in enumerate(ImageSequence.Iterator(img)):
        framedata = [0] * 8
        for i, bit in enumerate(frame.getdata()):
            framedata[i % 8] = (framedata[i % 8] << 1) | bit
        out.puts(data_offset + idx * 8, struct.pack('8B', *framedata))
    
    # Write out the frame count
    out[0x02] = idx + 1
    
    if len(args) == 2:
        out.tofile(sys.stdout, format='hex')
    else:
        out.tofile(open(args[2], 'w'), format='hex')


if __name__ == '__main__':
    main(sys.argv)
