// OpenCNC3DFoliageExport.cs -- writes every prefab of a vegetation pack out as plain JSON:
// the meshes each renderer draws, placed in the prefab root's own space, and the
// material each submesh uses (its shader, its textures by asset path, its alpha cut).
//
// Run in batch mode on a throwaway project that has the pack imported:
//   Unity -batchmode -projectPath <proj> -executeMethod OpenCNC3DFoliageExport.Run \
//         -foliageRoot "Assets/Realistic Grass and Bush Pack1_1" -foliageOut <dir> -quit
//
// The JSON is what tools/art/foliage/bake_foliage3d.py reads. Unity is used only to
// resolve the prefabs, whose meshes are stored in Unity's own binary serialisation and
// in its "_modified" mesh assets rather than in the FBX files beside them.
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;
using UnityEditor;
using UnityEngine;

public static class OpenCNC3DFoliageExport
{
    static string Arg(string name, string def)
    {
        var a = System.Environment.GetCommandLineArgs();
        for (int i = 0; i + 1 < a.Length; i++) if (a[i] == name) return a[i + 1];
        return def;
    }

    static string F(float v) { return v.ToString("R", CultureInfo.InvariantCulture); }

    static void Vec(StringBuilder sb, IList<Vector3> v)
    {
        sb.Append('[');
        for (int i = 0; i < v.Count; i++) {
            if (i > 0) sb.Append(',');
            sb.Append(F(v[i].x)).Append(',').Append(F(v[i].y)).Append(',').Append(F(v[i].z));
        }
        sb.Append(']');
    }

    static void Tex(StringBuilder sb, Material m, string prop)
    {
        if (!m.HasProperty(prop)) return;
        var t = m.GetTexture(prop);
        if (t == null) return;
        sb.Append(",\"tex").Append(prop).Append("\":\"").Append(AssetDatabase.GetAssetPath(t)).Append('"');
    }

    public static void Run()
    {
        string root = Arg("-foliageRoot", "Assets");
        string outDir = Arg("-foliageOut", "foliage_export");
        Directory.CreateDirectory(outDir);
        var guids = AssetDatabase.FindAssets("t:Prefab", new[] { root });
        foreach (var g in guids) {
            string path = AssetDatabase.GUIDToAssetPath(g);
            var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(path);
            // THE ASSET ITSELF, NOT AN INSTANCE. A Tree Creator prefab's mesh is a sub-asset
            // of the prefab, and an instantiated copy in batch mode comes back with no mesh.
            var go = prefab;
            var sb = new StringBuilder();
            sb.Append("{\"prefab\":\"").Append(path).Append("\",\"rootScale\":[")
              .Append(F(prefab.transform.localScale.x)).Append(',')
              .Append(F(prefab.transform.localScale.y)).Append(',')
              .Append(F(prefab.transform.localScale.z)).Append("],\"renderers\":[");
            // LOD membership, when the prefab has an LODGroup
            var lodOf = new Dictionary<Renderer, int>();
            var lg = go.GetComponentInChildren<LODGroup>();
            if (lg != null) {
                var lods = lg.GetLODs();
                for (int l = 0; l < lods.Length; l++)
                    foreach (var r in lods[l].renderers) if (r != null) lodOf[r] = l;
            }
            bool first = true;
            var root2 = go.transform.worldToLocalMatrix;
            foreach (var mf in go.GetComponentsInChildren<MeshFilter>(true)) {
                var mr = mf.GetComponent<MeshRenderer>();
                var mesh = mf.sharedMesh;
                // A TREE CREATOR PREFAB keeps its generated mesh and its two "Optimized"
                // materials as sub-assets, and the filter's own reference can come back
                // empty in batch mode. The sub-assets are the same objects, so take them.
                Material[] subMats = null;
                if (mesh == null) {
                    var subs = AssetDatabase.LoadAllAssetsAtPath(path);
                    var ml = new List<Material>();
                    foreach (var o in subs) {
                        if (o is Mesh && mesh == null) mesh = (Mesh)o;
                        if (o is Material) ml.Add((Material)o);
                    }
                    // bark first, leaves second: the Tree Creator's own submesh order
                    ml.Sort((a, b) => a.name.Contains("Bark") ? -1 : (b.name.Contains("Bark") ? 1 : 0));
                    subMats = ml.ToArray();
                    Debug.Log("OpenCNC3DFoliageExport: " + path + " took its mesh from the sub-assets: "
                              + (mesh != null ? mesh.name + " " + mesh.subMeshCount + " submeshes" : "none"));
                }
                if (mr == null || mesh == null) continue;
                if (!first) sb.Append(',');
                first = false;
                var M = root2 * mf.transform.localToWorldMatrix;
                var v = mesh.vertices; var n = mesh.normals;
                var pv = new List<Vector3>(); var pn = new List<Vector3>();
                for (int i = 0; i < v.Length; i++) pv.Add(M.MultiplyPoint3x4(v[i]));
                for (int i = 0; i < n.Length; i++) pn.Add(M.MultiplyVector(n[i]).normalized);
                sb.Append("{\"node\":\"").Append(mf.name).Append("\",\"mesh\":\"").Append(mesh.name)
                  .Append("\",\"meshAsset\":\"").Append(AssetDatabase.GetAssetPath(mesh))
                  .Append("\",\"lod\":").Append(lodOf.ContainsKey(mr) ? lodOf[mr] : -1)
                  .Append(",\"pos\":"); Vec(sb, pv);
                sb.Append(",\"nrm\":"); Vec(sb, pn);
                var uv = mesh.uv;
                sb.Append(",\"uv\":[");
                for (int i = 0; i < uv.Length; i++) {
                    if (i > 0) sb.Append(',');
                    sb.Append(F(uv[i].x)).Append(',').Append(F(uv[i].y));
                }
                sb.Append("],\"col\":[");
                var c = mesh.colors;
                for (int i = 0; i < c.Length; i++) {
                    if (i > 0) sb.Append(',');
                    sb.Append(F(c[i].r)).Append(',').Append(F(c[i].g)).Append(',')
                      .Append(F(c[i].b)).Append(',').Append(F(c[i].a));
                }
                sb.Append("],\"sub\":[");
                var mats = (subMats != null && subMats.Length > 0) ? subMats : mr.sharedMaterials;
                for (int s = 0; s < mesh.subMeshCount; s++) {
                    if (s > 0) sb.Append(',');
                    var m = s < mats.Length ? mats[s] : null;
                    sb.Append("{\"idx\":[");
                    var idx = mesh.GetTriangles(s);
                    for (int i = 0; i < idx.Length; i++) { if (i > 0) sb.Append(','); sb.Append(idx[i]); }
                    sb.Append("]");
                    if (m != null) {
                        sb.Append(",\"mat\":\"").Append(m.name).Append("\",\"shader\":\"")
                          .Append(m.shader != null ? m.shader.name : "").Append('"');
                        sb.Append(",\"matAsset\":\"").Append(AssetDatabase.GetAssetPath(m)).Append('"');
                        foreach (var p in new[] { "_MainTex", "_BumpMap", "_SpecGlossMap",
                                                  "_TranslucencyMap", "_ShadowOffset", "_BumpSpecMap",
                                                  "_TranslucencyGloss", "_ShadowTex" })
                            Tex(sb, m, p);
                        if (m.HasProperty("_Cutoff")) sb.Append(",\"cutoff\":").Append(F(m.GetFloat("_Cutoff")));
                        if (m.HasProperty("_Color")) {
                            var cc = m.GetColor("_Color");
                            sb.Append(",\"color\":[").Append(F(cc.r)).Append(',').Append(F(cc.g))
                              .Append(',').Append(F(cc.b)).Append(',').Append(F(cc.a)).Append(']');
                        }
                        // every texture property the material actually carries, by name
                        sb.Append(",\"allTex\":{");
                        bool ft = true;
                        foreach (var tp in m.GetTexturePropertyNames()) {
                            var t = m.GetTexture(tp);
                            if (t == null) continue;
                            if (!ft) sb.Append(',');
                            ft = false;
                            sb.Append('"').Append(tp).Append("\":\"").Append(AssetDatabase.GetAssetPath(t)).Append('"');
                        }
                        sb.Append('}');
                    }
                    sb.Append('}');
                }
                sb.Append("]}");
            }
            sb.Append("]}");
            string name = Path.GetFileNameWithoutExtension(path).Replace(' ', '_');
            File.WriteAllText(Path.Combine(outDir, name + ".json"), sb.ToString());
            Debug.Log("OpenCNC3DFoliageExport: wrote " + name);
        }
    }
}
