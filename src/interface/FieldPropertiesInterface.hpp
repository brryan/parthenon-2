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
#ifndef PARTHENON_INTERFACE_FIELDPROPERTIESINTERFACE_HPP_
#define PARTHENON_INTERFACE_FIELDPROPERTIESINTERFACE_HPP_

#include <map>
#include <string>

#include "interface/StateDescriptor.hpp"

namespace parthenon {

class FieldPropertiesInterface {
public:
  virtual ~FieldPropertiesInterface() {}

  virtual StateDescriptor &State() = 0;

  static int GetIDFromLabel(std::string &label) {
    return FieldPropertiesInterface::_label_to_id[label];
  }

  static std::string GetLabelFromID(int id) {
    for (auto &x : FieldPropertiesInterface::_label_to_id) {
      if (x.second == id)
        return x.first;
    }
    return "UNKNOWN";
  }

  static void InsertID(const std::string &label, const int &id) {
    FieldPropertiesInterface::_label_to_id[label] = id;
  }

private:
  // _label_to_id is declared here and defined in
  // FieldPropertiesInterface.cpp
  static std::map<std::string, int> _label_to_id;
};

/*
template <typename T>
auto ConvertFieldPropertiesToInterface(
    const std::vector<std::shared_ptr<T>> &properties) {
  static_assert(std::is_base_of<FieldPropertiesInterface, T>::value,
                "Type given to ConvertFieldPropertiesToInterface is not "
                "derived from FieldPropertiesInterface");
  std::vector<std::shared_ptr<FieldPropertiesInterface>> res;
  for (auto p : properties)
    res.push_back(p);

  return res;
}
*/

} // namespace parthenon

#endif // PARTHENON_INTERFACE_FIELDPROPERTIESINTERFACE_HPP_
