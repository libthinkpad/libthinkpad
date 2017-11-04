Welcome to the libthinkpad documentation 	{#mainpage}
============

### What is libthinkpad?

libthinkpad is a userspace general purpose library to change hardware configuration and <br>
manage hardware events in the userspace for Lenovo/IBM ThinkPad laptops. <br>

The library unifies ACPI dispatchers, hardware information systems and hardware configuration <br>
interfaces into a single userspace library. <br>

### What does this library enable?

This library enables userspace applications to configure the ThinkPad-specific hardware and fetch <br>
information about it. Examples include the docking events, the dock state and other things. <br>

This library also has a X configuration interface (similar to xrandr) based on the X11 RandR <br>
API wrapped into a nice, object oriented interface. <br>

The API is located in the DisplayManagement namespace.

### How to build the library?

To build the library you will need a few components first: <br>

__libsystemd__: needed to provide suspend support via logind  <br>
__libudev__: needed to monitor the system interface filesystem   <br>
__libXrandr__: needed to configure the X server rotation, positioning and state <br>    
__libX11__: dependency for libXRandr    <br>

*Note about systemd:* The library is not heavily dependent on systemd. <br>
systemd is only needed for power management state changes, and it can be <br> 
disable via the SYSTEMD CMake flag. <br>
However, disabling systemd support also cripples the power management system of the library. <br>

In the future, we will support *ALL* init systems. <br>

__Building the library__:

1) Download the latest source   <br>
2) Create a new folder called build inside the source root (*optional*)    <br>
3) Run `cmake .` or `cmake ..` depending on where you are     <br>
4) Run `make` <br>
5) Run `make install` <br>

Now to use the library, use `-lthinkpad`. <br>

To build the examples, use `g++ example.cpp -lthinkpad -std=c++11` <br>

### Where to start?

To start with the library, you can start exploring the ThinkPad namespace. <br>

Also, you can find examples inside the `examples` directory in the main source tree. <br>

### A few notes

This library is early in development, bugs may occur. <br>

Licensed under the BSD 2-Clause license.

