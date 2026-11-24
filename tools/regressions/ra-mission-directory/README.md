# Red Alert mission-directory precedence

The requested mission directory must take precedence over shared campaign INIs.
Shared rules remain available before engine initialization, including when a
custom folder contains only its own mission INI. The RA archive directory stays
ahead of both to prevent caching Dawn's same-named archives.

Run with a staged runtime and its matching engine:

```sh
python3 tools/regressions/ra-mission-directory/probe.py \
  --runtime /absolute/private/runtime --exe /absolute/current/cnc_eyes \
  --out /absolute/private/results
```

The probe reuses G343's private Allied 5 fixture, adding units absent from the
installed mission. Four native object-count assertions must pass with the full
custom folder and with an INI-only folder. It writes all settings/saves/results
privately and uses the hidden background policy. `--baseline-exe` verifies that
the pre-fix build loads none of those four units, making the regression's
negative control explicit. G343/G345/G346 retain the full visual and behavior
checks; this smaller probe isolates file precedence and the shared-rules fallback.
