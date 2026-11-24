# Foundation regressions

Compile and run the connected-platform and continuous-ground checks:

```sh
c++ -std=c++14 -O2 -fsanitize=address,undefined tools/regressions/foundations/foundations.cpp -o /tmp/cnc3d-foundations
/tmp/cnc3d-foundations
```

Run hidden native construction and campaign save/load checks with a complete
private runtime and the installation supplying original maps and paired engines:

```sh
python3 tools/regressions/foundations/probe.py \
  --exe /absolute/candidate/cnc_eyes \
  --runtime /absolute/private-runtime \
  --installation /absolute/installed-playable \
  --out /absolute/private-results \
  --expect-connected --save-load --capture
```

The fixtures preserve original standing building geometry and raw map data.
Under the user’s 3 November policy, touching foundations may adjust height to
share a platform; separated pads retain their own source levels. Every exposed
edge must be continuous, with zero vertical faces in colour, normals and depth.
The older `--expect-stable` flag remains a diagnostic for the superseded policy;
it is not a current acceptance requirement.

The fixtures’
their player ownership, initial observer/start waypoint, tech/limits and mission
automation are isolated for construction. Money, instant construction,
invulnerability and fog switches affect only this private test. Placement still
uses the normal sidebar and legal-cell overlay. Test settings, images, logs and
saves go under `--out`; the explicit save directory is created before calling
the engine. No window is shown or focused.

`--save-load` verifies the saved world after constructing an additional building
in an abandoned timeline. Geometry and height checks, original G239, GPU water
ownership, prior gameplay regressions, imported-map startup and native Windows
verification remain separate evidence. See `docs/terrain-foundations.md`.
