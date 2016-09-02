mbed Parking Meter Endpoint (Ethernet)

To compile locally, you need to install "mbed-cli"

Example (Linux):

 % pip install -U "mbed-cli"

Once installed:

 % git clone https://github.com/ARMmbed/mbed-parking-meter-ethernet

 % cd mbed-parking-meter-ethernet

 % mbed deploy

 % mbed target K64F

 % mbed toolchain GCC_ARM

 % mbed compile -m K64F -t GCC_ARM -c -j0

The compiled binary will be found in here:

 % cd .build/K64F/GCC_ARM/mbed-parking-meter-ethernet.bin

Lastly, copy the "bin" file to your mbed device and reset the device
