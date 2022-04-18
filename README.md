# pipedal_p2pd

Provides P2P (WiFi Direct) session management for wpa_supplicant, as used in the PiPedal project.

For support, please go to the [PiPedal Project](http://github.com//rerdavies/pipedal) page.

Built as a submodule in the PiPedal Project Build.


## Borrowing Code 

The project is currently fairly tightly coupled to PiPedal. The current implementation addresses PiPedal's needs adequately; but there's functionality 
missing for a general-purpose library.

If there's interest, I could be convinced to speed up the development of a full component library. (nudge nudge sponsor sponsor. ;-P ). But that's fairly
far down my list of personal priorities.

Please rename binaries and libraries if you fork the code. Not doing so will break your installs and mine. And there's a good chance that some or all of this code will get upgraded to a more generally usable component.

## State of the Code.


What's provided:

- P2P (WiFi Direct) session management for autonomous groups (no GO negotiation), with LABEL authentication only. (i.e. a fixed pin that would be 
   printed on a label attached to the bottom of the device).
   
- Fast reliable pairing with Android devices.

- Runs on the dhcpcd network stack used by Raspberry Pi OS.

- libcotask -- a fairly effecient and interesting C++20 coroutine library providing  co-routine-based I/O on sockets (not yet for filesystem objects). 
 Mostly documented through a doxygen build. It's bound for general release, but not there yet.

What's not provided:

-   Proper keypad, display, pbc authentication. (Needs a control socket to interact with UI).
 
-   Provides P2P UPNP service discovery. Needs P2P DNS-SD service discovery.

-   Support for more than one running autonomous Group is designed-in, but incomplete (Raspberry Pi can only run one GO anyway).
 
-   Testing and configuration for newtworkd-based network stacks.
-   
-   Automonous GO only.

-   Testing on fully-compliant C++20 compilers coroutine implementation. (Raspberry Pi OS only supports G++10.2).
    
-   Switch the coroutine library over to iouring libarary in order to get asynchronous file i/o as well.



## Build Procedure.

A CMake based build and VSCODE configuration files are  provided. Actual production builds are done using this project as a sub-module of the PiPedal project. Installation and systemd configuration are also done in the PiPedal project.

Depencies are pretty minimal.  libuuid, and libsystemd, and that's probably it.



