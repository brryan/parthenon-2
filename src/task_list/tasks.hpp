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

#ifndef TASK_LIST_TASKS_HPP_
#define TASK_LIST_TASKS_HPP_

#include <bitset>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "basic_types.hpp"

namespace parthenon {

class MeshBlock;
struct Integrator;

enum class TaskListStatus { running, stuck, complete, nothing_to_do };

using SimpleTaskFunc = std::function<TaskStatus()>;
using BlockTaskFunc = std::function<TaskStatus(MeshBlock *)>;
using BlockStageTaskFunc = std::function<TaskStatus(MeshBlock *, int)>;
using BlockStageNamesTaskFunc =
    std::function<TaskStatus(MeshBlock *, int, std::vector<std::string> &)>;
using BlockStageNamesIntegratorTaskFunc =
    std::function<TaskStatus(MeshBlock *, int, std::vector<std::string> &, Integrator *)>;

//----------------------------------------------------------------------------------------
//! \class TaskID
//  \brief generalization of bit fields for Task IDs, status, and dependencies.

#define BITBLOCK 16

class TaskID {
 public:
  TaskID() { Set(0); }
  explicit TaskID(int id);

  void Set(int id);
  void clear();
  bool CheckDependencies(const TaskID &rhs) const;
  void SetFinished(const TaskID &rhs);
  bool operator==(const TaskID &rhs) const;
  TaskID operator|(const TaskID &rhs) const;
  std::string to_string();

 private:
  std::vector<std::bitset<BITBLOCK>> bitblocks;
};

class BaseTask {
 public:
  BaseTask(TaskID id, TaskID dep) : _myid(id), _dep(dep) {}
  virtual ~BaseTask() = default;
  virtual TaskStatus operator()() = 0;
  TaskID GetID() { return _myid; }
  TaskID GetDependency() { return _dep; }
  void SetComplete() { _complete = true; }
  bool IsComplete() { return _complete; }

 protected:
  TaskID _myid, _dep;
  bool lb_time, _complete = false;
};

class SimpleTask : public BaseTask {
 public:
  SimpleTask(TaskID id, SimpleTaskFunc func, TaskID dep)
      : BaseTask(id, dep), _func(func) {}
  TaskStatus operator()() { return _func(); }

 private:
  SimpleTaskFunc _func;
};

class BlockTask : public BaseTask {
 public:
  BlockTask(TaskID id, BlockTaskFunc func, TaskID dep, MeshBlock *pmb)
      : BaseTask(id, dep), _func(func), _pblock(pmb) {}
  TaskStatus operator()() { return _func(_pblock); }

 private:
  BlockTaskFunc _func;
  MeshBlock *_pblock;
};

class BlockStageTask : public BaseTask {
 public:
  BlockStageTask(TaskID id, BlockStageTaskFunc func, TaskID dep, MeshBlock *pmb,
                 int stage)
      : BaseTask(id, dep), _func(func), _pblock(pmb), _stage(stage) {}
  TaskStatus operator()() { return _func(_pblock, _stage); }

 private:
  BlockStageTaskFunc _func;
  MeshBlock *_pblock;
  int _stage;
};

class BlockStageNamesTask : public BaseTask {
 public:
  BlockStageNamesTask(TaskID id, BlockStageNamesTaskFunc func, TaskID dep, MeshBlock *pmb,
                      int stage, const std::vector<std::string> &sname)
      : BaseTask(id, dep), _func(func), _pblock(pmb), _stage(stage), _sname(sname) {}
  TaskStatus operator()() { return _func(_pblock, _stage, _sname); }

 private:
  BlockStageNamesTaskFunc _func;
  MeshBlock *_pblock;
  int _stage;
  std::vector<std::string> _sname;
};

class BlockStageNamesIntegratorTask : public BaseTask {
 public:
  BlockStageNamesIntegratorTask(TaskID id, BlockStageNamesIntegratorTaskFunc func,
                                TaskID dep, MeshBlock *pmb, int stage,
                                const std::vector<std::string> &sname, Integrator *integ)
      : BaseTask(id, dep), _func(func), _pblock(pmb), _stage(stage), _sname(sname),
        _int(integ) {}
  TaskStatus operator()() { return _func(_pblock, _stage, _sname, _int); }

 private:
  BlockStageNamesIntegratorTaskFunc _func;
  MeshBlock *_pblock;
  int _stage;
  std::vector<std::string> _sname;
  Integrator *_int;
};

class TaskList {
 public:
  bool IsComplete() { return _task_list.empty(); }
  int Size() { return _task_list.size(); }
  void Reset() {
    _tasks_added = 0;
    _task_list.clear();
    _dependencies.clear();
    _tasks_completed.clear();
  }
  bool IsReady() {
    for (auto &l : _dependencies) {
      if (!l->IsComplete()) {
        return false;
      }
    }
    return true;
  }
  void MarkTaskComplete(TaskID id) { _tasks_completed.SetFinished(id); }
  void ClearComplete() {
    auto task = _task_list.begin();
    while (task != _task_list.end()) {
      if ((*task)->IsComplete()) {
        task = _task_list.erase(task);
      } else {
        ++task;
      }
    }
  }
  TaskListStatus DoAvailable() {
    for (auto &task : _task_list) {
      auto dep = task->GetDependency();
      if (_tasks_completed.CheckDependencies(dep)) {
        /*std::cerr << "Task dependency met:" << std::endl
                  << dep.to_string() << std::endl
                  << _tasks_completed.to_string() << std::endl
                  << task->GetID().to_string() << std::endl << std::endl;*/
        TaskStatus status = (*task)();
        if (status == TaskStatus::complete) {
          task->SetComplete();
          MarkTaskComplete(task->GetID());
          /*std::cerr << "Task complete:" << std::endl
                    << task->GetID().to_string() << std::endl
                    << _tasks_completed.to_string() << std::endl << std::endl;*/
        }
      }
    }
    ClearComplete();
    if (IsComplete()) return TaskListStatus::complete;
    return TaskListStatus::running;
  }
  template <typename T, class... Args>
  TaskID AddTask(Args... args) {
    TaskID id(_tasks_added + 1);
    _task_list.push_back(std::make_unique<T>(id, std::forward<Args>(args)...));
    _tasks_added++;
    return id;
  }
  void Print() {
    int i = 0;
    std::cout << "TaskList::Print():" << std::endl;
    for (auto &t : _task_list) {
      std::cout << "  " << i << "  " << t->GetID().to_string() << "  "
                << t->GetDependency().to_string() << std::endl;
      i++;
    }
  }

 protected:
  std::list<std::unique_ptr<BaseTask>> _task_list;
  int _tasks_added = 0;
  std::vector<TaskList *> _dependencies;
  TaskID _tasks_completed;
};

} // namespace parthenon

#endif // TASK_LIST_TASKS_HPP_
