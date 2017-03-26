# V4L2-Webcam

Simple video pipeline to be used as a base for image/video processsing algorithms. Uses
V4L2 to stream video from webcam and display in an SDL window.

To compile:
`gcc % -std=gnu99 -lSDL -O3 -o wc`

Light on resources. On my old i3 laptop..
`
17686 leo       20   0  119848   9812   8168 S   0.3  0.2   0:00.11 wc                              
17696 leo       20   0   25744   2956   2432 R   0.3  0.1   0:00.07 top  `
