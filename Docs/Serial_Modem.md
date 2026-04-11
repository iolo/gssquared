# Serial Port / Modem

On the IIgs, GSSquared emulates the serial port as well as a simple Hayes-compatible modem.

Use any normal serial communications program like ProTERM, Spectrum, or TelCom. The virtual modem will automatically.

The virtual modem allows you to "dial" an internet TCP/IP address.

Example:
```
ATDTcqbbs.ddns.net:6800
```

will connect to the Captain's Quarters BBS, at the DNS name qbbs.ddns.net, port 6800.

You can hang up by sending the Hayes "attention" sequence and the hangup command:

```
+++
ATH
```

