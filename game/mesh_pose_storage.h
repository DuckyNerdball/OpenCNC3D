#ifndef OPENCNC3D_MESH_POSE_STORAGE_H
#define OPENCNC3D_MESH_POSE_STORAGE_H

#include <algorithm>
#include <cstddef>
#include <limits>
#include <new>
#include <vector>

/* CPU pose storage is separate from its GPU descriptor. A direct position upload
   can be reused by later passes without reserving unused per-corner CPU arrays. */
struct MeshGeometryCorner { float p[3], n[3]; };
struct MeshPoseStorage {
    std::vector<MeshGeometryCorner> corners;
    std::vector<unsigned char> valid;
    size_t requestedCount = 0;
    bool fresh = true;
};
enum MeshPoseStorageResult { MESH_POSE_READY, MESH_POSE_LIMIT, MESH_POSE_ALLOCATION };

/* Preserve eager allocation on maps that never exhaust the original working set.
   Auto switches once, at a real ephemeral admission limit after tail reclamation.
   OOM, settled storage and a single oversized pose are not scene pressure. */
enum MeshPoseMode { MESH_POSE_AUTO, MESH_POSE_EAGER, MESH_POSE_LAZY };
struct MeshPosePolicy {
    MeshPoseMode mode;
    size_t ceiling;
    bool pressure = false;
    unsigned long long activations = 0;
    MeshPosePolicy(MeshPoseMode selected, size_t cap) : mode(selected), ceiling(cap) {}
    size_t cap() const {
        const size_t legacy = 64u*1024u*1024u;
        return mode == MESH_POSE_AUTO && !pressure ? std::min(legacy, ceiling) : ceiling;
    }
    bool lazy() const { return mode == MESH_POSE_LAZY || (mode == MESH_POSE_AUTO && pressure); }
    bool admit_pressure(MeshPoseStorageResult result, bool ephemeral,
                        size_t requestedBytes, size_t otherBytes) {
        const size_t initial = cap();
        if (mode != MESH_POSE_AUTO || pressure || !ephemeral || result != MESH_POSE_LIMIT
            || requestedBytes > initial || otherBytes <= initial-requestedBytes) return false;
        pressure = true; activations++; return true;
    }
    void reset() { pressure = false; }
};

static bool mesh_pose_required_bytes(size_t count, size_t& bytes) {
    const size_t stride = sizeof(MeshGeometryCorner) + sizeof(unsigned char);
    if (count > std::numeric_limits<size_t>::max() / stride) return false;
    bytes = count * stride;
    return true;
}
static size_t mesh_pose_storage_bytes(const MeshPoseStorage& pose) {
    return pose.corners.capacity() * sizeof(MeshGeometryCorner) + pose.valid.capacity();
}
static void mesh_pose_storage_release(MeshPoseStorage& pose, size_t& total) {
    total -= mesh_pose_storage_bytes(pose);
    std::vector<MeshGeometryCorner>().swap(pose.corners);
    std::vector<unsigned char>().swap(pose.valid);
    pose.fresh = true;
}
/* Account retained capacities, not just the requested size. Old-frame data is
   disposable, so release it before replacement rather than briefly retaining two
   copies near the ceiling. Allocation failure leaves a valid empty descriptor. */
static MeshPoseStorageResult mesh_pose_storage_materialize(MeshPoseStorage& pose,
        size_t cap, size_t& total, bool allocationAllowed = true) {
    const size_t n = pose.requestedCount;
    size_t exact = 0;
    if (!mesh_pose_required_bytes(n, exact) || exact > cap) return MESH_POSE_LIMIT;
    const size_t old = mesh_pose_storage_bytes(pose);
    if (old > total || total - old > cap || exact > cap - (total - old))
        return MESH_POSE_LIMIT;
    const bool fits = pose.corners.capacity() >= n && pose.valid.capacity() >= n;
    if (fits && total <= cap) {
        pose.corners.resize(n); pose.valid.resize(n);
        if (pose.fresh) std::fill(pose.valid.begin(), pose.valid.end(), 0);
        pose.fresh = false;
        return MESH_POSE_READY;
    }
    mesh_pose_storage_release(pose, total);
    try {
        if (!allocationAllowed) throw std::bad_alloc();
        std::vector<MeshGeometryCorner> corners(n);
        const size_t cornerBytes = corners.capacity() * sizeof(MeshGeometryCorner);
        if (cornerBytes > cap - total || n > cap - total - cornerBytes)
            return MESH_POSE_LIMIT;
        std::vector<unsigned char> valid(n);
        if (valid.capacity() > cap - total - cornerBytes) return MESH_POSE_LIMIT;
        pose.corners.swap(corners); pose.valid.swap(valid);
        total += mesh_pose_storage_bytes(pose);
        pose.fresh = false;
        return MESH_POSE_READY;
    } catch (const std::bad_alloc&) {
        return MESH_POSE_ALLOCATION;
    }
}
#endif
