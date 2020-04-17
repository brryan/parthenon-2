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
#ifndef EXAMPLE_PARTICLES_PARTICLES_HPP_
#define EXAMPLE_PARTICLES_PARTICLES_HPP_

#include <memory>

#include "driver/driver.hpp"
#include "driver/multistage.hpp"
#include "interface/container.hpp"
#include "interface/state_descriptor.hpp"
#include "mesh/mesh.hpp"
#include "task_list/tasks.hpp"

using parthenon::AmrTag;
using parthenon::BaseTask;
using parthenon::Container;
using parthenon::SwarmContainer;
using parthenon::Swarm;
using parthenon::Mesh;
using parthenon::MeshBlock;
using parthenon::MultiStageBlockTaskDriver;
using parthenon::Outputs;
using parthenon::ParameterInput;
using parthenon::Real;
using parthenon::StateDescriptor;
using parthenon::TaskID;
using parthenon::TaskList;
using parthenon::TaskStatus;

namespace particles_example {

class ParticleDriver : public MultiStageBlockTaskDriver {
 public:
  ParticleDriver(ParameterInput *pin, Mesh *pm, Outputs *pout)
      : MultiStageBlockTaskDriver(pin, pm, pout) {}
  // This next function essentially defines the driver.
  // Call graph looks like
  // main()
  //   MultiStageBlockTaskDriver::Execute (driver.cpp)
  //     MultiStageBlockTaskDriver::Step (driver.cpp)
  //       DriverUtils::ConstructAndExecuteBlockTasks (driver.hpp)
  //         AdvectionDriver::MakeTaskList (advection.cpp)
  TaskList MakeTaskList(MeshBlock *pmb, int stage);
};

// demonstrate making a custom Task type
/*using ContainerTaskFunc = std::function<TaskStatus(Container<Real> &)>;
class ContainerTask : public BaseTask {
 public:
  ContainerTask(TaskID id, ContainerTaskFunc func, TaskID dep, Container<Real> rc)
      : BaseTask(id, dep), _func(func), _cont(rc) {}
  TaskStatus operator()() { return _func(_cont); }

 private:
  ContainerTaskFunc _func;
  Container<Real> _cont;
};
*/
using TwoSwarmTaskFunc =
    std::function<TaskStatus(Swarm &, Swarm &)>;
class TwoSwarmTask : public BaseTask {
 public:
  TwoSwarmTask(TaskID id, TwoSwarmTaskFunc func, TaskID dep, Swarm s1,
                   Swarm s2)
      : BaseTask(id, dep), _func(func), _swarm1(s1), _swarm2(s2) {}
  TaskStatus operator()() { return _func(_swarm1, _swarm2); }

 private:
  TwoSwarmTaskFunc _func;
  Swarm _swarm1;
  Swarm _swarm2;
};

namespace Particles {

std::shared_ptr<StateDescriptor> Initialize(ParameterInput *pin);
AmrTag CheckRefinement(Container<Real> &rc);
void PreFill(Container<Real> &rc);
void SquareIt(Container<Real> &rc);
void PostFill(Container<Real> &rc);
Real EstimateTimestep(Container<Real> &rc);
//TaskStatus CalculateFluxes(Container<Real> &rc);

} // namespace Advection

} // namespace advection_example

#endif // EXAMPLE_ADVECTION_ADVECTION_HPP_
