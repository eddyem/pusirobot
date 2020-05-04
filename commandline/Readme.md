Command line motor management
=============================

CAN controller: my CAN-USB sniffer


Usage: steppermove [args]

Where args are:

    -0, --zeropos          set current position to zero
    -P, --pidfile=arg      pidfile (default: /tmp/steppersmng.pid)
    -S, --stop             stop motor
    -a, --abs=arg          move to absolute position (steps)
    -c, --clearerr         clear errors
    -d, --device=arg       serial device name (default: /dev/ttyUSB0)
    -h, --help             show this help
    -i, --nodeid=arg       node ID (1..127)
    -l, --logfile=arg      file to save logs
    -m, --maxspd=arg       maximal motor speed (steps per second)
    -r, --rel=arg          move to relative position (steps)
    -s, --canspd=arg       CAN bus speed (default: DEFAULT_SPEED)
    -t, --serialspd=arg    serial (tty) device speed (default: DEFAULT_SPEED)
    -u, --microsteps=arg   microstepping (0..256)
