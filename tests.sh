#!/bin/sh
#

echo "1. IPv4 simple tire"
./rtLookup 4 simple perf

echo && echo && echo "2. IPv4 path-compressed tire:"
./rtLookup 4 pc perf

echo && echo && echo "3. IPv6 simple tire:"
./rtLookup 6 simple perf

echo && echo && echo "4. IPv6 path-compressed tire:"
./rtLookup 6 pc perf
