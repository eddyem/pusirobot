Command line motor management
=============================

Works with [my CAN-USB sniffer](https://github.com/eddyem/stm32samples/tree/master/F0-nolib/usbcan).


Usage: steppermove [args]

Where args are:

    -0, --zeropos          set current position to zero
    -D, --disable          disable motor
    -E, --enablesw         enable end-switches 1 and 2
    -I, --nodeid=arg       node ID (1..127)
    -P, --pidfile=arg      pidfile (default: /tmp/steppersmng.pid)
    -R, --readvals         read values of used parameters
    -S, --stop             stop motor
    -a, --abs=arg          move to absolute position (steps)
    -c, --clearerr         clear errors
    -d, --device=arg       serial device name (default: /dev/ttyUSB0)
    -h, --help             show this help
    -k, --check=arg        check SDO data file
    -l, --logfile=arg      file to save logs
    -m, --maxspd=arg       maximal motor speed (steps per second)
    -p, --parse=arg        file with SDO data to send to device
    -q, --quick            directly send command without getting status
    -r, --rel=arg          move to relative position (steps)
    -s, --canspd=arg       CAN bus speed
    -t, --serialspd=arg    serial (tty) device speed (default: 115200)
    -u, --microsteps=arg   microstepping (0..256)
    -w, --wait             wait while motor is busy


## Some usefull information

Factory settings of pusirobot drivers: 125kBaud, nodeID=5
