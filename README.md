# pipedal_p2pd

Provides P2P (WiFi Direct) session management for wpa_supplicant, as used in the PiPedal project.


## Borrowing Code 

The project is currently fairly tightly coupled to PiPedal. The current implementation addresses PiPedal's needs adequately; but there's functionality 
missing for a general-purpose library.

Not currently suitable for general use; but what's here can be easily leveraged your own purposes. If you branch, please rename the executables and libaries
so you don't take down my projects. There's a good chance this code will go on to become a properly componentized library.

If there's interest, I could be convinced to speed up the development of a full component library. (nudge nudge sponsor sponsor. ;-P ). But that's fairly
far down my list of personal priorities.

Pull requests are welcome. Support is pretty much as-is, for now.

What's provided:

   - P2P (WiFi Direct) session management for autonomous groups (no GO negotiation), with LABEL authentication only. (i.e. a fixed pin that would be 
      printed on a label attached to the bottom of the device). 

    - There is code for a "none" authenticantion mode (PBC always on) -- which could be used 
      while doing initial onboarding for an IoT device -- not recommended due to a a plethora of security issues. 

   - Allows robust successful connections from Android devices. 

   - Provides WiFi Direct UPNP service discovery for PiPedal.


   - Runs on the dhcpcd network stack used by Raspberry Pi OS.

   - libcotask -- a fairly effecient and interesting C++20 coroutine library providing  co-routine-based I/O on sockets (not yet for filesystem objects). Mostly documented through a doxygen build. It's bound for general release, but not there yet.

What's not provided:

-   Proper keypad, display, pbc authentication, which requires UI support on the host device to display pins (keypad) enter pins (display), and push -buttons (pbc).   This requires -- at a minimum -- a socket service that allows UI components to interact with the service. And probably a user interface as well.
    (Control socket: easy; session management extensions: some easy debugging left; UI: need help with that).

-   A possibility that "keypad" and "display" authentication options are a bottomless rabbithole. wap_supplicant appears to use BSID's instead of device GUIDs
    for caching re-authentication credetials, which may break Android fast re-connect feature. Fixable, but dire.

-   More general WiFi Direct upnp service discovery; and WiFi Direct DNS-SD discovery as well. (Easy)

-   Support for more than one running autonomous Group is designed-in, but missing code in a few places. Raspberry PI devices only allows one GO, so it's untestable
    unless the package gets ported to a more fully functional platform. (Easy with appropriate test device).
    
-   Testing and configuration for newtworkd-based network stacks. Android requires working DHCP servers on the P2P GO ports. Mostly this is just a matter of 
    ensuring that p2p group network get assigned a non-default IPv4 address, which is easy on networkd stacks.  (Easy).

-   Absolutely no support for GO negotation. The current implementation is for an Autonomous GO, running on the local device, only.

-   Switch the coroutine library over to ioring libarary in order to get asynchronous file i/o as well.

-   Testing on fully-compliant C++20 compilers coroutine implementation. (Currently runs on G++10.2). Medium -- because Raspberry pi doesn't support 
    G++11+ toochains yet. So, a complete port to Ubuntu (along with the implementation of networkd support) is required.


## Build Procedure.

A CMake based build and VSCODE configuration files are  provided. Actual production builds are done using this project as a sub-module of the PiPedal project. Installation and systemd configuration are also done in the PiPedal project.

Depencies are pretty minimal.  libuuid, and libsystemd, and that's probably it.



