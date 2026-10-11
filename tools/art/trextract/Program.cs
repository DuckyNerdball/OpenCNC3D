// TRExtract: dump uncooked UE5.4 editor assets (skeletal mesh, skeleton, texture,
// animation) into plain files a Python converter can read. Built on CUE4Parse.
//
//   TRExtract <projectDir> mesh  <objectPath> <outPrefix>    -> outPrefix.json + .bin
//   TRExtract <projectDir> skel  <objectPath> <out.json>
//   TRExtract <projectDir> tex   <objectPath> <out.png>        (cooked textures only)
//   TRExtract <projectDir> texsrc <objectPath> <out.raw>      (an editor texture's source
//                                                             payload, still compressed)
//   TRExtract <projectDir> anim  <objectPath> <out.json>
//   TRExtract <projectDir> props <objectPath>                 -> json to stdout
using CUE4Parse.FileProvider;
using CUE4Parse.UE4.Assets.Exports;
using CUE4Parse.UE4.Assets.Exports.Animation;
using CUE4Parse.UE4.Assets.Exports.SkeletalMesh;
using CUE4Parse.UE4.Assets.Exports.Texture;
using CUE4Parse.UE4.Versions;
using CUE4Parse.UE4.Assets.Exports.Animation;
using CUE4Parse.UE4.Objects.Core.Math;
using CUE4Parse_Conversion.Textures;
using Newtonsoft.Json;
using SkiaSharp;

var projectDir = args[0];
var cmd = args[1];
var provider = new DefaultFileProvider(projectDir, SearchOption.AllDirectories,
    new VersionContainer(EGame.GAME_UE5_4), StringComparer.OrdinalIgnoreCase);
provider.Initialize();
provider.Mount();

UObject Load(string p) => provider.LoadPackageObject(p);

switch (cmd)
{
case "props":
{
    var pkg = provider.LoadPackage(args[2].Split('.')[0]);
    Console.WriteLine(JsonConvert.SerializeObject(pkg.GetExports(), Formatting.Indented));
    break;
}
case "mesh":
{
    // The editor LOD model: per section, soft vertices whose bone indices are local to
    // that section's BoneMap; the index buffer is global over the concatenated sections.
    var mesh = (USkeletalMesh)Load(args[2]);
    var lod = mesh.LODModels[0];
    var rs = mesh.ReferenceSkeleton;
    var meta = new Dictionary<string, object>();
    var bones = new List<object>();
    for (int i = 0; i < rs.FinalRefBoneInfo.Length; i++)
    {
        var t = rs.FinalRefBonePose[i];
        bones.Add(new { name = rs.FinalRefBoneInfo[i].Name.Text, parent = rs.FinalRefBoneInfo[i].ParentIndex,
            t = new[] { t.Translation.X, t.Translation.Y, t.Translation.Z },
            r = new[] { t.Rotation.X, t.Rotation.Y, t.Rotation.Z, t.Rotation.W },
            s = new[] { t.Scale3D.X, t.Scale3D.Y, t.Scale3D.Z } });
    }
    meta["bones"] = bones;
    meta["materials"] = mesh.SkeletalMaterials.Select(m => new { slot = m.MaterialSlotName.Text,
        imported = m.ImportedMaterialSlotName?.Text,
        path = m.MaterialInterface?.ResolvedObject?.GetPathName() }).ToList();
    var secs = new List<object>();
    var verts = new List<FSoftVertex>();
    var maps = new List<ushort[]>();
    foreach (var s in lod.Sections)
    {
        secs.Add(new { mat = (int)s.MaterialIndex, first = s.BaseIndex, faces = s.NumTriangles,
            basevert = s.BaseVertexIndex, nverts = s.SoftVertices.Length, disabled = s.bDisabled });
        foreach (var v in s.SoftVertices) { verts.Add(v); maps.Add(s.BoneMap); }
    }
    meta["sections"] = secs;
    var idx = lod.Indices.Buffer;
    meta["nverts"] = verts.Count;
    meta["nidx"] = idx.Length;
    meta["lods"] = mesh.LODModels.Length;
    File.WriteAllText(args[3] + ".json", JsonConvert.SerializeObject(meta, Formatting.Indented));
    using var bw = new BinaryWriter(File.Create(args[3] + ".bin"));
    foreach (var v in verts) { bw.Write(v.Pos.X); bw.Write(v.Pos.Y); bw.Write(v.Pos.Z); }
    foreach (var v in verts) { var n = (FVector)v.Normal[2]; bw.Write(n.X); bw.Write(n.Y); bw.Write(n.Z); }
    foreach (var v in verts) { bw.Write(v.UVs[0].U); bw.Write(v.UVs[0].V); }
    int maxInf = 0;
    for (int i = 0; i < verts.Count; i++)
    {
        var inf = verts[i].Infs;
        var map = maps[i];
        var bi = new ushort[8]; var bwt = new float[8];
        int n = 0; float sum = 0;
        for (int k = 0; k < inf.BoneWeight.Length && n < 8; k++)
        {
            if (inf.BoneWeight[k] == 0) continue;
            bi[n] = map[inf.BoneIndex[k]]; bwt[n] = inf.BoneWeight[k]; sum += bwt[n]; n++;
        }
        maxInf = Math.Max(maxInf, n);
        for (int k = 0; k < 8; k++) bw.Write(bi[k]);
        for (int k = 0; k < 8; k++) bw.Write(sum > 0 ? bwt[k] / sum : 0f);
    }
    for (int i = 0; i < idx.Length; i++) bw.Write((uint)idx[i]);
    // Every LOD the asset carries, so a dump says what it left behind: only LOD 0 is written.
    for (int l = 0; l < mesh.LODModels.Length; l++)
    {
        var lm = mesh.LODModels[l];
        Console.WriteLine($"lod {l}: {lm.Sections.Sum(x => x.SoftVertices.Length)} verts, {(lm.Indices?.Buffer?.Length ?? 0) / 3} tris, {lm.Sections.Length} sections");
    }
    Console.WriteLine($"mesh: {verts.Count} verts, {idx.Length / 3} tris, {bones.Count} bones, {lod.Sections.Length} sections, maxinf {maxInf}");
    break;
}
case "skel":
{
    var sk = (USkeleton)Load(args[2]);
    var rs = sk.ReferenceSkeleton;
    var bones = new List<object>();
    for (int i = 0; i < rs.FinalRefBoneInfo.Length; i++)
    {
        var t = rs.FinalRefBonePose[i];
        bones.Add(new { name = rs.FinalRefBoneInfo[i].Name.Text, parent = rs.FinalRefBoneInfo[i].ParentIndex,
            t = new[] { t.Translation.X, t.Translation.Y, t.Translation.Z },
            r = new[] { t.Rotation.X, t.Rotation.Y, t.Rotation.Z, t.Rotation.W },
            s = new[] { t.Scale3D.X, t.Scale3D.Y, t.Scale3D.Z } });
    }
    File.WriteAllText(args[3], JsonConvert.SerializeObject(new { bones }, Formatting.Indented));
    Console.WriteLine($"skeleton: {bones.Count} bones");
    break;
}
case "tex":
{
    var tex = (UTexture2D)Load(args[2]);
    var bmp = tex.Decode();
    if (bmp == null) { Console.Error.WriteLine("decode failed"); Environment.Exit(2); }
    var sk = bmp.ToSkBitmap();
    using var fs = File.Create(args[3]);
    sk.Encode(fs, SKEncodedImageFormat.Png, 100);
    Console.WriteLine($"tex: {sk.Width}x{sk.Height}");
    break;
}
case "texsrc":
{
    // The editor source art: an FCompressedBuffer (64 byte header, then for a compressed
    // method a big-endian block size table and the blocks) holding PNG or raw pixels.
    var tex = (UTexture)Load(args[2]);
    var pl = tex.EditorData?.Payload;
    if (pl == null) { Console.Error.WriteLine("no editor payload"); Environment.Exit(2); }
    var h = pl.Header;
    Console.WriteLine($"method {h.Method} comp {h.Compressor} blocks {h.BlockCount} exp {h.BlockSizeExponent} raw {h.TotalRawSize} comp {h.TotalCompressedSize} data {pl.Data.Length}");
    File.WriteAllBytes(args[3], pl.Data);
    break;
}
case "anim":
{
    // Editor assets carry the legacy raw tracks (one per animated bone, full keys) and a
    // TrackToSkeletonMapTable property naming the skeleton bone of each track.
    var seq = (UAnimSequence)Load(args[2]);
    var skel = seq.Skeleton.Load<USkeleton>();
    var names = skel.ReferenceSkeleton.FinalRefBoneInfo;
    var mapProp = seq.GetOrDefault<CUE4Parse.UE4.Assets.Objects.FStructFallback[]>("TrackToSkeletonMapTable") ?? Array.Empty<CUE4Parse.UE4.Assets.Objects.FStructFallback>();
    var map = mapProp.Select(m => m.GetOrDefault<int>("BoneTreeIndex")).ToArray();
    var raw = seq.RawAnimationData ?? Array.Empty<FRawAnimSequenceTrack>();
    var tracks = new List<object>();
    for (int t = 0; t < raw.Length; t++)
    {
        var bi = t < map.Length ? map[t] : -1;
        tracks.Add(new { bone = bi >= 0 && bi < names.Length ? names[bi].Name.Text : $"#{bi}",
            q = raw[t].RotKeys.Select(q => new[] { q.X, q.Y, q.Z, q.W }),
            p = raw[t].PosKeys.Select(p => new[] { p.X, p.Y, p.Z }),
            sc = raw[t].ScaleKeys.Select(p => new[] { p.X, p.Y, p.Z }) });
    }
    if (raw.Length == 0)
    {
        // UE5 layout: the keys live in the AnimDataModel subobject, one track per bone,
        // named rather than indexed.
        var pkg = provider.LoadPackage(args[2].Split('.')[0]);
        foreach (var e in pkg.GetExports())
        {
            if (e.ExportType != "AnimDataModel") continue;
            // Read through the JSON form, which types every nested struct for us.
            var jo = Newtonsoft.Json.Linq.JObject.Parse(JsonConvert.SerializeObject(e));
            var bat = jo["Properties"]?["BoneAnimationTracks"] as Newtonsoft.Json.Linq.JArray;
            if (bat == null) continue;
            foreach (var t in bat)
            {
                var d = t["InternalTrackData"];
                float[] V(Newtonsoft.Json.Linq.JToken v) => new[] { (float)v["X"], (float)v["Y"], (float)v["Z"] };
                tracks.Add(new { bone = (string)t["Name"],
                    q = (d["RotKeys"] ?? new Newtonsoft.Json.Linq.JArray()).Select(v => new[] { (float)v["X"], (float)v["Y"], (float)v["Z"], (float)v["W"] }),
                    p = (d["PosKeys"] ?? new Newtonsoft.Json.Linq.JArray()).Select(V),
                    sc = (d["ScaleKeys"] ?? new Newtonsoft.Json.Linq.JArray()).Select(V) });
            }
        }
    }
    var refSeq = seq.GetOrDefault<CUE4Parse.UE4.Assets.Exports.UObject>("RefPoseSeq");
    File.WriteAllText(args[3], JsonConvert.SerializeObject(new { name = seq.Name, frames = seq.NumFrames,
        refpose_type = seq.GetOrDefault<CUE4Parse.UE4.Objects.UObject.FName>("RefPoseType").Text, refpose_seq = refSeq?.Name,
        length = seq.SequenceLength, rate = seq.RateScale, additive = seq.AdditiveAnimType.ToString(),
        root_motion = seq.GetOrDefault<bool>("bEnableRootMotion"), tracks }));
    Console.WriteLine($"anim {seq.Name}: {seq.NumFrames} frames, {seq.SequenceLength:F3}s, {tracks.Count} tracks, {seq.AdditiveAnimType}");
    break;
}
}
