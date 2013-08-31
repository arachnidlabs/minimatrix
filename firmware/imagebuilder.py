#!/usr/bin/env python

import os
import sys


def main():
    infiles = sys.argv[1:]
    outfile = infiles.pop()
    out = open(outfile, "w")
    header = open(os.path.splitext(outfile)[0] + ".h", "w")
    for file in infiles:
        name = file.replace(".", "_")
        f = open(file, 'rb')

        header.write("extern const unsigned char %s[] PROGMEM;\n" % (name,))
        out.write("const unsigned char %s[] PROGMEM = {\n" % (name,))
        line = f.read(16)
        while line:
            line = ", ".join("0x%.2X" % ord(b) for b in line)
            out.write("    %s,\n" % line)
            line = f.read(16)
        out.write("};\n\n")

if __name__ == '__main__':
    main()
