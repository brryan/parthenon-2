//========================================================================================
// (C) (or copyright) 2020. Triad National Security, LLC. All rights reserved.
//
// This program was produced under U.S. Government contract 89233218CNA000001 for Los
// Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC
// for the U.S. Department of Energy/National Nuclear Security Administration. All rights
// in the program are reserved by Triad National Security, LLC, and the U.S. Department
// of Energy/National Nuclear Security Administration. The Government is granted for
// itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide
// license in this material to reproduce, prepare derivative works, distribute copies to
// the public, perform publicly and display publicly, and to permit others to do so.
//========================================================================================

#include "interface/update.hpp"

#include <algorithm>
#include <limits>

#include "coordinates/coordinates.hpp"
#include "interface/container.hpp"
#include "interface/container_iterator.hpp"
#include "mesh/mesh.hpp"

namespace parthenon {

namespace Update {

TaskStatus FluxDivergence(Container<Real> &in, Container<Real> &dudt_cont) {
  MeshBlock *pmb = in.pmy_block;
  int is = pmb->is;
  int js = pmb->js;
  int ks = pmb->ks;
  int ie = pmb->ie;
  int je = pmb->je;
  int ke = pmb->ke;

  Metadata m;
  ContainerIterator<Real> cin_iter(in, {Metadata::Independent});
  ContainerIterator<Real> cout_iter(dudt_cont, {Metadata::Independent});
  int nvars = cout_iter.vars.size();

  ParArrayND<Real> x1area("x1area", pmb->ncells1);
  ParArrayND<Real> x2area0("x2area0", pmb->ncells1);
  ParArrayND<Real> x2area1("x2area1", pmb->ncells1);
  ParArrayND<Real> x3area0("x3area0", pmb->ncells1);
  ParArrayND<Real> x3area1("x3area1", pmb->ncells1);
  ParArrayND<Real> vol("vol", pmb->ncells1);

  int ndim = pmb->pmy_mesh->ndim;
  ParArrayND<Real> du("du", pmb->ncells1);
  for (int k = ks; k <= ke; k++) {
    for (int j = js; j <= je; j++) {
      pmb->pcoord->Face1Area(k, j, is, ie + 1, x1area);
      pmb->pcoord->CellVolume(k, j, is, ie, vol);
      if (pmb->pmy_mesh->ndim >= 2) {
        pmb->pcoord->Face2Area(k, j, is, ie, x2area0);
        pmb->pcoord->Face2Area(k, j + 1, is, ie, x2area1);
      }
      if (pmb->pmy_mesh->ndim >= 3) {
        pmb->pcoord->Face3Area(k, j, is, ie, x3area0);
        pmb->pcoord->Face3Area(k + 1, j, is, ie, x3area1);
      }
      for (int n = 0; n < nvars; n++) {
        CellVariable<Real> &q = *cin_iter.vars[n];
        ParArrayND<Real> &x1flux = q.flux[0];
        ParArrayND<Real> &x2flux = q.flux[1];
        ParArrayND<Real> &x3flux = q.flux[2];
        CellVariable<Real> &dudt = *cout_iter.vars[n];
        for (int l = 0; l < q.GetDim(4); l++) {
          for (int i = is; i <= ie; i++) {
            du(i) =
                (x1area(i + 1) * x1flux(l, k, j, i + 1) - x1area(i) * x1flux(l, k, j, i));
          }

          if (ndim >= 2) {
            for (int i = is; i <= ie; i++) {
              du(i) +=
                  (x2area1(i) * x2flux(l, k, j + 1, i) - x2area0(i) * x2flux(l, k, j, i));
            }
          }
          // TODO(jcd): should the next block be in the preceding if??
          if (ndim >= 3) {
            for (int i = is; i <= ie; i++) {
              du(i) +=
                  (x3area1(i) * x3flux(l, k + 1, j, i) - x3area0(i) * x3flux(l, k, j, i));
            }
          }
          for (int i = is; i <= ie; i++) {
            dudt(l, k, j, i) = -du(i) / vol(i);
          }
        }
      }
    }
  }

  return TaskStatus::complete;
}

//void TransportSwarm(Swarm &in, const Real dt, Swarm &out) {
TaskStatus TransportSwarm(Swarm &in, Swarm &out) {
  // TODO BRR put const real dt back in the args list
  const Real dt = 0.1;
  int nmax_active = in.get_nmax_active();

  ParticleVariable<Real> &x_in = in.GetReal("x");
  ParticleVariable<Real> &y_in = in.GetReal("y");
  ParticleVariable<Real> &z_in = in.GetReal("z");
  ParticleVariable<Real> &x_out = out.GetReal("x");
  ParticleVariable<Real> &y_out = out.GetReal("y");
  ParticleVariable<Real> &z_out = out.GetReal("z");
  //ParticleVariable<Real> vx = in.Get("vx");
  //ParticleVariable<Real> vy = in.Get("vy");
  //ParticleVariable<Real> vz = in.Get("vz");
  //ParticleVariable<int> mask = in.Get("mask");
  double vx = 1.;
  double vy = 1.;
  double vz = 1.;

  for (int n = 0; n < nmax_active; n++) {
    x_out(n) = x_in(n) + vx*dt;
    y_out(n) = y_in(n) + vy*dt;
    z_out(n) = z_in(n) + vz*dt;
  }

  return TaskStatus::complete;
}

void UpdateContainer(Container<Real> &in, Container<Real> &dudt_cont, const Real dt,
                     Container<Real> &out) {
  MeshBlock *pmb = in.pmy_block;
  int is = pmb->is;
  int js = pmb->js;
  int ks = pmb->ks;
  int ie = pmb->ie;
  int je = pmb->je;
  int ke = pmb->ke;

  Metadata m;
  ContainerIterator<Real> cin_iter(in, {Metadata::Independent});
  ContainerIterator<Real> cout_iter(out, {Metadata::Independent});
  ContainerIterator<Real> du_iter(dudt_cont, {Metadata::Independent});
  int nvars = cout_iter.vars.size();

  for (int n = 0; n < nvars; n++) {
    CellVariable<Real> &qin = *cin_iter.vars[n];
    CellVariable<Real> &dudt = *du_iter.vars[n];
    CellVariable<Real> &qout = *cout_iter.vars[n];
    for (int l = 0; l < qout.GetDim(4); l++) {
      for (int k = ks; k <= ke; k++) {
        for (int j = js; j <= je; j++) {
          for (int i = is; i <= ie; i++) {
            qout(l, k, j, i) = qin(l, k, j, i) + dt * dudt(l, k, j, i);
          }
        }
      }
    }
  }
  return;
}

void AverageContainers(Container<Real> &c1, Container<Real> &c2, const Real wgt1) {
  MeshBlock *pmb = c1.pmy_block;
  int is = pmb->is;
  int js = pmb->js;
  int ks = pmb->ks;
  int ie = pmb->ie;
  int je = pmb->je;
  int ke = pmb->ke;

  Metadata m;
  ContainerIterator<Real> c1_iter(c1, {Metadata::Independent});
  ContainerIterator<Real> c2_iter(c2, {Metadata::Independent});
  int nvars = c2_iter.vars.size();

  for (int n = 0; n < nvars; n++) {
    CellVariable<Real> &q1 = *c1_iter.vars[n];
    CellVariable<Real> &q2 = *c2_iter.vars[n];
    for (int l = 0; l < q1.GetDim(4); l++) {
      for (int k = ks; k <= ke; k++) {
        for (int j = js; j <= je; j++) {
          for (int i = is; i <= ie; i++) {
            q1(l, k, j, i) = wgt1 * q1(l, k, j, i) + (1 - wgt1) * q2(l, k, j, i);
          }
        }
      }
    }
  }
  return;
}

Real EstimateTimestep(Container<Real> &rc) {
  MeshBlock *pmb = rc.pmy_block;
  Real dt_min = std::numeric_limits<Real>::max();
  for (auto &pkg : pmb->packages) {
    auto &desc = pkg.second;
    if (desc->EstimateTimestep != nullptr) {
      Real dt = desc->EstimateTimestep(rc);
      dt_min = std::min(dt_min, dt);
    }
  }
  return dt_min;
}

} // namespace Update

static FillDerivedVariables::FillDerivedFunc *_pre_package_fill = nullptr;
static FillDerivedVariables::FillDerivedFunc *_post_package_fill = nullptr;

void FillDerivedVariables::SetFillDerivedFunctions(FillDerivedFunc *pre,
                                                   FillDerivedFunc *post) {
  _pre_package_fill = pre;
  _post_package_fill = post;
}

TaskStatus FillDerivedVariables::FillDerived(Container<Real> &rc) {
  if (_pre_package_fill != nullptr) {
    _pre_package_fill(rc);
  }
  for (auto &pkg : rc.pmy_block->packages) {
    auto &desc = pkg.second;
    if (desc->FillDerived != nullptr) {
      desc->FillDerived(rc);
    }
  }
  if (_post_package_fill != nullptr) {
    _post_package_fill(rc);
  }
  return TaskStatus::complete;
}

} // namespace parthenon
