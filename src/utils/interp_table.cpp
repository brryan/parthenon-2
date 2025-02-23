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
//! \file interp_table.cpp
//  \brief implements functions in class InterpTable2D an intpolated lookup table

#include <cmath>
#include <stdexcept>

#include "athena.hpp"
#include "coordinates/coordinates.hpp"
#include "parthenon_arrays.hpp"
#include "utils/interp_table.hpp"

namespace parthenon {

// A contructor that setts the size of the table with number of variables nvar
// and dimensions nx2 x nx1 (interpolated dimensions)
InterpTable2D::InterpTable2D(const int nvar, const int nx2, const int nx1) {
  SetSize(nvar, nx2, nx1);
}

// Set size of table
void InterpTable2D::SetSize(const int nvar, const int nx2, const int nx1) {
  nvar_ = nvar; // number of variables/tables
  nx2_ = nx2;   // slower indexing dimension
  nx1_ = nx1;   // faster indexing dimension
  data = ParArrayND<Real>(PARARRAY_TEMP, nvar, nx2, nx1);
}

// Set the corrdinate limits for x1
void InterpTable2D::SetX1lim(Real x1min, Real x1max) {
  x1min_ = x1min;
  x1max_ = x1max;
  x1norm_ = (nx1_ - 1) / (x1max - x1min);
}

// Set the corrdinate limits for x2
void InterpTable2D::SetX2lim(Real x2min, Real x2max) {
  x2min_ = x2min;
  x2max_ = x2max;
  x2norm_ = (nx2_ - 1) / (x2max - x2min);
}

void InterpTable2D::GetX1lim(Real &x1min, Real &x1max) {
  x1min = x1min_;
  x1max = x1max_;
}

void InterpTable2D::GetX2lim(Real &x2min, Real &x2max) {
  x2min = x2min_;
  x2max = x2max_;
}

void InterpTable2D::GetSize(int &nvar, int &nx2, int &nx1) {
  nvar = nvar_;
  nx2 = nx2_;
  nx1 = nx1_;
}

// Bilinear interpolation
Real InterpTable2D::interpolate(int var, Real x2, Real x1) {
  Real x, y, xrl, yrl, out;
  x = (x2 - x2min_) * x2norm_;
  y = (x1 - x1min_) * x1norm_;
  int xil = static_cast<int>(x); // lower x index
  int yil = static_cast<int>(y); // lower y index
  int nx = nx2_;
  int ny = nx1_;
  // if off table, do linear extrapolation
  if (xil < 0) { // below xmin
    xil = 0;
  } else if (xil >= nx - 1) { // above xmax
    xil = nx - 2;
  }
  xrl = 1 + xil - x; // x residual

  if (yil < 0) { // below ymin
    yil = 0;
  } else if (yil >= ny - 1) { // above ymax
    yil = ny - 2;
  }
  yrl = 1 + yil - y; // y residual

  // Sample from the 4 nearest data points and weight appropriately
  // data is an attribute of the eos class
  out = xrl * yrl * data(var, xil, yil) + xrl * (1 - yrl) * data(var, xil, yil + 1) +
        (1 - xrl) * yrl * data(var, xil + 1, yil) +
        (1 - xrl) * (1 - yrl) * data(var, xil + 1, yil + 1);
  return out;
}

} // namespace parthenon
