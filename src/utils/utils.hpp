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
#ifndef UTILS_UTILS_HPP_
#define UTILS_UTILS_HPP_
//! \file utils.hpp
//  \brief prototypes of functions and class definitions for utils/*.cpp files

#include <csignal>
#include <cstdint>

namespace parthenon {

void ChangeRunDir(const char *pdir);
double ran2(std::int64_t *idum);
void ShowConfig();

//----------------------------------------------------------------------------------------
//! SignalHandler
//  \brief static data and functions that implement a simple signal handling system
namespace SignalHandler {

const int nsignal = 3;
static volatile int signalflag[nsignal];
const int ITERM = 0, IINT = 1, IALRM = 2;
static sigset_t mask;
void SignalHandlerInit();
int CheckSignalFlags();
int GetSignalFlag(int s);
void SetSignalFlag(int s);
void SetWallTimeAlarm(int t);
void CancelWallTimeAlarm();
void Report();

} // namespace SignalHandler

} // namespace parthenon

#endif // UTILS_UTILS_HPP_
