HTerm README

1. What is HTerm
----------------

HTerm is a terminal program for serial communication running on Windows and Linux.

Download the newest version at https://der-hammer.info/pages/terminal.html


2. Licenses
-----------

HTerm uses the following open source software:

- wxWidgets, licensed under "wxWindows Library Licence"
- fmt, licensed under "MIT License"
- spdlog, licensed under "MIT License"

For more details see accompanying LICENSE.txt.


3. FAQ / Tips and tricks
------------------------

* Load HTerm with configuration file
  Use hterm.exe configuration.cfg on windows or ./hterm configuration.cfg on Linux.

* Using custom baudrates
  Enter the baudrate in the input field. If it is supported by the hardware it is used
  otherwise an error is printed in the status line.

* Change input type in data input fields.
  The input type can be changes the the drop-down menu or with the following shortcuts,
  when the input field has focus:
  Alt+a : Ascii
  Alt+d : Decimal
  Alt+h : Hex
  Alt+b : Binary

* Install to Linux application launcher
  The Linux archive contains a desktop folder with desktop file and icon. Installation
  is testen on gnome/ubuntu. Copy files:
  cp <archive>/desktop/hterm.desktop ~/.local/share/applications
  cp <archive>/desktop/hterm.png ~/.local/share/icons/hicolor/32x32
  cp <archive>/hterm ~/.local/bin  # or elsewhere in PATH
  Refresh the desktop file cache (logout or alt+f2 "r")

* Using command (CMD) input mode
  Currently supported commands are
  - dtr=0 or dtr=1 or rts=0 or rts=1
  - wait=<number in milliseconds>, e.g. wait=1000 to wait 1 seconds before sending the
    remaining data
  Multiple command can be separated by ; e.g.
  dtr=1; wait=500; dtr=0

  The achievable accuracy of wait command is platform dependent and it might wait for
  multiple milliseconds longer than specified.


4. Reporting bugs
-----------------

For contact details see https://der-hammer.info/pages/terminal.html

Try to describe the issue as detailed as possible, include
* Which version of HTerm you are using
* The OS (Windows, Linux) and version it is running on
* The expected and the observed behavior
* Steps to reproduce the issue
* If available, the error message and/or screenshots of the issue
* Provide HTerm logs either from
  * Saving the last logs: Help > Show logs > Save
  * Saving all logs (better): hterm --log-file hterm-logs.txt
