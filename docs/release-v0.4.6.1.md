# Car Hotspot 0.4.6.1

Second pass on the radio load, again from the app's own Diagnostics log on a real phone.

## What the log said after 0.4.6 ran overnight

```
[09:29:46] Paging CAR MA7 (attempt 1/3 this cycle, radio duty ~10s)
[09:31:04] Paging CAR MA7 (attempt 2/3 this cycle, radio duty ~10s)
[09:32:13] Paging CAR MA7 (attempt 3/3 this cycle, radio duty ~10s)
[09:32:13] Car paging stopped after 3 attempts - monitoring passively, ...
[09:42:36] Paging CAR MA7 (attempt 1/3 this cycle, radio duty ~10s)
```

The screen-wake storm was gone - three attempts per window, windows kept apart by the ten-minute
timer. But the car was not in range for hours and the phone kept paging it anyway: 18 pages an hour,
about 30 seconds of BR/EDR paging every hour on the combo controller `com.android.bluetooth` shares.

## Paging is a fallback, not the way we find the car

A car kit that is switched on connects to the phone **by itself**; the phone does not have to look for
it. When that happens BlueZ emits `PropertiesChanged` for `org.bluez.Device1.Connected`, D-Bus wakes
this process, and the app turns the hotspot on. That path is the normal one and it costs the radio
nothing. Paging only matters for kits that never initiate, and it is what the budget exists to limit:
one page holds the controller for roughly ten seconds.

So in 0.4.6.1:

- a burst is **2** page attempts (was 3), still backed off 15 s then 30 s;
- after a burst finds no car the quiet window **grows**: 30 min, then 1 h, then 2 h at most. A real
  Bluetooth power event, a connect or disconnect, or picking a car resets it to 30 min;
- a **screen wake no longer re-arms paging**. Charging, notifications and glancing at the clock all
  wake the display, and none of them means the car is near. The state re-read still happens on a wake,
  only the page does not;
- nothing is paged while the hotspot is up or coming up, and nothing is paged when *Auto Connect to
  Car* is off.

Worst case with the phone idle and the car absent is now 2 pages every two hours instead of 18 an hour.

## Advanced option: `pageOnWake`

For a kit that only ever connects when the phone asks, add this to
`~/.config/harbour-carhotspot/harbour-carhotspot/carhotspot.ini` under `[General]`:

```ini
pageOnWake=true
```

Then a screen wake re-arms paging again, still rate-limited to one burst per fifteen minutes so wakes
cannot become a page storm. There is deliberately no toggle for it in the UI: the default is the right
answer for most cars, and the option is for the ones that are not.

## Car state now comes from the signal

`org.bluez.Device1` property changes are applied from the `PropertiesChanged` payload itself - the
new `Connected`, `Paired`, `Alias`, `Icon` and `AllowedModes` values are patched straight into the
device cache. `updateTargetStatus()` runs from the signal handler, so the hotspot starts on the signal
instead of waiting for the next watchdog tick, and no round trip to BlueZ is needed for the event that
matters most.

The periodic `GetManagedObjects` read became a 60-second safety net instead of one per tick (it used to
run every three seconds while the service was automating or a page was visible). It still covers a
missed signal, a BlueZ restart and a device appearing in the picker, and it is still asynchronous with
one in-flight call.

## Verified

- `harbour-carhotspot-0.4.6.1-1.aarch64` and `-1.armv7hl`: `Validation succeeded`, rpmlint
  `0 errors, 0 warnings`.
- Clean compile under `-Wall -W`; the only build output noise is the pre-existing
  `lrelease` duplicate-message notice for obsolete `.ts` entries.

## Version numbering

`Version: 0.4.6.1`, `Release: 1` for the store flavour and `1or` / `1sfos50or` for the OpenRepos
flavours. Upgrading from an installed `0.4.6` is a plain `rpm -Uvh`.
