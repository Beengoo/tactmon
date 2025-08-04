mkdir build 2> /dev/null
rm build/test_tactmon 2> /dev/null
gcc -g -o build/test_tactmon tactmon.c -I/usr/include/pipewire-0.3 -I/usr/include/spa-0.2 $(pkg-config --libs libpipewire-0.3 aubio) -lm
