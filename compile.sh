gcc -Wall -Isrc/include/ src/*.c `pkg-config --cflags --libs opencv` -O2 -o imgproc
