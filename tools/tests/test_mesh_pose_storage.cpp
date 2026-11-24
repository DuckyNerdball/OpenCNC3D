/* CPU-only tests use the renderer's actual lazy storage/accounting helper. */
#include <cassert>
#include <cstdio>
#include <cstring>
#include "../../game/mesh_pose_storage.h"

int main() {
    const size_t stride=sizeof(MeshGeometryCorner)+1;
    size_t bytes=0;
    assert(!mesh_pose_required_bytes(std::numeric_limits<size_t>::max(),bytes));
    assert(mesh_pose_required_bytes(7,bytes) && bytes==7*stride);

    MeshPoseStorage a,b;
    size_t total=0;
    a.requestedCount=4;
    assert(a.corners.empty() && a.valid.empty() && total==0); // GPU-only descriptor
    assert(mesh_pose_storage_materialize(a,8*stride,total)==MESH_POSE_READY);
    assert(total==mesh_pose_storage_bytes(a) && total==4*stride);
    const auto* retained=a.corners.data();
    a.corners[0].p[0]=17.25f;a.valid[0]=3;
    assert(mesh_pose_storage_materialize(a,8*stride,total)==MESH_POSE_READY);
    assert(a.valid[0]==3 && a.corners[0].p[0]==17.25f && retained==a.corners.data());

    // Recycled smaller slots retain capacity and clear only validity, not position math.
    a.requestedCount=2;a.fresh=true;
    assert(mesh_pose_storage_materialize(a,8*stride,total)==MESH_POSE_READY);
    assert(a.corners.data()==retained && total==4*stride && a.valid[0]==0);
    b.requestedCount=5;
    assert(mesh_pose_storage_materialize(b,8*stride,total)==MESH_POSE_LIMIT);
    assert(b.corners.empty() && total==4*stride);
    b.requestedCount=4;
    assert(mesh_pose_storage_materialize(b,8*stride,total)==MESH_POSE_READY);
    assert(total==8*stride);

    // A larger reusable slot cannot exceed the cap or corrupt the occupied peer.
    a.requestedCount=5;a.fresh=true;
    assert(mesh_pose_storage_materialize(a,8*stride,total)==MESH_POSE_LIMIT);
    assert(total==8*stride && b.valid.size()==4);
    mesh_pose_storage_release(b,total);
    assert(total==4*stride && b.corners.empty() && b.valid.empty());

    // Real allocation-failure branch discards obsolete data, accounts it once,
    // leaves a retryable descriptor, and does not publish partially allocated arrays.
    assert(mesh_pose_storage_materialize(a,8*stride,total,false)==MESH_POSE_ALLOCATION);
    assert(total==0 && a.corners.empty() && a.valid.empty() && a.fresh);
    assert(mesh_pose_storage_materialize(a,8*stride,total)==MESH_POSE_READY);
    assert(total==5*stride && a.valid[0]==0);

    // Smaller configured budgets release retained excess capacity before replacement.
    a.requestedCount=2;a.fresh=true;
    assert(mesh_pose_storage_materialize(a,3*stride,total)==MESH_POSE_READY);
    assert(total==2*stride && a.corners.capacity()==2 && a.valid.capacity()==2);
    a.requestedCount=std::numeric_limits<size_t>::max();a.fresh=true;
    assert(mesh_pose_storage_materialize(a,8*stride,total)==MESH_POSE_LIMIT);
    assert(total==2*stride);
    mesh_pose_storage_release(a,total);
    assert(total==0);
    // The same policy used by mesh_geometry_find must latch only the actual
    // ephemeral space limit after the caller has reclaimed old tail storage.
    const size_t mib=1024u*1024u;
    MeshPosePolicy policy(MESH_POSE_AUTO,256*mib);
    assert(!policy.lazy() && policy.cap()==64*mib && !policy.pressure);
    assert(!policy.admit_pressure(MESH_POSE_ALLOCATION,true,1*mib,64*mib));
    assert(!policy.admit_pressure(MESH_POSE_LIMIT,false,1*mib,64*mib));
    assert(!policy.admit_pressure(MESH_POSE_LIMIT,true,65*mib,0));
    assert(!policy.admit_pressure(MESH_POSE_LIMIT,true,1*mib,63*mib));
    assert(policy.admit_pressure(MESH_POSE_LIMIT,true,1*mib,64*mib));
    assert(policy.lazy() && policy.cap()==256*mib && policy.activations==1);
    assert(!policy.admit_pressure(MESH_POSE_LIMIT,true,1*mib,256*mib));
    policy.reset();
    assert(!policy.lazy() && policy.cap()==64*mib && policy.activations==1);
    MeshPosePolicy legacy(MESH_POSE_AUTO,64*mib);
    assert(legacy.admit_pressure(MESH_POSE_LIMIT,true,1*mib,64*mib));
    assert(legacy.lazy() && legacy.cap()==64*mib);
    MeshPosePolicy limited(MESH_POSE_AUTO,8*mib);
    assert(limited.cap()==8*mib);
    assert(limited.admit_pressure(MESH_POSE_LIMIT,true,1*mib,8*mib));
    assert(limited.cap()==8*mib);
    MeshPosePolicy eager(MESH_POSE_EAGER,256*mib),lazy(MESH_POSE_LAZY,256*mib);
    assert(!eager.lazy() && eager.cap()==256*mib);
    assert(lazy.lazy() && lazy.cap()==256*mib);
    assert(!eager.admit_pressure(MESH_POSE_LIMIT,true,1*mib,256*mib));
    assert(!lazy.admit_pressure(MESH_POSE_LIMIT,true,1*mib,256*mib));
    std::puts("mesh pose storage: overflow, reuse, cap, shrink, allocation recovery and pressure policy passed");
}
