# Car Hotspot 0.4.6

Only a behaviour fix, found by reading the app's own diagnostics log on a real phone.

## The paging budget was being refilled by screen wakes

0.4.5 caps phone-initiated paging of the car at three attempts per cycle (15 s → 30 s → 60 s),
then monitors passively until a Bluetooth power change, a display wake, a real connect/disconnect,
an action in the app or ten minutes of quiet re-arms it. On the device the log showed the opposite:

```
[16:50:05] Paging CAR MA7 (attempt 2/3 this cycle, radio duty ~10s)
[16:50:33] Paging CAR MA7 (attempt 1/3 this cycle, radio duty ~10s)
[16:50:41] Paging CAR MA7 (attempt 1/3 this cycle, radio duty ~10s)
```

A display wake resets both the attempt count and the back-off, and wakes are not rare — charging,
notifications and glancing at the clock all produce one. The result was a BR/EDR page every 10–30 s
on the Mediatek combo controller that `com.android.bluetooth` also uses, which is precisely the load
the budget exists to avoid.

Now a wake (or an unrelated setting change, including repeated slider saves) goes through
`maybeRearmAutoConnect()`: it refills the attempts only when the previous burst is at least ten
minutes old, and it never skips the back-off already running. Picking or clearing a car still resets
everything and pages immediately — that is a genuine "the car is right here" signal. Every re-arm
says why on the Diagnostics page:

```
[17:12:04] Car paging re-armed (screen wake, 10 min since the last burst)
```

Worst case paging duty is therefore ~30 s of page per 10 minutes (~5 %), plus one burst when you
change the car selection — instead of one page every 10–30 s.
