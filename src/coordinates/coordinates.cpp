//========================================================================================
// Athena++ astrophysical MHD code
// Copyright(C) 2014 James M. Stone <jmstone@princeton.edu> and other code contributors
// Licensed under the 3-clause BSD License, see LICENSE file for details
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
//! \file coordinates.cpp
//  \brief implements functions for Coordinates abstract base class

#include "coordinates/coordinates.hpp"

#include <algorithm>

#include "bvals/bvals.hpp"
#include "mesh/mesh.hpp"
#include "parameter_input.hpp"

namespace parthenon {

//----------------------------------------------------------------------------------------
// Coordinates constructor: sets coordinates and coordinate spacing of cell FACES

Coordinates::Coordinates(MeshBlock *pmb, ParameterInput *pin, bool flag)
    : pmy_block(pmb), coarse_flag(flag), pm(pmb->pmy_mesh) {
  RegionSize &mesh_size = pmy_block->pmy_mesh->mesh_size;
  RegionSize &block_size = pmy_block->block_size;

  // Set indices
  if (coarse_flag) {
    il = pmb->cis;
    jl = pmb->cjs;
    kl = pmb->cks;
    iu = pmb->cie;
    ju = pmb->cje;
    ku = pmb->cke;
    ng = NGHOST;
    nc1 = pmy_block->ncc1, nc2 = pmy_block->ncc2, nc3 = pmy_block->ncc3;
  } else {
    il = pmb->is;
    jl = pmb->js;
    kl = pmb->ks;
    iu = pmb->ie;
    ju = pmb->je;
    ku = pmb->ke;
    ng = NGHOST;
    nc1 = pmy_block->ncells1, nc2 = pmy_block->ncells2, nc3 = pmy_block->ncells3;
  }

  // allocate arrays for volume-centered coordinates and positions of cells
  dx1v = ParArrayND<Real>(PARARRAY_TEMP, nc1);
  dx2v = ParArrayND<Real>(PARARRAY_TEMP, nc2);
  dx3v = ParArrayND<Real>(PARARRAY_TEMP, nc3);
  x1v = ParArrayND<Real>(PARARRAY_TEMP, nc1);
  x2v = ParArrayND<Real>(PARARRAY_TEMP, nc2);
  x3v = ParArrayND<Real>(PARARRAY_TEMP, nc3);
  // allocate arrays for face-centered coordinates and coordinate spacing
  // (note extra cell for face-positions)
  dx1f = ParArrayND<Real>(PARARRAY_TEMP, nc1);
  dx2f = ParArrayND<Real>(PARARRAY_TEMP, nc2);
  dx3f = ParArrayND<Real>(PARARRAY_TEMP, nc3);
  x1f = ParArrayND<Real>(PARARRAY_TEMP, nc1 + 1);
  x2f = ParArrayND<Real>(PARARRAY_TEMP, nc2 + 1);
  x3f = ParArrayND<Real>(PARARRAY_TEMP, nc3 + 1);

  // allocate arrays for volume- and face-centered geometry coefficients of cells
  // (only for spherical-polar, cylindrical, cartesian coordinates, for now)
  h2f = ParArrayND<Real>(PARARRAY_TEMP, nc1);
  dh2fd1 = ParArrayND<Real>(PARARRAY_TEMP, nc1);
  h31f = ParArrayND<Real>(PARARRAY_TEMP, nc1);
  dh31fd1 = ParArrayND<Real>(PARARRAY_TEMP, nc1);
  h32f = ParArrayND<Real>(PARARRAY_TEMP, nc2);
  dh32fd2 = ParArrayND<Real>(PARARRAY_TEMP, nc2);
  h2v = ParArrayND<Real>(PARARRAY_TEMP, nc1);
  dh2vd1 = ParArrayND<Real>(PARARRAY_TEMP, nc1);
  h31v = ParArrayND<Real>(PARARRAY_TEMP, nc1);
  dh31vd1 = ParArrayND<Real>(PARARRAY_TEMP, nc1);
  h32v = ParArrayND<Real>(PARARRAY_TEMP, nc2);
  dh32vd2 = ParArrayND<Real>(PARARRAY_TEMP, nc2);

  std::int64_t nrootmesh, noffset;
  std::int64_t &lx1 = pmy_block->loc.lx1;
  int &ll = pmy_block->loc.level;

  //--- X1-DIRECTION: initialize coordinates and spacing of cell FACES (x1f,dx1f)

  nrootmesh = mesh_size.nx1 * (1L << (ll - pm->root_level));

  // use nonuniform or user-defined meshgen fn
  if (!(pm->use_uniform_meshgen_fn_[X1DIR])) {
    for (int i = il - ng; i <= iu + ng + 1; ++i) {
      // if there are too many levels, this won't work or be precise enough
      if (!coarse_flag) {
        noffset = static_cast<std::int64_t>(i - il + lx1 * block_size.nx1);
      } else {
        noffset = static_cast<std::int64_t>((i - il) * 2 + lx1 * block_size.nx1);
      }
      Real rx = ComputeMeshGeneratorX(noffset, nrootmesh, false);
      x1f(i) = pm->MeshGenerator_[X1DIR](rx, mesh_size);
    }
    x1f(il) = block_size.x1min;
    x1f(iu + 1) = block_size.x1max;
    for (int i = il - ng; i <= iu + ng; ++i) {
      dx1f(i) = x1f(i + 1) - x1f(i);
    }

    // check that coordinate spacing is reasonable
    if (!coarse_flag) {
      Real rmax = 1.0, rmin = 1.0;
      for (int i = il; i <= iu - 1; i++) {
        rmax = std::max(dx1f(i + 1) / dx1f(i), rmax);
        rmin = std::min(dx1f(i + 1) / dx1f(i), rmin);
      }
      if (rmax > 1.1 || rmin < 1.0 / 1.1) {
        std::cout << "### Warning in Coordinates constructor" << std::endl
                  << "Neighboring cell sizes differ by more than 10% in the x1 direction."
                  << std::endl;
      }
    }

  } else {
    // uniform grid: use UniformMeshGeneratorX1()
    Real dx = (block_size.x1max - block_size.x1min) / (iu - il + 1);
    for (int i = il - ng; i <= iu + ng + 1; ++i) {
      // if there are too many levels, this won't work or be precise enough
      if (!coarse_flag) {
        noffset = static_cast<std::int64_t>(i - il + lx1 * block_size.nx1);
      } else {
        noffset = static_cast<std::int64_t>((i - il) * 2 + lx1 * block_size.nx1);
      }
      Real rx = ComputeMeshGeneratorX(noffset, nrootmesh, true);
      x1f(i) = pm->MeshGenerator_[X1DIR](rx, mesh_size);
    }
    x1f(il) = block_size.x1min;
    x1f(iu + 1) = block_size.x1max;

    for (int i = il - ng; i <= iu + ng; ++i) {
      dx1f(i) = dx;
    }
  }

  // correct cell face coordinates in ghost zones for reflecting boundary condition
  if (pmy_block->pbval->block_bcs[BoundaryFace::inner_x1] == BoundaryFlag::reflect) {
    for (int i = 1; i <= ng; ++i) {
      dx1f(il - i) = dx1f(il + i - 1);
      x1f(il - i) = x1f(il - i + 1) - dx1f(il - i);
    }
  }
  if (pmy_block->pbval->block_bcs[BoundaryFace::outer_x1] == BoundaryFlag::reflect) {
    for (int i = 1; i <= ng; ++i) {
      dx1f(iu + i) = dx1f(iu - i + 1);
      x1f(iu + i + 1) = x1f(iu + i) + dx1f(iu + i);
    }
  }

  //--- X2-DIRECTION: initialize coordinates and spacing of cell FACES (x2f,dx2f)

  if (nc2 > 1) {
    std::int64_t &lx2 = pmy_block->loc.lx2;
    nrootmesh = mesh_size.nx2 * (1L << (ll - pm->root_level));
    // use nonuniform or user-defined meshgen fn
    if (!(pm->use_uniform_meshgen_fn_[X2DIR])) {
      for (int j = jl - ng; j <= ju + ng + 1; ++j) {
        // if there are too many levels, this won't work or be precise enough
        if (!coarse_flag) {
          noffset = static_cast<std::int64_t>(j - jl + lx2 * block_size.nx2);
        } else {
          noffset = static_cast<std::int64_t>((j - jl) * 2 + lx2 * block_size.nx2);
        }
        Real rx = ComputeMeshGeneratorX(noffset, nrootmesh, false);
        x2f(j) = pm->MeshGenerator_[X2DIR](rx, mesh_size);
      }
      x2f(jl) = block_size.x2min;
      x2f(ju + 1) = block_size.x2max;
      for (int j = jl - ng; j <= ju + ng; ++j) {
        dx2f(j) = x2f(j + 1) - x2f(j);
      }

      // check that coordinate spacing is reasonable
      if (!coarse_flag) {
        Real rmax = 1.0, rmin = 1.0;
        for (int j = jl; j <= ju - 1; j++) {
          rmax = std::max(dx2f(j + 1) / dx2f(j), rmax);
          rmin = std::min(dx2f(j + 1) / dx2f(j), rmin);
        }
        if (rmax > 1.1 || rmin < 1.0 / 1.1) {
          std::cout
              << "### Warning in Coordinates constructor" << std::endl
              << "Neighboring cell sizes differ by more than 10% in the x2 direction."
              << std::endl;
        }
      }

    } else {
      // uniform grid: use UniformMeshGeneratorX2()
      Real dx = (block_size.x2max - block_size.x2min) / (ju - jl + 1);
      for (int j = jl - ng; j <= ju + ng + 1; ++j) {
        if (!coarse_flag) {
          noffset = static_cast<std::int64_t>(j - jl + lx2 * block_size.nx2);
        } else {
          noffset = static_cast<std::int64_t>((j - jl) * 2 + lx2 * block_size.nx2);
        }
        Real rx = ComputeMeshGeneratorX(noffset, nrootmesh, true);
        x2f(j) = pm->MeshGenerator_[X2DIR](rx, mesh_size);
      }
      x2f(jl) = block_size.x2min;
      x2f(ju + 1) = block_size.x2max;

      for (int j = jl - ng; j <= ju + ng; ++j) {
        dx2f(j) = dx;
      }
    }

    // correct cell face coordinates in ghost zones for reflect bndry condition
    if (pmy_block->pbval->block_bcs[BoundaryFace::inner_x2] == BoundaryFlag::reflect) {
      for (int j = 1; j <= ng; ++j) {
        dx2f(jl - j) = dx2f(jl + j - 1);
        x2f(jl - j) = x2f(jl - j + 1) - dx2f(jl - j);
      }
    }
    if (pmy_block->pbval->block_bcs[BoundaryFace::outer_x2] == BoundaryFlag::reflect) {
      for (int j = 1; j <= ng; ++j) {
        dx2f(ju + j) = dx2f(ju - j + 1);
        x2f(ju + j + 1) = x2f(ju + j) + dx2f(ju + j);
      }
    }

    // 1D problem
  } else {
    dx2f(jl) = block_size.x2max - block_size.x2min;
    x2f(jl) = block_size.x2min;
    x2f(ju + 1) = block_size.x2max;
  }

  //--- X3-DIRECTION: initialize coordinates and spacing of cell FACES (x3f,dx3f)

  if (nc3 > 1) {
    std::int64_t &lx3 = pmy_block->loc.lx3;
    nrootmesh = mesh_size.nx3 * (1L << (ll - pm->root_level));

    // use nonuniform or user-defined meshgen fn
    if (!(pm->use_uniform_meshgen_fn_[X3DIR])) {
      for (int k = kl - ng; k <= ku + ng + 1; ++k) {
        // if there are too many levels, this won't work or be precise enough
        if (!coarse_flag) {
          noffset = static_cast<std::int64_t>(k - kl + lx3 * block_size.nx3);
        } else {
          noffset = static_cast<std::int64_t>((k - kl) * 2 + lx3 * block_size.nx3);
        }
        Real rx = ComputeMeshGeneratorX(noffset, nrootmesh, false);
        x3f(k) = pm->MeshGenerator_[X3DIR](rx, mesh_size);
      }
      x3f(kl) = block_size.x3min;
      x3f(ku + 1) = block_size.x3max;
      for (int k = kl - ng; k <= ku + ng; ++k) {
        dx3f(k) = x3f(k + 1) - x3f(k);
      }

      // check that coordinate spacing is reasonable
      if (!coarse_flag) {
        Real rmax = 1.0, rmin = 1.0;
        for (int k = kl; k <= ku - 1; k++) {
          rmax = std::max(dx3f(k + 1) / dx3f(k), rmax);
          rmin = std::min(dx3f(k + 1) / dx3f(k), rmin);
        }
        if (rmax > 1.1 || rmin < 1.0 / 1.1) {
          std::cout
              << "### Warning in Coordinates constructor" << std::endl
              << "Neighboring cell sizes differ by more than 10% in the x3 direction."
              << std::endl;
        }
      }
    } else {
      // uniform grid: use UniformMeshGeneratorX3()
      Real dx = (block_size.x3max - block_size.x3min) / (ku - kl + 1);
      for (int k = kl - ng; k <= ku + ng + 1; ++k) {
        if (!coarse_flag) {
          noffset = static_cast<std::int64_t>(k - kl + lx3 * block_size.nx3);
        } else {
          noffset = static_cast<std::int64_t>((k - kl) * 2 + lx3 * block_size.nx3);
        }
        Real rx = ComputeMeshGeneratorX(noffset, nrootmesh, true);
        x3f(k) = pm->MeshGenerator_[X3DIR](rx, mesh_size);
      }
      x3f(kl) = block_size.x3min;
      x3f(ku + 1) = block_size.x3max;

      for (int k = kl - ng; k <= ku + ng; ++k) {
        dx3f(k) = dx;
      }
    }

    // correct cell face coordinates in ghost zones for reflecting boundary condition
    if (pmy_block->pbval->block_bcs[BoundaryFace::inner_x3] == BoundaryFlag::reflect) {
      for (int k = 1; k <= ng; ++k) {
        dx3f(kl - k) = dx3f(kl + k - 1);
        x3f(kl - k) = x3f(kl - k + 1) - dx3f(kl - k);
      }
    }
    if (pmy_block->pbval->block_bcs[BoundaryFace::outer_x3] == BoundaryFlag::reflect) {
      for (int k = 1; k <= ng; ++k) {
        dx3f(ku + k) = dx3f(ku - k + 1);
        x3f(ku + k + 1) = x3f(ku + k) + dx3f(ku + k);
      }
    }

    // 1D or 2D problem
  } else {
    dx3f(kl) = block_size.x3max - block_size.x3min;
    x3f(kl) = block_size.x3min;
    x3f(ku + 1) = block_size.x3max;
  }
}

//----------------------------------------------------------------------------------------
// EdgeXLength functions: compute physical length at cell edge-X as vector
// Edge1(i,j,k) located at (i,j-1/2,k-1/2), i.e. (x1v(i), x2f(j), x3f(k))

void Coordinates::Edge1Length(const int k, const int j, const int il, const int iu,
                              ParArrayND<Real> &len) {
#pragma omp simd
  for (int i = il; i <= iu; ++i) {
    len(i) = dx1f(i);
  }
  return;
}

// Edge2(i,j,k) located at (i-1/2,j,k-1/2), i.e. (x1f(i), x2v(j), x3f(k))

void Coordinates::Edge2Length(const int k, const int j, const int il, const int iu,
                              ParArrayND<Real> &len) {
#pragma omp simd
  for (int i = il; i <= iu; ++i) {
    len(i) = dx2f(j);
  }
  return;
}

// Edge3(i,j,k) located at (i-1/2,j-1/2,k), i.e. (x1f(i), x2f(j), x3v(k))

void Coordinates::Edge3Length(const int k, const int j, const int il, const int iu,
                              ParArrayND<Real> &len) {
#pragma omp simd
  for (int i = il; i <= iu; ++i) {
    len(i) = dx3f(k);
  }
  return;
}

//----------------------------------------------------------------------------------------
// GetEdgeXLength functions: return length of edge-X at (i,j,k)

Real Coordinates::GetEdge1Length(const int k, const int j, const int i) {
  return dx1f(i);
}

Real Coordinates::GetEdge2Length(const int k, const int j, const int i) {
  return dx2f(j);
}

Real Coordinates::GetEdge3Length(const int k, const int j, const int i) {
  return dx3f(k);
}

//----------------------------------------------------------------------------------------
// VolCenterXLength functions: compute physical length connecting cell centers as vector
// VolCenter1(i,j,k) located at (i+1/2,j,k), i.e. (x1f(i+1), x2v(j), x3v(k))
void Coordinates::VolCenter1Length(const int k, const int j, const int il, const int iu,
                                   ParArrayND<Real> &len) {
#pragma omp simd
  for (int i = il; i <= iu; ++i) {
    len(i) = dx1v(i);
  }
  return;
}
void Coordinates::VolCenter2Length(const int k, const int j, const int il, const int iu,
                                   ParArrayND<Real> &len) {
#pragma omp simd
  for (int i = il; i <= iu; ++i) {
    len(i) = dx2v(j);
  }
  return;
}
void Coordinates::VolCenter3Length(const int k, const int j, const int il, const int iu,
                                   ParArrayND<Real> &len) {
#pragma omp simd
  for (int i = il; i <= iu; ++i) {
    len(i) = dx3v(k);
  }
  return;
}

//----------------------------------------------------------------------------------------
// CenterWidthX functions: return physical width in X-dir at (i,j,k) cell-center

void Coordinates::CenterWidth1(const int k, const int j, const int il, const int iu,
                               ParArrayND<Real> &dx1) {
#pragma omp simd
  for (int i = il; i <= iu; ++i) {
    dx1(i) = dx1f(i);
  }
  return;
}

void Coordinates::CenterWidth2(const int k, const int j, const int il, const int iu,
                               ParArrayND<Real> &dx2) {
#pragma omp simd
  for (int i = il; i <= iu; ++i) {
    dx2(i) = dx2f(j);
  }
  return;
}

void Coordinates::CenterWidth3(const int k, const int j, const int il, const int iu,
                               ParArrayND<Real> &dx3) {
#pragma omp simd
  for (int i = il; i <= iu; ++i) {
    dx3(i) = dx3f(k);
  }
  return;
}

//----------------------------------------------------------------------------------------
// FaceXArea functions: compute area of face with normal in X-dir as vector

void Coordinates::Face1Area(const int k, const int j, const int il, const int iu,
                            ParArrayND<Real> &area) {
#pragma nounroll
  for (int i = il; i <= iu; ++i) {
    // area1 = dy dz
    Real &area_i = area(i);
    area_i = dx2f(j) * dx3f(k);
  }
  return;
}

void Coordinates::Face2Area(const int k, const int j, const int il, const int iu,
                            ParArrayND<Real> &area) {
#pragma nounroll
  for (int i = il; i <= iu; ++i) {
    // area2 = dx dz
    Real &area_i = area(i);
    area_i = dx1f(i) * dx3f(k);
  }
  return;
}

void Coordinates::Face3Area(const int k, const int j, const int il, const int iu,
                            ParArrayND<Real> &area) {
#pragma nounroll
  for (int i = il; i <= iu; ++i) {
    // area3 = dx dy
    Real &area_i = area(i);
    area_i = dx1f(i) * dx2f(j);
  }
  return;
}

//----------------------------------------------------------------------------------------
// GetFaceXArea functions: return area of face with normal in X-dir at (i,j,k)

Real Coordinates::GetFace1Area(const int k, const int j, const int i) {
  return dx2f(j) * dx3f(k);
}

Real Coordinates::GetFace2Area(const int k, const int j, const int i) {
  return dx1f(i) * dx3f(k);
}

Real Coordinates::GetFace3Area(const int k, const int j, const int i) {
  return dx1f(i) * dx2f(j);
}

//----------------------------------------------------------------------------------------
// VolCenterFaceXArea functions: compute area of face with normal in X-dir as vector
// where the faces are joined by cell centers (for non-ideal MHD)

void Coordinates::VolCenterFace1Area(const int k, const int j, const int il, const int iu,
                                     ParArrayND<Real> &area) {
#pragma omp simd
  for (int i = il; i <= iu; ++i) {
    Real &area_i = area(i);
    area_i = dx2v(j) * dx3v(k);
  }
  return;
}

void Coordinates::VolCenterFace2Area(const int k, const int j, const int il, const int iu,
                                     ParArrayND<Real> &area) {
#pragma omp simd
  for (int i = il; i <= iu; ++i) {
    Real &area_i = area(i);
    area_i = dx1v(i) * dx3v(k);
  }
  return;
}

void Coordinates::VolCenterFace3Area(const int k, const int j, const int il, const int iu,
                                     ParArrayND<Real> &area) {
#pragma omp simd
  for (int i = il; i <= iu; ++i) {
    Real &area_i = area(i);
    area_i = dx1v(i) * dx2v(j);
  }
  return;
}

//----------------------------------------------------------------------------------------
// Cell Volume function: compute volume of cell as vector

void Coordinates::CellVolume(const int k, const int j, const int il, const int iu,
                             ParArrayND<Real> &vol) {
#pragma omp simd
  for (int i = il; i <= iu; ++i) {
    // volume = dx dy dz
    Real &vol_i = vol(i);
    vol_i = dx1f(i) * dx2f(j) * dx3f(k);
  }
  return;
}

//----------------------------------------------------------------------------------------
// GetCellVolume: returns cell volume at (i,j,k)

Real Coordinates::GetCellVolume(const int k, const int j, const int i) {
  return dx1f(i) * dx2f(j) * dx3f(k);
}

//-------------------------------------------------------------------------------------
// Laplacian: calculate total Laplacian of 4D scalar array s() to second order accuracy
// may need to replace dx*f with dx*v for nonuniform coordinates for some applications

void Coordinates::Laplacian(const ParArrayND<Real> &s, ParArrayND<Real> &delta_s,
                            const int il, const int iu, const int jl, const int ju,
                            const int kl, const int ku, const int nl, const int nu) {
  for (int n = nl; n <= nu; ++n) {
    for (int k = kl; k <= ku; ++k) {
      for (int j = jl; j <= ju; ++j) {
#pragma omp simd
        for (int i = il; i <= iu; ++i) {
          delta_s(n, k, j, i) =
              (s(n, k, j, i - 1) - 2.0 * s(n, k, j, i) + s(n, k, j, i + 1)) /
              (dx1f(i) * dx1f(i));
        }
        if (pmy_block->block_size.nx2 > 1) {
#pragma omp simd
          for (int i = il; i <= iu; ++i) {
            delta_s(n, k, j, i) +=
                (s(n, k, j - 1, i) - 2.0 * s(n, k, j, i) + s(n, k, j + 1, i)) /
                (dx2f(j) * dx2f(j));
          }
        }
        if (pmy_block->block_size.nx3 > 1) {
#pragma omp simd
          for (int i = il; i <= iu; ++i) {
            delta_s(n, k, j, i) +=
                (s(n, k - 1, j, i) - 2.0 * s(n, k, j, i) + s(n, k + 1, j, i)) /
                (dx3f(k) * dx3f(k));
          }
        }
      }
    }
  }
  return;
}

//-------------------------------------------------------------------------------------
// LaplacianX* functions: calculate Laplacian in subspaces orthogonal to X-dir

void Coordinates::LaplacianX1(const ParArrayND<Real> &s, ParArrayND<Real> &delta_s,
                              const int n, const int k, const int j, const int il,
                              const int iu) {
  if (pmy_block->block_size.nx3 > 1) {
#pragma omp simd
    for (int i = il; i <= iu; ++i) {
      delta_s(i) = (s(n, k, j - 1, i) - 2.0 * s(n, k, j, i) + s(n, k, j + 1, i)) /
                       (dx2f(j) * dx2f(j)) +
                   (s(n, k - 1, j, i) - 2.0 * s(n, k, j, i) + s(n, k + 1, j, i)) /
                       (dx3f(k) * dx3f(k));
    }
  } else if (pmy_block->block_size.nx2 > 1) {
#pragma omp simd
    for (int i = il; i <= iu; ++i) {
      delta_s(i) = (s(n, k, j - 1, i) - 2.0 * s(n, k, j, i) + s(n, k, j + 1, i)) /
                   (dx2f(j) * dx2f(j));
    }
  } else {
#pragma omp simd
    for (int i = il; i <= iu; ++i) {
      delta_s(i) = 0.0;
    }
  }
}

void Coordinates::LaplacianX1All(const ParArrayND<Real> &s, ParArrayND<Real> &delta_s,
                                 const int nl, const int nu, const int kl, const int ku,
                                 const int jl, const int ju, const int il, const int iu) {
  if (pmy_block->block_size.nx3 > 1) {
    for (int n = nl; n <= nu; ++n) {
      for (int k = kl; k <= ku; ++k) {
        for (int j = jl; j <= ju; ++j) {
#pragma omp simd
          for (int i = il; i <= iu; ++i) {
            delta_s(n, k, j, i) =
                (s(n, k, j - 1, i) - 2.0 * s(n, k, j, i) + s(n, k, j + 1, i)) /
                    (dx2f(j) * dx2f(j)) +
                (s(n, k - 1, j, i) - 2.0 * s(n, k, j, i) + s(n, k + 1, j, i)) /
                    (dx3f(k) * dx3f(k));
          }
        }
      }
    }
  } else if (pmy_block->block_size.nx2 > 1) {
    for (int n = nl; n <= nu; ++n) {
      for (int k = kl; k <= ku; ++k) {
        for (int j = jl; j <= ju; ++j) {
#pragma omp simd
          for (int i = il; i <= iu; ++i) {
            delta_s(n, k, j, i) =
                (s(n, k, j - 1, i) - 2.0 * s(n, k, j, i) + s(n, k, j + 1, i)) /
                (dx2f(j) * dx2f(j));
          }
        }
      }
    }
  } else {
    for (int n = nl; n <= nu; ++n) {
      for (int k = kl; k <= ku; ++k) {
        for (int j = jl; j <= ju; ++j) {
#pragma omp simd
          for (int i = il; i <= iu; ++i) {
            delta_s(n, k, j, i) = 0.0;
          }
        }
      }
    }
  }
  return;
}

void Coordinates::LaplacianX2(const ParArrayND<Real> &s, ParArrayND<Real> &delta_s,
                              const int n, const int k, const int j, const int il,
                              const int iu) {
  if (pmy_block->block_size.nx3 > 1) {
#pragma omp simd
    for (int i = il; i <= iu; ++i) {
      delta_s(i) = (s(n, k, j, i - 1) - 2.0 * s(n, k, j, i) + s(n, k, j, i + 1)) /
                       (dx1f(i) * dx1f(i)) +
                   (s(n, k - 1, j, i) - 2.0 * s(n, k, j, i) + s(n, k + 1, j, i)) /
                       (dx3f(k) * dx3f(k));
    }
  } else {
#pragma omp simd
    for (int i = il; i <= iu; ++i) {
      delta_s(i) = (s(n, k, j, i - 1) - 2.0 * s(n, k, j, i) + s(n, k, j, i + 1)) /
                   (dx1f(i) * dx1f(i));
    }
  }
}

void Coordinates::LaplacianX2All(const ParArrayND<Real> &s, ParArrayND<Real> &delta_s,
                                 const int nl, const int nu, const int kl, const int ku,
                                 const int jl, const int ju, const int il, const int iu) {
  if (pmy_block->block_size.nx3 > 1) {
    for (int n = nl; n <= nu; ++n) {
      for (int k = kl; k <= ku; ++k) {
        for (int j = jl; j <= ju; ++j) {
#pragma omp simd
          for (int i = il; i <= iu; ++i) {
            delta_s(n, k, j, i) =
                (s(n, k, j, i - 1) - 2.0 * s(n, k, j, i) + s(n, k, j, i + 1)) /
                    (dx1f(i) * dx1f(i)) +
                (s(n, k - 1, j, i) - 2.0 * s(n, k, j, i) + s(n, k + 1, j, i)) /
                    (dx3f(k) * dx3f(k));
          }
        }
      }
    }
  } else {
    for (int n = nl; n <= nu; ++n) {
      for (int k = kl; k <= ku; ++k) {
        for (int j = jl; j <= ju; ++j) {
#pragma omp simd
          for (int i = il; i <= iu; ++i) {
            delta_s(n, k, j, i) =
                (s(n, k, j, i - 1) - 2.0 * s(n, k, j, i) + s(n, k, j, i + 1)) /
                (dx1f(i) * dx1f(i));
          }
        }
      }
    }
  }
  return;
}

void Coordinates::LaplacianX3(const ParArrayND<Real> &s, ParArrayND<Real> &delta_s,
                              const int n, const int k, const int j, const int il,
                              const int iu) {
  if (pmy_block->block_size.nx2 > 1) {
#pragma omp simd
    for (int i = il; i <= iu; ++i) {
      delta_s(i) = (s(n, k, j, i - 1) - 2.0 * s(n, k, j, i) + s(n, k, j, i + 1)) /
                       (dx1f(i) * dx1f(i)) +
                   (s(n, k, j - 1, i) - 2.0 * s(n, k, j, i) + s(n, k, j + 1, i)) /
                       (dx2f(j) * dx2f(j));
    }
  } else {
#pragma omp simd
    for (int i = il; i <= iu; ++i) {
      delta_s(i) = (s(n, k, j, i - 1) - 2.0 * s(n, k, j, i) + s(n, k, j, i + 1)) /
                   (dx1f(i) * dx1f(i));
    }
  }
  return;
}

void Coordinates::LaplacianX3All(const ParArrayND<Real> &s, ParArrayND<Real> &delta_s,
                                 const int nl, const int nu, const int kl, const int ku,
                                 const int jl, const int ju, const int il, const int iu) {
  if (pmy_block->block_size.nx2 > 1) {
    for (int n = nl; n <= nu; ++n) {
      for (int k = kl; k <= ku; ++k) {
        for (int j = jl; j <= ju; ++j) {
#pragma omp simd
          for (int i = il; i <= iu; ++i) {
            delta_s(n, k, j, i) =
                (s(n, k, j, i - 1) - 2.0 * s(n, k, j, i) + s(n, k, j, i + 1)) /
                    (dx1f(i) * dx1f(i)) +
                (s(n, k, j - 1, i) - 2.0 * s(n, k, j, i) + s(n, k, j + 1, i)) /
                    (dx2f(j) * dx2f(j));
          }
        }
      }
    }
  } else {
    for (int n = nl; n <= nu; ++n) {
      for (int k = kl; k <= ku; ++k) {
        for (int j = jl; j <= ju; ++j) {
#pragma omp simd
          for (int i = il; i <= iu; ++i) {
            delta_s(n, k, j, i) =
                (s(n, k, j, i - 1) - 2.0 * s(n, k, j, i) + s(n, k, j, i + 1)) /
                (dx1f(i) * dx1f(i));
          }
        }
      }
    }
  }
  return;
}

//----------------------------------------------------------------------------------------
// Coordinate (Geometric) source term function
void Coordinates::AddCoordTermsDivergence(const Real dt, const ParArrayND<Real> *flux,
                                          const ParArrayND<Real> &prim,
                                          const ParArrayND<Real> &bcc,
                                          ParArrayND<Real> &u) {
  return;
}

//----------------------------------------------------------------------------------------
// Function for implementing user-defined metric
// Inputs:
//   x1,x2,x3: spatial location of point
//   pin: pointer to runtime inputs
// Outputs:
//   g,g_inv: arrays of metric covariant and contravariant components
//   dg_dx1,dg_dx2,dg_dx3: arrays of spatial derivatives of covariant components

void Coordinates::Metric(Real x1, Real x2, Real x3, ParameterInput *pin,
                         ParArrayND<Real> &g, ParArrayND<Real> &g_inv,
                         ParArrayND<Real> &dg_dx1, ParArrayND<Real> &dg_dx2,
                         ParArrayND<Real> &dg_dx3) {
  pmy_block->pmy_mesh->UserMetric_(x1, x2, x3, pin, g, g_inv, dg_dx1, dg_dx2, dg_dx3);
  return;
}

} // namespace parthenon
