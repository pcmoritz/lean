/*
Author: Philipp C. Moritz
*/

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include <iostream>
#include <fstream>
#include <signal.h>
#include <cstdlib>
#include <getopt.h>
#include <string>
#include <utility>
#include <vector>
#include <util/timer.h>
#include "util/realpath.h"
#include "util/stackinfo.h"
#include "util/macros.h"
#include "util/debug.h"
#include "util/sstream.h"
#include "util/interrupt.h"
#include "util/init_module.h"
#include "util/memory.h"
#include "util/thread.h"
#include "util/lean_path.h"
#include "util/file_lock.h"
#include "util/sexpr/options.h"
#include "util/sexpr/option_declarations.h"
#include "kernel/environment.h"
#include "kernel/kernel_exception.h"
#include "kernel/type_checker.h"
#include "kernel/formatter.h"
#include "library/st_task_queue.h"
#include "library/mt_task_queue.h"
#include "library/module_mgr.h"
#include "kernel/standard_kernel.h"
#include "library/module.h"
#include "library/type_context.h"
#include "library/io_state_stream.h"
#include "library/export.h"
#include "library/message_builder.h"
#include "frontends/lean/parser.h"
#include "frontends/lean/pp.h"
#include "frontends/lean/dependencies.h"
#include "frontends/smt2/parser.h"
#include "frontends/lean/json.h"
#include "library/native_compiler/options.h"
#include "library/native_compiler/native_compiler.h"
#include "library/trace.h"
#include "init/init.h"
#include "shell/simple_pos_info_provider.h"
#include "shell/leandoc.h"

#include "util/extensible_object.h"

using namespace lean;

class LeanContext {
 public:
  LeanContext() {
    unsigned trust_lvl = LEAN_BELIEVER_TRUST_LEVEL+1;
    environment env = mk_environment(trust_lvl);
    options opts;
    io_state ios(opts, lean::mk_pretty_formatter_factory());
    tq_ = std::make_shared<st_task_queue>();
    set_task_queue(tq_.get());
    log_tree_ = std::make_shared<log_tree>();
    module_vfs_ = std::make_shared<fs_module_vfs>();
    mod_mgr_ = std::make_shared<module_mgr>(module_vfs_.get(), log_tree_->get_root(), env, ios);
    auto mod_info = mod_mgr_->get_module("/home/ubuntu/lean/library/standard.lean");

    // try {
    //   auto res = lean::get(mod_info->m_result);
    // } catch (...) {
    //   std::cout << "exception occured" << std::endl;
    // }
    mod_infos_.push_back(mod_info);
    env_ = get_combined_environment(mod_mgr_->get_initial_env(), mod_infos_);
  }
  declaration get(const name& name) {
    return env_.get(name);
  }

  lean::initializer init_;
  lean::environment env_;
  std::shared_ptr<st_task_queue> tq_;
  std::shared_ptr<log_tree> log_tree_;
  std::shared_ptr<fs_module_vfs> module_vfs_;
  std::shared_ptr<module_mgr> mod_mgr_;
  buffer<std::shared_ptr<module_info const>> mod_infos_;
};

LeanContext initialize_lean() {
  try {
    return LeanContext();
  } catch (lean::throwable &ex) {
    std::cout << "exception has been thrown: " << ex.what() << std::endl;
  }
}

std::vector<name> get_module_declarations(const LeanContext& context) {
  std::vector<name> result;
  context.env_.for_each_declaration([&result](const declaration& d) {
      result.push_back(d.get_name());
    });
  return result;
}

PYBIND11_MODULE(leanpy, m) {
  m.doc() = "Python module for LEAN";

  py::class_<LeanContext>(m, "LeanContext")
    .def("get", &LeanContext::get);
  py::class_<declaration>(m, "declaration");
  py::class_<environment>(m, "environment")
    .def("get", &environment::get);
  py::class_<name>(m, "name")
    .def("__repr__", [](const name& n) {
	return n.to_string();
      });

  m.def("initialize", &initialize_lean, "Initialize PyLean");
  m.def("get_module_declarations", &get_module_declarations);
}
