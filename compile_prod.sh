mkdir build 2> /dev/null
rm build/tactmon 2> /dev/null
gcc -o build/tactmon tactmon.c -I/usr/include/pipewire-0.3 -I/usr/include/spa-0.2 $(pkg-config --libs libpipewire-0.3 aubio) -lm
