Command line motor management
=============================

Works with [my CAN-USB sniffer](https://github.com/eddyem/stm32samples/tree/master/F0-nolib/usbcan).


Usage: steppermove [args]

Where args are:

  -0, --zeropos          set current position to zero
  -A, --disablesw        disable end-switches
  -D, --disable          disable motor
  -E, --enablesw=arg     enable end-switches with given mask
  -I, --nodeid=arg       node ID (1..127)
  -P, --pidfile=arg      pidfile (default: /tmp/steppersmng.pid)
  -R, --readvals         read values of used parameters
  -S, --stop             stop motor
  -a, --abs=arg          move to absolute position (in encoder ticks)
  -c, --clearerr         clear errors
  -d, --device=arg       serial device name (default: /dev/ttyUSB0)
  -h, --help             show this help
  -k, --check            check SDO data file
  -l, --logfile=arg      file to save logs
  -m, --maxspd=arg       maximal motor speed (enc ticks per second)
  -p, --parse            file[s] with SDO data to send to device
  -q, --quick            directly send command without getting status
  -r, --rel=arg          move to relative position (in encoder ticks)
  -s, --canspd=arg       CAN bus speed
  -t, --serialspd=arg    serial (tty) device speed (default: 115200)
  -u, --microsteps=arg   set microstepping (0..256)
  -v, --verbose          verbosity level for logging (each -v increases level)
  -w, --wait             wait while motor is busy


## Some usefull information

Factory settings of pusirobot drivers: 125kBaud, nodeID=5

Speed 6553.6 == 1rev/s
