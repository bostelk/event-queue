Event Queue
===========

A lock-free event queue. Not for production use (experimenting) and for Windows only.

Example output:
```
...
01/22/2019 00:15:01.11 (12428): random event occured.
01/22/2019 00:15:01.11 (8340): random event occured.
01/22/2019 00:15:01.11 (6628): random event occured.
01/22/2019 00:15:01.13 (8340): random event occured.
01/22/2019 00:15:01.13 (12428): random event occured.
thread (12428): 500.959787 events/second (50.10% of max).
thread (6628): 501.140260 events/second (50.11% of max).
thread (8340): 500.984413 events/second (50.10% of max).
thread (10756): 501.025414 events/second (50.10% of max).
sum: 2004.109875 events/second (50.10% of max).
```
