gcc -Wall -Isrc/include/ src/*.c `pkg-config --cflags --libs opencv` -o imgproc
