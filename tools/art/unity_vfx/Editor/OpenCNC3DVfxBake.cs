// OpenCNC3DVfxBake.cs -- records a Unity particle-system prefab as data the OpenCNC3D renderer
// replays, and renders Unity's own reference frames of it.
//
// WHY RECORD RATHER THAN RE-SIMULATE. A Shuriken system's look comes from its noise
// module, its world collisions and Unity's private per-particle random streams. None of
// those can be reproduced outside Unity to the bit, so the renderer does not try: Unity
// runs the simulation here, at a fixed step, with fixed seeds, and every live particle's
// state is written out per step. What the renderer still has to get right is the part
// that is fully specified: building each particle's geometry for the current camera and
// running the material's shader. Several seeds are baked per prefab so two explosions
// in one battle do not look identical.
//
// Run from the command line (Unity must NOT have the project open):
//   Unity -batchmode -projectPath <proj> -executeMethod OpenCNC3DVfxBake.Run \
//         -vfxPrefab "Assets/.../VFX_Explosion_Generic.prefab" -vfxOut <dir> \
//         [-vfxSeeds 4] [-vfxFps 60] [-vfxSeconds 8] [-vfxRef 1] \
//         [-vfxPitch 48.7] [-vfxFov 50] [-vfxDist 72] [-vfxW 1280] [-vfxH 720] \
//         [-vfxGround 0|1] [-vfxRefEvery 2] -quit
//
// Output, all little endian:
//   <out>/meta.json           systems, renderers, materials, vertex streams, light template
//   <out>/seed<k>.bin         the recorded frames of seed k (layout in WriteFrame below)
//   <out>/mesh_<name>.bin     every mesh a mesh-mode renderer uses (layout in WriteMesh)
//   <out>/ref/<variant>_<frame>.png   Unity's own render, when -vfxRef 1
//
// FRAME RECORD. Per frame: uint32 systemCount, then per system uint32 n and n particles
// of REC_FLOATS float32 each:
//    0 seed(uint32 bits)  1-3 position  4-6 rotation3D (degrees)  7-9 current size3D
//   10-13 current colour (0..1)  14-16 total velocity  17 remaining life  18 start life
//   19-22 custom1  23-26 custom2  27-28 texture-sheet tile origin (u,v; -1 = none)
//   29 mesh index
// Positions and velocities are in the prefab root's space with the root at the origin.

using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

public static class OpenCNC3DVfxBake
{
    const int REC_FLOATS = 30;

    static string Arg(string name, string def)
    {
        var a = Environment.GetCommandLineArgs();
        for (int i = 0; i < a.Length - 1; i++) if (a[i] == name) return a[i + 1];
        return def;
    }
    static float ArgF(string n, float d) => float.Parse(Arg(n, d.ToString(System.Globalization.CultureInfo.InvariantCulture)), System.Globalization.CultureInfo.InvariantCulture);
    static int ArgI(string n, int d) => int.Parse(Arg(n, d.ToString()));

    static string F(float v) => v.ToString("R", System.Globalization.CultureInfo.InvariantCulture);
    static string V3(Vector3 v) => $"[{F(v.x)},{F(v.y)},{F(v.z)}]";
    static string Q(string s) => "\"" + s.Replace("\\", "\\\\").Replace("\"", "\\\"") + "\"";

    public static void Run()
    {
        string prefabPath = Arg("-vfxPrefab", null);
        string outDir     = Arg("-vfxOut", null);
        int seeds         = ArgI("-vfxSeeds", 4);
        int fps           = ArgI("-vfxFps", 60);
        float seconds     = ArgF("-vfxSeconds", 8f);
        bool refRender    = ArgI("-vfxRef", 1) != 0;
        int refEvery      = ArgI("-vfxRefEvery", 2);
        foreach (var t in Arg("-vfxProbe", "").Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries)) s_probe.Add(int.Parse(t));
        // A LOOPING PREFAB PLAYED ONCE. Every muzzle flash in the gun pack loops (it is made
        // for automatic fire); a single shot is one cycle of it, so -vfxOnce 1 turns the
        // loop off on every system and the recording is that one cycle and its smoke.
        bool once         = ArgI("-vfxOnce", 0) != 0;
        // A TRAIL IS RECORDED IN FLIGHT: -vfxMove "x,y,z" moves the prefab at that velocity
        // (metres a second) from the first frame, and -vfxMoveStop T stops its emitters at T
        // seconds, as a projectile's trail stops when the round lands, and lets the rest
        // live out. The particles a world-space system leaves behind are then the trail.
        var moveParts     = Arg("-vfxMove", "0,0,0").Split(',');
        var move          = new Vector3(float.Parse(moveParts[0], System.Globalization.CultureInfo.InvariantCulture),
                                        float.Parse(moveParts[1], System.Globalization.CultureInfo.InvariantCulture),
                                        float.Parse(moveParts[2], System.Globalization.CultureInfo.InvariantCulture));
        float moveStop    = ArgF("-vfxMoveStop", 1e9f);
        if (prefabPath == null || outDir == null) throw new Exception("need -vfxPrefab and -vfxOut");
        Directory.CreateDirectory(outDir);

        var scene = EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
        var cam = BuildStage();

        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(prefabPath);
        if (prefab == null) throw new Exception("no prefab at " + prefabPath);

        int frames = Mathf.CeilToInt(seconds * fps) + 1;
        float dt = 1f / fps;

        for (int s = 0; s < seeds; s++)
        {
            var go = (GameObject)PrefabUtility.InstantiatePrefab(prefab);
            go.transform.position = Vector3.zero;
            go.transform.rotation = Quaternion.identity;
            var systems = go.GetComponentsInChildren<ParticleSystem>(true);
            for (int i = 0; i < systems.Length; i++)
            {
                systems[i].Stop(false, ParticleSystemStopBehavior.StopEmittingAndClear);
                systems[i].useAutoRandomSeed = false;
                systems[i].randomSeed = (uint)(1000003 * (s + 1) + 7919 * i);
            }
            // WORLD COLLISIONS DO NOT RUN IN AN EDITOR-SIDE SIMULATE: measured, debris
            // recorded 200 m below a ground that had a collider. The ground in this stage is
            // a flat plane at y = 0 anyway, so every World collision module is pointed at
            // that plane as a Planes collision instead, keeping its bounce, damping,
            // lifetime loss and radius. The same answer Unity gives on flat ground, without
            // the physics scene.
            foreach (var ps in systems)
            {
                var col = ps.collision;
                if (!col.enabled || col.type != ParticleSystemCollisionType.World) continue;
                col.type = ParticleSystemCollisionType.Planes;
                col.SetPlane(0, s_ground);
            }
            if (once)
                foreach (var ps in systems) { var mn = ps.main; mn.loop = false; }
            // -vfxOnly "Name,Name": only those systems emit, on their own. The chaff flare's
            // smoke and sparks are sub-emitters of flares it throws; flown by themselves
            // they lay a smoke trail and a spark trail along the prefab's own flight.
            var only = Arg("-vfxOnly", "");
            if (only.Length > 0)
            {
                var keepNames = new HashSet<string>(only.Split(','));
                foreach (var ps in systems)
                {
                    var sub = ps.subEmitters; sub.enabled = false;
                    if (!keepNames.Contains(ps.name)) { var e = ps.emission; e.enabled = false; }
                    else { var mn = ps.main; mn.loop = true; }
                }
            }
            // -vfxNoLights 1: the Lights modules off, to measure what a prefab's particle
            // lights add to its own picture
            if (ArgI("-vfxNoLights", 0) != 0)
                foreach (var ps in systems) { var lm = ps.lights; lm.enabled = false; }
            if (s == 0) WriteMeta(outDir, go, systems, fps, frames, move, moveStop);

            // THE ROOTS: the systems no other system sits above. An explosion prefab has one
            // at its top; the gun pack's prefabs are an empty object with sibling systems
            // under it, and each of those is simulated with its own children.
            var roots = new List<ParticleSystem>();
            foreach (var ps in systems)
            {
                var up = ps.transform.parent;
                while (up != null && up.GetComponent<ParticleSystem>() == null) up = up.parent;
                if (up == null) roots.Add(ps);
            }
            foreach (var r0 in roots) r0.Simulate(0f, true, true, false);

            using (var bw = new BinaryWriter(File.Create(Path.Combine(outDir, $"seed{s}.bin"))))
            {
                bw.Write(Encoding.ASCII.GetBytes("C3VX"));
                bw.Write((uint)1);             // version
                bw.Write((uint)systems.Length);
                bw.Write((uint)frames);
                bw.Write((uint)fps);
                bw.Write((uint)REC_FLOATS);
                for (int f = 0; f < frames; f++)
                {
                    if (f > 0)
                    {
                        float tNow = f * dt;
                        if (move != Vector3.zero && tNow <= moveStop + 0.5f * dt)
                            go.transform.position = move * Mathf.Min(tNow, moveStop);
                        if (tNow >= moveStop && tNow < moveStop + dt)
                            foreach (var r0 in roots) r0.Stop(true, ParticleSystemStopBehavior.StopEmitting);
                        foreach (var r0 in roots) r0.Simulate(dt, true, false, false);
                    }
                    WriteFrame(bw, systems, cam);
                    if (s == 0 && s_probe.Contains(f)) WriteProbe(outDir, systems, cam, f);
                    if (refRender && s == 0 && (f % refEvery) == 0)
                    {
                        RenderRef(cam, Path.Combine(outDir, "ref"), f);
                    }
                }
            }
            UnityEngine.Object.DestroyImmediate(go);
            Debug.Log($"OpenCNC3DVfxBake: seed {s} done, {frames} frames");
        }
        Debug.Log("OpenCNC3DVfxBake: finished " + outDir);
    }

    // ------------------------------------------------------------------ the library
    // THE EXPLOSION PICKER'S RIGHT-HAND SIDE. Every prefab in a list, rendered as Unity
    // renders it (Camera.Render into a float target, the pack's post-processing), from
    // the game's own closest camera at the game's scale, and cropped to the same box the
    // game's own explosions are recorded in, so the two can be compared side by side.
    //   -executeMethod OpenCNC3DVfxBake.RenderLibrary -vfxList <file> -vfxOut <dir>
    //     [-vfxPitch 44.691 -vfxDist 28.41 -vfxW 1280 -vfxH 800 -vfxBox 360,40,560,560]
    // Per prefab writes <out>/<name>.rgb (raw RGB24 frames, top row first, 30 a second)
    // and <out>/<name>.txt (frame count, size).
    public static void RenderLibrary()
    {
        string outDir = Arg("-vfxOut", null);
        Directory.CreateDirectory(outDir);
        var list = File.ReadAllLines(Arg("-vfxList", null));
        int[] box = Array.ConvertAll(Arg("-vfxBox", "360,40,560,560").Split(','), int.Parse);
        float maxSec = ArgF("-vfxMaxSeconds", 6f);
        const float dt = 1f / 30f;
        foreach (var raw in list)
        {
            string path = raw.Trim();
            if (path.Length == 0) continue;
            EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
            var cam = BuildStage();
            // the ground this time is seen: a dark grass colour, lit, so the flash lands on it
            var g = s_ground.GetComponent<MeshRenderer>();
            g.enabled = true;
            var gm = g.sharedMaterial;
            gm.SetColor("_BaseColor", new Color(ArgF("-vfxGroundR", 0.055f), ArgF("-vfxGroundG", 0.075f), ArgF("-vfxGroundB", 0.035f), 1f));
            // matte: the demo sun shines towards the camera, and any gloss mirrors it into the lens
            // (smoothness alone: switching the specular and reflection keywords on as well
            // rendered the ground black in batch mode, measured)
            gm.SetFloat("_Smoothness", 0f);
            var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(path);
            if (prefab == null) { Debug.Log("OpenCNC3DVfxBake: no prefab " + path); continue; }
            var go = (GameObject)PrefabUtility.InstantiatePrefab(prefab);
            string name = Path.GetFileNameWithoutExtension(path);
            bool aerial = name.Contains("Aerial") || name.Contains("Chaff");
            go.transform.position = new Vector3(0, aerial ? 3f : 0f, 0);
            var systems = go.GetComponentsInChildren<ParticleSystem>(true);
            for (int i = 0; i < systems.Length; i++)
            {
                systems[i].Stop(false, ParticleSystemStopBehavior.StopEmittingAndClear);
                systems[i].useAutoRandomSeed = false;
                systems[i].randomSeed = (uint)(1000003 + 7919 * i);
                var col = systems[i].collision;
                if (col.enabled && col.type == ParticleSystemCollisionType.World)
                {
                    col.type = ParticleSystemCollisionType.Planes;
                    col.SetPlane(0, s_ground);
                }
            }
            var root = go.GetComponent<ParticleSystem>();
            if (root == null) { Debug.Log("OpenCNC3DVfxBake: no particle system at the root of " + name); continue; }
            root.Simulate(0f, true, true, false);
            var rt = cam.targetTexture;
            var rd = new Texture2D(box[2], box[3], TextureFormat.RGBAFloat, false, true);
            int frames = 0, quiet = 0;
            var bytes = new byte[box[2] * box[3] * 3];
            using (var bw = new BinaryWriter(File.Create(Path.Combine(outDir, name + ".rgb"))))
            {
                for (float t = 0f; t <= maxSec; t += dt)
                {
                    if (frames > 0) root.Simulate(dt, true, false, false);
                    cam.Render();
                    RenderTexture.active = rt;
                    // Unity's y runs up; the box is given top-down like the game's frames
                    rd.ReadPixels(new Rect(box[0], rt.height - box[1] - box[3], box[2], box[3]), 0, 0);
                    RenderTexture.active = null;
                    var px = rd.GetPixels();
                    for (int y = 0; y < box[3]; y++)
                    {
                        int src = (box[3] - 1 - y) * box[2];
                        for (int x = 0; x < box[2]; x++)
                        {
                            var c = px[src + x];
                            int o = (y * box[2] + x) * 3;
                            bytes[o] = Enc(c.r); bytes[o + 1] = Enc(c.g); bytes[o + 2] = Enc(c.b);
                        }
                    }
                    bw.Write(bytes);
                    frames++;
                    // stop a second after the last particle, looping prefabs at the cap
                    quiet = root.IsAlive(true) ? 0 : quiet + 1;
                    if (t > 0.5f && quiet > 30) break;
                }
            }
            File.WriteAllText(Path.Combine(outDir, name + ".txt"), $"{frames} {box[2]} {box[3]}\n");
            Debug.Log($"OpenCNC3DVfxBake: library {name}: {frames} frames");
            UnityEngine.Object.DestroyImmediate(rd);
        }
    }

    // ------------------------------------------------------------------ the tonemap
    // UNITY'S TONEMAP, MEASURED RATHER THAN PORTED. URP's ACES path is a chain of colour
    // space changes, a glow module, a red modifier, a fitted RRT+ODT and a desaturation,
    // and URP itself never runs that chain per pixel: it bakes it into a 32^3 LUT indexed
    // in LogC and samples that. So does this: a 1024x32 strip of known HDR colours (the
    // LUT's own grid, LogC encoded, blue in slices across) is drawn unlit through the
    // pack's post-processing with tonemapping as the ONLY effect, and read back. The
    // renderer samples the result the way URP samples its own.
    //   -executeMethod OpenCNC3DVfxBake.BakeTonemap -vfxOut <dir>
    // writes <dir>/tonemap_lut.bin: "C3LT", uint32 N, then N^3 x RGB float32, red fastest.
    /* SHADER GRAPH SOURCE. A graph's own JSON says what its nodes are but not what URP
       wraps around them (blend state, premultiplication, the unlit pass), so the port
       reads the shader text Shader Graph itself generates. The generator is internal to
       the package; this reaches it by reflection.
         -sgList "<asset path>;<asset path>"   -sgOut <dir> */
    public static void DumpShaderGraphs()
    {
        string outDir = Arg("-sgOut", "sgout");
        Directory.CreateDirectory(outDir);
        Type imp = Type.GetType("UnityEditor.ShaderGraph.ShaderGraphImporter, Unity.ShaderGraph.Editor");
        System.Reflection.MethodInfo get = null;
        foreach (var m in imp.GetMethods(System.Reflection.BindingFlags.Static | System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Public))
            if (m.Name == "GetShaderText" && m.GetParameters().Length == 4 && m.GetParameters()[3].IsOut) get = m;
        foreach (string path in Arg("-sgList", "").Split(';')) {
            if (path.Length == 0) continue;
            object[] a = new object[] { path, null, null, null };
            string text = (string)get.Invoke(null, a);
            File.WriteAllText(Path.Combine(outDir, Path.GetFileNameWithoutExtension(path) + ".shader"), text);
            Debug.Log("SG|" + path + "|" + text.Length);
        }
    }

    public static void BakeTonemap()
    {
        string outDir = Arg("-vfxOut", null);
        Directory.CreateDirectory(outDir);
        const int N = 32;
        EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
        // Full float for the camera colour so the readback measures the curve, not the
        // default 32 bit HDR buffer's rounding of the grid.
        var rp = GraphicsSettings.currentRenderPipeline as UniversalRenderPipelineAsset;
        if (rp != null) rp.hdrColorBufferPrecision = HDRColorBufferPrecision._64Bits;

        var src = new Texture2D(N * N, N, TextureFormat.RGBAFloat, false, true);
        src.filterMode = FilterMode.Point;
        src.wrapMode = TextureWrapMode.Clamp;
        var px = new Color[N * N * N];
        for (int b = 0; b < N; b++) for (int g = 0; g < N; g++) for (int r = 0; r < N; r++)
            px[g * N * N + b * N + r] = new Color(LogCToLinear(r / (N - 1f)), LogCToLinear(g / (N - 1f)), LogCToLinear(b / (N - 1f)), 1f);
        src.SetPixels(px);
        src.Apply();

        var quad = GameObject.CreatePrimitive(PrimitiveType.Quad);
        quad.transform.position = new Vector3(N * N * 0.5f, N * 0.5f, 0f);
        quad.transform.localScale = new Vector3(N * N, N, 1f);
        var mat = new Material(Shader.Find("Universal Render Pipeline/Unlit"));
        mat.SetTexture("_BaseMap", src);
        mat.SetColor("_BaseColor", Color.white);
        quad.GetComponent<MeshRenderer>().sharedMaterial = mat;

        var prof = ScriptableObject.CreateInstance<VolumeProfile>();
        var tm = prof.Add<Tonemapping>(true);
        tm.mode.Override(TonemappingMode.ACES);
        var vol = new GameObject("Vol").AddComponent<Volume>();
        vol.isGlobal = true;
        vol.sharedProfile = prof;

        var cam = new GameObject("Cam").AddComponent<Camera>();
        cam.orthographic = true;
        cam.orthographicSize = N * 0.5f;
        cam.transform.position = new Vector3(N * N * 0.5f, N * 0.5f, -10f);
        cam.clearFlags = CameraClearFlags.SolidColor;
        cam.backgroundColor = Color.black;
        cam.allowHDR = true;
        cam.allowMSAA = false;
        var data = cam.gameObject.AddComponent<UniversalAdditionalCameraData>();
        data.renderPostProcessing = true;
        data.antialiasing = AntialiasingMode.None;
        data.dithering = false;
        var rt = new RenderTexture(N * N, N, 24, RenderTextureFormat.ARGBFloat, RenderTextureReadWrite.Linear);
        cam.targetTexture = rt;
        cam.aspect = N;

        Debug.Log($"OpenCNC3DVfxBake: pipeline {(rp != null ? rp.name : "none")} hdr={(rp != null && rp.supportsHDR)} quality={QualitySettings.names[QualitySettings.GetQualityLevel()]} path={Arg("-vfxPath", "request")}");
        if (Arg("-vfxPath", "request") == "render") cam.Render();
        else
        {
            var req = new UniversalRenderPipeline.SingleCameraRequest { destination = rt };
            if (RenderPipeline.SupportsRenderRequest(cam, req)) RenderPipeline.SubmitRenderRequest(cam, req);
            else { Debug.Log("OpenCNC3DVfxBake: render request unsupported, using Camera.Render"); cam.Render(); }
        }
        RenderTexture.active = rt;
        var back = new Texture2D(N * N, N, TextureFormat.RGBAFloat, false, true);
        back.ReadPixels(new Rect(0, 0, N * N, N), 0, 0);
        back.Apply();
        RenderTexture.active = null;
        var o = back.GetPixels();
        using (var bw = new BinaryWriter(File.Create(Path.Combine(outDir, "tonemap_lut.bin"))))
        {
            bw.Write(Encoding.ASCII.GetBytes("C3LT"));
            bw.Write((uint)N);
            for (int b = 0; b < N; b++) for (int g = 0; g < N; g++) for (int r = 0; r < N; r++)
            {
                var c = o[g * N * N + b * N + r];
                bw.Write(c.r); bw.Write(c.g); bw.Write(c.b);
            }
        }
        // two spot checks into the log, so a run that silently rendered black says so
        Debug.Log($"OpenCNC3DVfxBake: tonemap LUT written; grey 0.18 -> {o[16 * N * N + 16 * N + 16]}, white -> {o[(N - 1) * N * N + (N - 1) * N + (N - 1)]}");
    }

    // URP's LogC (ARRI EI800), the encoding its internal grading LUT is indexed in.
    static float LogCToLinear(float x)
    {
        const float cut = 0.011361f, a = 5.555556f, b = 0.047996f, c = 0.244161f, d = 0.386036f, e = 5.301883f, f = 0.092819f;
        return x > e * cut + f ? (Mathf.Pow(10f, (x - d) / c) - b) / a : (x - f) / e;
    }

    // ------------------------------------------------------------------ the stage
    // The pack's own demo conditions: one white directional light at 3.14, flat black
    // ambient, its post-processing profile (bloom threshold 1, ACES). A ground collider
    // at y = 0 for the debris and sparks to land on, visible only with -vfxGround 1.
    static Transform s_ground;
    static Camera BuildStage()
    {
        RenderSettings.ambientMode = AmbientMode.Flat;
        RenderSettings.ambientLight = Color.black;
        RenderSettings.skybox = null;
        RenderSettings.fog = false;

        var sun = new GameObject("Sun").AddComponent<Light>();
        sun.type = LightType.Directional;
        sun.color = Color.white;
        sun.intensity = 3.14f;
        sun.shadows = LightShadows.None;
        sun.transform.rotation = Quaternion.Euler(ArgF("-vfxSunX", 135f), ArgF("-vfxSunY", 0f), 0f);

        var ground = GameObject.CreatePrimitive(PrimitiveType.Plane);
        ground.name = "Ground";
        s_ground = ground.transform;
        ground.transform.localScale = new Vector3(100, 1, 100);
        var gm = new Material(Shader.Find("Universal Render Pipeline/Lit"));
        gm.SetColor("_BaseColor", new Color(0.25f, 0.25f, 0.25f, 1));
        ground.GetComponent<MeshRenderer>().sharedMaterial = gm;
        ground.GetComponent<MeshRenderer>().enabled = ArgI("-vfxGround", 0) != 0;

        var volGo = new GameObject("PostVolume");
        var vol = volGo.AddComponent<Volume>();
        vol.isGlobal = true;
        // THE STAGE'S POST IS THE PROFILE'S AND NOTHING THE PROJECT PICKED UP ON THE WAY.
        // URP stacks the project's default volume profile under every camera, and importing
        // the gun pack replaced that default (Assets/Settings/DefaultVolumeProfile.asset
        // ships inside it) with one carrying +0.5 EV of post exposure: measured, the gun
        // pack's reference frames came out exactly sqrt(2) brighter than the explosions'
        // did before the import. Post exposure is pinned at 0 here, so a reference frame
        // does not depend on which package was imported last.
        var baseProfile = AssetDatabase.LoadAssetAtPath<VolumeProfile>(
            Arg("-vfxProfile", "Assets/Vefects/Explosions VFX URP/Demo/Resources/PP/Vefects_Explosion_VFX_Post_Processing.asset"));
        var profile = UnityEngine.Object.Instantiate(baseProfile);
        if (!profile.TryGet<ColorAdjustments>(out var adj)) adj = profile.Add<ColorAdjustments>(true);
        adj.postExposure.Override(0f);
        vol.sharedProfile = profile;

        var camGo = new GameObject("RefCam");
        var cam = camGo.AddComponent<Camera>();
        cam.clearFlags = CameraClearFlags.SolidColor;
        cam.backgroundColor = Color.black;
        cam.fieldOfView = ArgF("-vfxFov", 50f);
        cam.nearClipPlane = 0.5f;
        cam.farClipPlane = 1000f;
        cam.allowHDR = true;
        cam.allowMSAA = false;
        float pitch = ArgF("-vfxPitch", 48.7f), yaw = ArgF("-vfxYaw", 0f), dist = ArgF("-vfxDist", 72f);
        var rot = Quaternion.Euler(pitch, yaw, 0f);
        cam.transform.rotation = rot;
        cam.transform.position = new Vector3(0, ArgF("-vfxAtY", 0f), 0) - rot * Vector3.forward * dist;
        var data = camGo.AddComponent<UniversalAdditionalCameraData>();
        data.renderPostProcessing = ArgI("-vfxPost", 1) != 0;
        data.requiresDepthTexture = true;
        data.requiresColorTexture = true;
        data.antialiasing = AntialiasingMode.None;
        int w = ArgI("-vfxW", 1280), h = ArgI("-vfxH", 720);
        // A HALF FLOAT TARGET. Measured: with an eight bit sRGB target the camera renders
        // in low range, clips the fireball at 1.0 before bloom sees it, and the frame comes
        // out with no bloom and a 214 ceiling. The float target keeps URP's intermediate
        // in high range; the sRGB encode is done on readback instead.
        cam.targetTexture = new RenderTexture(w, h, 24, RenderTextureFormat.ARGBHalf, RenderTextureReadWrite.Linear);
        return cam;
    }

    // -vfxRaw 1 also writes the frame WITHOUT post-processing as raw float (<variant>_<frame>.f32:
    // uint32 w, h, then w*h RGBA float32, bottom row first), so the renderer's port of
    // the bloom and the tonemap can be checked against Unity's own post-processed PNG.
    static void RenderRef(Camera cam, string dir, int frame)
    {
        Directory.CreateDirectory(dir);
        if (ArgI("-vfxRaw", 0) != 0)
        {
            var d = cam.GetComponent<UniversalAdditionalCameraData>();
            d.renderPostProcessing = false;
            cam.Render();
            var prv = RenderTexture.active;
            RenderTexture.active = cam.targetTexture;
            var t = new Texture2D(cam.targetTexture.width, cam.targetTexture.height, TextureFormat.RGBAFloat, false, true);
            t.ReadPixels(new Rect(0, 0, t.width, t.height), 0, 0);
            RenderTexture.active = prv;
            using (var bw = new BinaryWriter(File.Create(Path.Combine(dir, $"{Arg("-vfxVariant", "ref")}_{frame:D4}.f32"))))
            {
                bw.Write((uint)t.width); bw.Write((uint)t.height);
                bw.Write(t.GetRawTextureData());
            }
            UnityEngine.Object.DestroyImmediate(t);
            d.renderPostProcessing = true;
        }
        var rt = cam.targetTexture;
        // Camera.Render, NOT a SingleCameraRequest: measured, a render request skips the
        // post-processing stack entirely and hands back an eight bit, untonemapped frame.
        if (Arg("-vfxPath", "render") == "render") cam.Render();
        else
        {
            var req = new UniversalRenderPipeline.SingleCameraRequest { destination = rt };
            RenderPipeline.SubmitRenderRequest(cam, req);
        }
        var prev = RenderTexture.active;
        RenderTexture.active = rt;
        var f = new Texture2D(rt.width, rt.height, TextureFormat.RGBAFloat, false, true);
        f.ReadPixels(new Rect(0, 0, rt.width, rt.height), 0, 0);
        f.Apply();
        RenderTexture.active = prev;
        var px = f.GetPixels();
        var tex = new Texture2D(rt.width, rt.height, TextureFormat.RGB24, false, false);
        var o = new Color32[px.Length];
        float maxv = 0f;
        for (int i = 0; i < px.Length; i++)
        {
            var c = px[i];
            maxv = Mathf.Max(maxv, Mathf.Max(c.r, Mathf.Max(c.g, c.b)));
            o[i] = new Color32(Enc(c.r), Enc(c.g), Enc(c.b), 255);
        }
        tex.SetPixels32(o);
        tex.Apply();
        UnityEngine.Object.DestroyImmediate(f);
        if (frame % 30 == 0) Debug.Log($"OpenCNC3DVfxBake: ref frame {frame} peak {maxv}");
        File.WriteAllBytes(Path.Combine(dir, $"{Arg("-vfxVariant", "ref")}_{frame:D4}.png"), tex.EncodeToPNG());
        UnityEngine.Object.DestroyImmediate(tex);
    }

    static byte Enc(float v)
    {
        v = Mathf.Clamp01(v);
        v = v <= 0.0031308f ? v * 12.92f : 1.055f * Mathf.Pow(v, 1f / 2.4f) - 0.055f;
        return (byte)Mathf.Clamp(Mathf.RoundToInt(v * 255f), 0, 255);
    }

    // ------------------------------------------------------------------ frames
    static ParticleSystem.Particle[] s_buf = new ParticleSystem.Particle[4096];
    static List<Vector4> s_c1 = new List<Vector4>(), s_c2 = new List<Vector4>();
    static Mesh s_bake;

    static void WriteFrame(BinaryWriter bw, ParticleSystem[] systems, Camera cam)
    {
        bw.Write((uint)systems.Length);
        foreach (var ps in systems)
        {
            int n = ps.GetParticles(s_buf);
            ps.GetCustomParticleData(s_c1, ParticleSystemCustomData.Custom1);
            ps.GetCustomParticleData(s_c2, ParticleSystemCustomData.Custom2);
            var uvs = TileOrigins(ps, n, cam);
            // GetMeshIndex divides by the renderer's mesh count and takes the editor down
            // with it when that is zero, so it is only asked of a mesh renderer.
            var pr = ps.GetComponent<ParticleSystemRenderer>();
            bool meshy = pr != null && pr.renderMode == ParticleSystemRenderMode.Mesh && pr.meshCount > 1;
            bw.Write((uint)n);
            for (int i = 0; i < n; i++)
            {
                var p = s_buf[i];
                bw.Write(BitConverter.ToSingle(BitConverter.GetBytes(p.randomSeed), 0));
                W3(bw, p.position);
                W3(bw, p.rotation3D);
                W3(bw, p.GetCurrentSize3D(ps));
                Color c = p.GetCurrentColor(ps);
                bw.Write(c.r); bw.Write(c.g); bw.Write(c.b); bw.Write(c.a);
                W3(bw, p.totalVelocity);
                bw.Write(p.remainingLifetime);
                bw.Write(p.startLifetime);
                W4(bw, i < s_c1.Count ? s_c1[i] : Vector4.zero);
                W4(bw, i < s_c2.Count ? s_c2[i] : Vector4.zero);
                bw.Write(uvs != null ? uvs[i].x : -1f);
                bw.Write(uvs != null ? uvs[i].y : -1f);
                bw.Write(meshy ? (float)p.GetMeshIndex(ps) : 0f);
            }
        }
    }

    // THE TEXTURE-SHEET TILE, which Unity does not expose per particle. Every system in
    // this pack that animates a sheet picks one random tile at birth and holds it, so the
    // tile is read back off the renderer's own baked quads: four vertices per particle,
    // in particle order when the renderer does not sort. Checked, not assumed: a count
    // that does not come out at four per particle stops the bake.
    static Vector2[] TileOrigins(ParticleSystem ps, int n, Camera cam)
    {
        var tsa = ps.textureSheetAnimation;
        var r = ps.GetComponent<ParticleSystemRenderer>();
        if (!tsa.enabled || n == 0 || r == null) return null;
        // a system that draws nothing (the gun pack's light carriers) has no quads to read
        if (!r.enabled || r.renderMode == ParticleSystemRenderMode.None) return null;
        if (r.renderMode == ParticleSystemRenderMode.Mesh) throw new Exception(ps.name + ": texture sheet on a mesh renderer is not handled");
        if (r.sortMode != ParticleSystemSortMode.None) throw new Exception(ps.name + ": sorted renderer, baked quad order would not match particle order");
        if (s_bake == null) s_bake = new Mesh();
        r.BakeMesh(s_bake, cam, ParticleSystemBakeMeshOptions.Default);
        var uv = s_bake.uv;
        if (uv.Length != n * 4) throw new Exception($"{ps.name}: baked {uv.Length} vertices for {n} particles");
        var o = new Vector2[n];
        for (int i = 0; i < n; i++)
        {
            float u = float.MaxValue, v = float.MaxValue;
            for (int k = 0; k < 4; k++) { u = Mathf.Min(u, uv[i * 4 + k].x); v = Mathf.Min(v, uv[i * 4 + k].y); }
            o[i] = new Vector2(u, v);
        }
        return o;
    }

    // PROBE: Unity's own geometry for one frame, for checking the renderer's reconstruction
    // against rather than trusting it. Per system: "C3VP", uint32 vertexCount, indexCount,
    // then per vertex position(3) normal(3) colour(4) uv0..uv3 (4 each), then indices.
    static HashSet<int> s_probe = new HashSet<int>();
    static void WriteProbe(string outDir, ParticleSystem[] systems, Camera cam, int frame)
    {
        string dir = Path.Combine(outDir, "probe");
        Directory.CreateDirectory(dir);
        for (int i = 0; i < systems.Length; i++)
        {
            var r = systems[i].GetComponent<ParticleSystemRenderer>();
            if (r != null && r.enabled && systems[i].trails.enabled) WriteTrailProbe(dir, r, cam, frame, i);
            if (r == null || !r.enabled || r.renderMode == ParticleSystemRenderMode.None) continue;
            var m = new Mesh();
            r.BakeMesh(m, cam, ParticleSystemBakeMeshOptions.Default);
            int vc = m.vertexCount;
            var pos = m.vertices; var nrm = m.normals; var col = m.colors;
            var uvs = new List<Vector4>[4];
            for (int c = 0; c < 4; c++) { uvs[c] = new List<Vector4>(); m.GetUVs(c, uvs[c]); }
            var idx = m.triangles;
            using (var bw = new BinaryWriter(File.Create(Path.Combine(dir, $"f{frame:D4}_s{i:D2}.bin"))))
            {
                bw.Write(Encoding.ASCII.GetBytes("C3VP"));
                bw.Write((uint)vc); bw.Write((uint)idx.Length);
                for (int v = 0; v < vc; v++)
                {
                    W3(bw, pos[v]);
                    W3(bw, nrm.Length == vc ? nrm[v] : Vector3.zero);
                    Color c = col.Length == vc ? col[v] : Color.white;
                    bw.Write(c.r); bw.Write(c.g); bw.Write(c.b); bw.Write(c.a);
                    for (int ch = 0; ch < 4; ch++) W4(bw, uvs[ch].Count == vc ? uvs[ch][v] : Vector4.zero);
                }
                foreach (var ix in idx) bw.Write((uint)ix);
            }
            UnityEngine.Object.DestroyImmediate(m);
        }
        var root = systems.Length > 0 ? systems[0].transform.root : null;
        if (root != null)
        {
            var trs = root.GetComponentsInChildren<TrailRenderer>(true);
            for (int k = 0; k < trs.Length; k++)
            {
                var m = new Mesh();
                trs[k].BakeMesh(m, cam, true);
                WriteMeshProbe(Path.Combine(dir, $"f{frame:D4}_line{k:D2}.bin"), m);
                UnityEngine.Object.DestroyImmediate(m);
                // and the points it was built from, with their ages
                var pts = new Vector3[trs[k].positionCount];
                trs[k].GetPositions(pts);
                using (var bw = new BinaryWriter(File.Create(Path.Combine(dir, $"f{frame:D4}_line{k:D2}_pts.bin"))))
                {
                    bw.Write((uint)pts.Length);
                    foreach (var q in pts) W3(bw, q);
                }
            }
        }
        // the camera the probe was baked for, so the check can rebuild with the same one
        File.WriteAllText(Path.Combine(dir, $"f{frame:D4}_camera.json"),
            "{\"position\":" + V3(cam.transform.position) + ",\"forward\":" + V3(cam.transform.forward) +
            ",\"up\":" + V3(cam.transform.up) + ",\"right\":" + V3(cam.transform.right) + "}\n");
    }

    static void WriteMeshProbe(string path, Mesh m)
    {
        int vc = m.vertexCount;
        var pos = m.vertices; var nrm = m.normals; var col = m.colors;
        var uvs = new List<Vector4>[4];
        for (int c = 0; c < 4; c++) { uvs[c] = new List<Vector4>(); m.GetUVs(c, uvs[c]); }
        var idx = m.triangles;
        using (var bw = new BinaryWriter(File.Create(path)))
        {
            bw.Write(Encoding.ASCII.GetBytes("C3VP"));
            bw.Write((uint)vc); bw.Write((uint)idx.Length);
            for (int v = 0; v < vc; v++)
            {
                W3(bw, pos[v]);
                W3(bw, nrm.Length == vc ? nrm[v] : Vector3.zero);
                Color c = col.Length == vc ? col[v] : Color.white;
                bw.Write(c.r); bw.Write(c.g); bw.Write(c.b); bw.Write(c.a);
                for (int ch = 0; ch < 4; ch++) W4(bw, uvs[ch].Count == vc ? uvs[ch][v] : Vector4.zero);
            }
            foreach (var ix in idx) bw.Write((uint)ix);
        }
    }

    // Unity's own trail geometry for one frame, in the same layout as a particle probe.
    static void WriteTrailProbe(string dir, ParticleSystemRenderer r, Camera cam, int frame, int i)
    {
        var m = new Mesh();
        r.BakeTrailsMesh(m, cam, ParticleSystemBakeMeshOptions.Default);
        int vc = m.vertexCount;
        var pos = m.vertices; var nrm = m.normals; var col = m.colors;
        var uvs = new List<Vector4>[4];
        for (int c = 0; c < 4; c++) { uvs[c] = new List<Vector4>(); m.GetUVs(c, uvs[c]); }
        var idx = m.triangles;
        using (var bw = new BinaryWriter(File.Create(Path.Combine(dir, $"f{frame:D4}_s{i:D2}_trail.bin"))))
        {
            bw.Write(Encoding.ASCII.GetBytes("C3VP"));
            bw.Write((uint)vc); bw.Write((uint)idx.Length);
            for (int v = 0; v < vc; v++)
            {
                W3(bw, pos[v]);
                W3(bw, nrm.Length == vc ? nrm[v] : Vector3.zero);
                Color c = col.Length == vc ? col[v] : Color.white;
                bw.Write(c.r); bw.Write(c.g); bw.Write(c.b); bw.Write(c.a);
                for (int ch = 0; ch < 4; ch++) W4(bw, uvs[ch].Count == vc ? uvs[ch][v] : Vector4.zero);
            }
            foreach (var ix in idx) bw.Write((uint)ix);
        }
        UnityEngine.Object.DestroyImmediate(m);
    }

    static void W3(BinaryWriter bw, Vector3 v) { bw.Write(v.x); bw.Write(v.y); bw.Write(v.z); }
    static void W4(BinaryWriter bw, Vector4 v) { bw.Write(v.x); bw.Write(v.y); bw.Write(v.z); bw.Write(v.w); }

    // ------------------------------------------------------------------ meta
    static void WriteMeta(string outDir, GameObject go, ParticleSystem[] systems, int fps, int frames,
                          Vector3 move, float moveStop)
    {
        var sb = new StringBuilder();
        sb.Append("{\n\"fps\":").Append(fps).Append(",\"frames\":").Append(frames)
          .Append(",\"move\":").Append(V3(move)).Append(",\"moveStop\":").Append(F(Mathf.Min(moveStop, 1e6f)))
          .Append(",\n\"systems\":[\n");
        var meshes = new HashSet<Mesh>();
        for (int i = 0; i < systems.Length; i++)
        {
            var ps = systems[i];
            var r = ps.GetComponent<ParticleSystemRenderer>();
            var m = ps.main;
            var streams = new List<ParticleSystemVertexStream>();
            if (r != null) r.GetActiveVertexStreams(streams);
            var mat = r != null ? r.sharedMaterial : null;
            var tsa = ps.textureSheetAnimation;
            var l2w = go.transform.worldToLocalMatrix * ps.transform.localToWorldMatrix;
            sb.Append("{\"index\":").Append(i)
              .Append(",\"name\":").Append(Q(ps.name))
              .Append(",\"active\":").Append(ps.gameObject.activeInHierarchy ? 1 : 0)
              .Append(",\"simSpace\":").Append((int)m.simulationSpace)
              .Append(",\"scalingMode\":").Append((int)m.scalingMode)
              .Append(",\"l2w\":[");
            for (int k = 0; k < 16; k++) sb.Append(k > 0 ? "," : "").Append(F(l2w[k % 4, k / 4]));
            sb.Append("]");
            if (r != null)
            {
                sb.Append(",\"rendererEnabled\":").Append(r.enabled ? 1 : 0)
                  .Append(",\"renderMode\":").Append(Q(r.renderMode.ToString()))
                  .Append(",\"alignment\":").Append(Q(r.alignment.ToString()))
                  .Append(",\"sortMode\":").Append(Q(r.sortMode.ToString()))
                  .Append(",\"sortingFudge\":").Append(F(r.sortingFudge))
                  .Append(",\"sortingOrder\":").Append(r.sortingOrder)
                  .Append(",\"lengthScale\":").Append(F(r.lengthScale))
                  .Append(",\"velocityScale\":").Append(F(r.velocityScale))
                  .Append(",\"cameraVelocityScale\":").Append(F(r.cameraVelocityScale))
                  .Append(",\"normalDirection\":").Append(F(r.normalDirection))
                  .Append(",\"pivot\":").Append(V3(r.pivot))
                  .Append(",\"flip\":").Append(V3(r.flip))
                  .Append(",\"minParticleSize\":").Append(F(r.minParticleSize))
                  .Append(",\"maxParticleSize\":").Append(F(r.maxParticleSize))
                  .Append(",\"allowRoll\":").Append(r.allowRoll ? 1 : 0)
                  .Append(",\"material\":").Append(mat != null ? Q(AssetDatabase.GetAssetPath(mat)) : "null")
                  .Append(",\"shader\":").Append(mat != null ? Q(mat.shader.name) : "null")
                  .Append(",\"renderQueue\":").Append(mat != null ? mat.renderQueue : 0)
                  .Append(",\"streams\":[");
                for (int k = 0; k < streams.Count; k++) sb.Append(k > 0 ? "," : "").Append(Q(streams[k].ToString()));
                sb.Append("],\"meshes\":[");
                if (r.renderMode == ParticleSystemRenderMode.Mesh)
                {
                    var ms = new Mesh[r.meshCount];
                    r.GetMeshes(ms);
                    for (int k = 0; k < ms.Length; k++)
                    {
                        sb.Append(k > 0 ? "," : "").Append(Q(ms[k] != null ? ms[k].name : ""));
                        if (ms[k] != null) meshes.Add(ms[k]);
                    }
                }
                sb.Append("]");
            }
            sb.Append(",\"tsa\":").Append(tsa.enabled ? $"{{\"tilesX\":{tsa.numTilesX},\"tilesY\":{tsa.numTilesY}}}" : "null");
            var lm = ps.lights;
            if (lm.enabled && lm.light != null)
            {
                var L = lm.light;
                sb.Append(",\"lights\":{\"type\":").Append(Q(L.type.ToString()))
                  .Append(",\"color\":[").Append(F(L.color.r)).Append(",").Append(F(L.color.g)).Append(",").Append(F(L.color.b)).Append("]")
                  .Append(",\"intensity\":").Append(F(L.intensity))
                  .Append(",\"range\":").Append(F(L.range))
                  .Append(",\"useParticleColor\":").Append(lm.useParticleColor ? 1 : 0)
                  .Append(",\"sizeAffectsRange\":").Append(lm.sizeAffectsRange ? 1 : 0)
                  .Append(",\"alphaAffectsIntensity\":").Append(lm.alphaAffectsIntensity ? 1 : 0)
                  .Append(",\"rangeMultiplier\":").Append(F(lm.rangeMultiplier))
                  .Append(",\"intensityMultiplier\":").Append(F(lm.intensityMultiplier))
                  .Append(",\"rangeCurveMode\":").Append((int)lm.range.mode)
                  .Append(",\"intensityCurveMode\":").Append((int)lm.intensity.mode)
                  .Append(",\"maxLights\":").Append(lm.maxLights)
                  .Append("}");
            }
            // THE TRAILS MODULE, with its curves sampled at 33 points along the trail (0 the
            // particle's end, 1 the far end) and its material, which is the renderer's second
            var tm = ps.trails;
            var em = ps.emission;
            sb.Append(",\"emission\":{\"rateOverTime\":").Append(F(em.rateOverTime.constantMax))
              .Append(",\"rateOverTimeMode\":").Append((int)em.rateOverTime.mode)
              .Append(",\"rateOverDistance\":").Append(F(em.rateOverDistance.constantMax))
              .Append(",\"rateOverDistanceMode\":").Append((int)em.rateOverDistance.mode)
              .Append(",\"bursts\":").Append(em.burstCount).Append("}");
            if (tm.enabled && r != null)
            {
                Material tmat = null;
                var mats = r.sharedMaterials;
                if (mats != null && mats.Length > 1) tmat = mats[1];
                sb.Append(",\"trail\":{\"mode\":").Append((int)tm.mode)
                  .Append(",\"ratio\":").Append(F(tm.ratio))
                  .Append(",\"lifetime\":").Append(F(tm.lifetime.constantMax))
                  .Append(",\"lifetimeMode\":").Append((int)tm.lifetime.mode)
                  .Append(",\"minVertexDistance\":").Append(F(tm.minVertexDistance))
                  .Append(",\"textureMode\":").Append((int)tm.textureMode)
                  .Append(",\"worldSpace\":").Append(tm.worldSpace ? 1 : 0)
                  .Append(",\"dieWithParticles\":").Append(tm.dieWithParticles ? 1 : 0)
                  .Append(",\"sizeAffectsWidth\":").Append(tm.sizeAffectsWidth ? 1 : 0)
                  .Append(",\"sizeAffectsLifetime\":").Append(tm.sizeAffectsLifetime ? 1 : 0)
                  .Append(",\"inheritParticleColor\":").Append(tm.inheritParticleColor ? 1 : 0)
                  .Append(",\"ribbonCount\":").Append(tm.ribbonCount)
                  .Append(",\"splitSubEmitterRibbons\":").Append(tm.splitSubEmitterRibbons ? 1 : 0)
                  .Append(",\"attachRibbonsToTransform\":").Append(tm.attachRibbonsToTransform ? 1 : 0)
                  .Append(",\"widthMultiplier\":").Append(F(tm.widthOverTrailMultiplier))
                  .Append(",\"material\":").Append(tmat != null ? Q(AssetDatabase.GetAssetPath(tmat)) : "null")
                  .Append(",\"width\":[");
                for (int k = 0; k <= 32; k++)
                    sb.Append(k > 0 ? "," : "").Append(F(tm.widthOverTrail.Evaluate(k / 32f, 0.5f)));
                sb.Append("],\"color\":[");
                for (int k = 0; k <= 32; k++)
                {
                    var c = tm.colorOverTrail.Evaluate(k / 32f, 0.5f);
                    sb.Append(k > 0 ? "," : "").Append("[").Append(F(c.r)).Append(",").Append(F(c.g))
                      .Append(",").Append(F(c.b)).Append(",").Append(F(c.a)).Append("]");
                }
                sb.Append("],\"colorMode\":").Append((int)tm.colorOverTrail.mode)
                  .Append(",\"widthMode\":").Append((int)tm.widthOverTrail.mode).Append("}");
            }
            sb.Append("}").Append(i + 1 < systems.Length ? ",\n" : "\n");
        }
        sb.Append("],\n\"lines\":[\n");
        // TRAIL RENDERERS (the component, not the particle module): a ribbon through the
        // object's own past positions, kept for `time` seconds, with its width and colour
        // curves along the line sampled at 33 points (0 the newest end).
        var trs = go.GetComponentsInChildren<TrailRenderer>(true);
        for (int i = 0; i < trs.Length; i++)
        {
            var tr = trs[i];
            var l2w = go.transform.worldToLocalMatrix * tr.transform.localToWorldMatrix;
            sb.Append("{\"index\":").Append(i).Append(",\"name\":").Append(Q(tr.name))
              .Append(",\"enabled\":").Append(tr.enabled && tr.gameObject.activeInHierarchy ? 1 : 0)
              .Append(",\"time\":").Append(F(tr.time))
              .Append(",\"minVertexDistance\":").Append(F(tr.minVertexDistance))
              .Append(",\"widthMultiplier\":").Append(F(tr.widthMultiplier))
              .Append(",\"textureMode\":").Append((int)tr.textureMode)
              .Append(",\"textureScale\":[").Append(F(tr.textureScale.x)).Append(",").Append(F(tr.textureScale.y)).Append("]")
              .Append(",\"alignment\":").Append((int)tr.alignment)
              .Append(",\"numCornerVertices\":").Append(tr.numCornerVertices)
              .Append(",\"numCapVertices\":").Append(tr.numCapVertices)
              .Append(",\"generateLightingData\":").Append(tr.generateLightingData ? 1 : 0)
              .Append(",\"material\":").Append(tr.sharedMaterial != null ? Q(AssetDatabase.GetAssetPath(tr.sharedMaterial)) : "null")
              .Append(",\"l2w\":[");
            for (int k = 0; k < 16; k++) sb.Append(k > 0 ? "," : "").Append(F(l2w[k % 4, k / 4]));
            sb.Append("],\"width\":[");
            for (int k = 0; k <= 32; k++) sb.Append(k > 0 ? "," : "").Append(F(tr.widthCurve.Evaluate(k / 32f)));
            sb.Append("],\"color\":[");
            for (int k = 0; k <= 32; k++)
            {
                var c = tr.colorGradient.Evaluate(k / 32f);
                sb.Append(k > 0 ? "," : "").Append("[").Append(F(c.r)).Append(",").Append(F(c.g))
                  .Append(",").Append(F(c.b)).Append(",").Append(F(c.a)).Append("]");
            }
            sb.Append("]}").Append(i + 1 < trs.Length ? ",\n" : "\n");
        }
        sb.Append("]}\n");
        File.WriteAllText(Path.Combine(outDir, "meta.json"), sb.ToString());
        foreach (var mesh in meshes) WriteMesh(outDir, mesh);
    }

    // MESH: "C3VM", uint32 version, vertexCount, indexCount, then per vertex
    // position(3) normal(3) tangent(4) uv0(2) colour(4), then uint32 indices.
    static void WriteMesh(string outDir, Mesh mesh)
    {
        using (var arr = Mesh.AcquireReadOnlyMeshData(mesh))
        {
            var md = arr[0];
            int vc = md.vertexCount;
            var pos = new Unity.Collections.NativeArray<Vector3>(vc, Unity.Collections.Allocator.Temp);
            var nrm = new Unity.Collections.NativeArray<Vector3>(vc, Unity.Collections.Allocator.Temp);
            var tan = new Unity.Collections.NativeArray<Vector4>(vc, Unity.Collections.Allocator.Temp);
            var uv  = new Unity.Collections.NativeArray<Vector2>(vc, Unity.Collections.Allocator.Temp);
            var col = new Unity.Collections.NativeArray<Color>(vc, Unity.Collections.Allocator.Temp);
            md.GetVertices(pos);
            if (md.HasVertexAttribute(VertexAttribute.Normal)) md.GetNormals(nrm);
            if (md.HasVertexAttribute(VertexAttribute.Tangent)) md.GetTangents(tan);
            if (md.HasVertexAttribute(VertexAttribute.TexCoord0)) md.GetUVs(0, uv);
            bool hasCol = md.HasVertexAttribute(VertexAttribute.Color);
            if (hasCol) md.GetColors(col);
            var idx = new List<int>();
            for (int sm = 0; sm < md.subMeshCount; sm++)
            {
                var d = md.GetSubMesh(sm);
                var tmp = new Unity.Collections.NativeArray<int>(d.indexCount, Unity.Collections.Allocator.Temp);
                md.GetIndices(tmp, sm);
                idx.AddRange(tmp);
                tmp.Dispose();
            }
            string safe = mesh.name.Replace(' ', '_').Replace('/', '_');
            using (var bw = new BinaryWriter(File.Create(Path.Combine(outDir, $"mesh_{safe}.bin"))))
            {
                bw.Write(Encoding.ASCII.GetBytes("C3VM"));
                bw.Write((uint)1); bw.Write((uint)vc); bw.Write((uint)idx.Count);
                for (int v = 0; v < vc; v++)
                {
                    W3(bw, pos[v]); W3(bw, nrm[v]); W4(bw, tan[v]);
                    bw.Write(uv[v].x); bw.Write(uv[v].y);
                    Color c = hasCol ? col[v] : Color.white;
                    bw.Write(c.r); bw.Write(c.g); bw.Write(c.b); bw.Write(c.a);
                }
                foreach (var i in idx) bw.Write((uint)i);
            }
            pos.Dispose(); nrm.Dispose(); tan.Dispose(); uv.Dispose(); col.Dispose();
        }
    }
}
