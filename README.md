# openeeg-opengl-visualization

Takes EEG data coming in from an OpenEEG device, splits it into the different brainwaves and uses them to change an almost real-time OpenGL ES visualization. The purpose is to teach basic meditation by keeping the visualizations as static as possible.

It currently works on two platforms: X Windows (linux) and Raspberry Pi (tested with Raspbian)

= Setup:

== Linux (X Windows)

To run on X11, the following libraries are required to be installed in /usr/include: X11, GLESv2, EGL, rfftw, fftw, Math and pthread.

These commands should take care of all setup on a Debian-based system, you may need to adapt for other systems:

`
sudo apt-get install git cmake libglm-dev
sudo apt-get install libx11-dev
sudo apt-get install libgles2-mesa-dev

make setup_nsd         # Fails? Install neuroserver manually (https://github.com/BCI-AR/NeuroServer)
make setup_fftw2       # Fails? Install FFTW 2.1.5 manually (http://www.fftw.org/download.html)
make meditrainer_x11   # Fails? Please contact the author!
`

If all of the above went correctly, these executable files should exist:

nsd
modeegdriver
meditrainer

= Running Meditrainer

./nsd  # Starts neuroserver TCP server for EEG data
sudo ./modeegdriver -d /dev/ttyUSB0  # Starts a driver to stream EEG data to the nsd. ttyUSB0 should point to the OpenEEG drive.
./meditrainer

At this point you should see red lines on a black screen. This is calibration mode.

== Calibration

Press the space bar or the 'a' key to move on to the different visualizations.

== Waves

== Fire

== Oscilloscope

== Fiery Tunnel

