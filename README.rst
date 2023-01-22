Low Frequency Brute Forcer
==========================

One day this'll grow up into a real tool for LF RFID on the Flipper
Zero.

For now it just lets you select an LF protocol and enter raw data to
send, which is at least useful for enumerating through tag serial
numbers.

It also crashes a bit.

TODO
----

* Hook up a debugger and see what's causing the NULL dereferences
* Load an existing savefile to seed ``state.data``
* Less shitty UI
* Allow direct editing of FC/ID/etc fields rather than just the raw
  data, when I can be bothered reversing the render_data process for
  all protocols
