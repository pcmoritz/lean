/*
Author: Philipp C. Moritz
*/

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include <iostream>
#include <fstream>
#include <sstream>
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
#include "util/list.h"
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
  LeanContext(const std::string &leanpath) {
    unsigned trust_lvl = LEAN_BELIEVER_TRUST_LEVEL+1;
    environment env = mk_environment(trust_lvl);
    options opts;
    ios_ = io_state(opts, lean::mk_pretty_formatter_factory());
    tq_ = std::make_shared<st_task_queue>();
    set_task_queue(tq_.get());
    log_tree_ = std::make_shared<log_tree>();
    module_vfs_ = std::make_shared<fs_module_vfs>();
    mod_mgr_ = std::make_shared<module_mgr>(module_vfs_.get(), log_tree_->get_root(), env, ios_);
    auto mod_info = mod_mgr_->get_module(leanpath + "/standard.lean");

    // try {
    //   auto res = lean::get(mod_info->m_result);
    // } catch (...) {
    //   std::cout << "exception occured" << std::endl;
    // }
    mod_infos_.push_back(mod_info);
    env_ = get_combined_environment(mod_mgr_->get_initial_env(), mod_infos_);
    taskq().wait_for_finish(log_tree_->get_root().wait_for_finish());
  }
  declaration get(const name& name) {
    return env_.get(name);
  }
  void check(declaration const & d) {
    lean::check(env_, d);
  }

  lean::initializer init_; // TODO(pcm): Consider calling this when the module is imported
  lean::environment env_;
  std::shared_ptr<st_task_queue> tq_;
  std::shared_ptr<log_tree> log_tree_;
  std::shared_ptr<fs_module_vfs> module_vfs_;
  std::shared_ptr<module_mgr> mod_mgr_;
  io_state ios_;
  buffer<std::shared_ptr<module_info const>> mod_infos_;
};

std::shared_ptr<LeanContext> initialize_lean(const std::string& leanpath) {
  try {
    return std::make_shared<LeanContext>(leanpath);
  } catch (lean::throwable &ex) {
    std::cout << "exception has been thrown: " << ex.what() << std::endl;
  }
  return nullptr;
}

std::vector<name> get_module_declarations(std::shared_ptr<LeanContext> context) {
  std::vector<name> result;
  context->env_.for_each_declaration([&result](const declaration& d) {
      result.push_back(d.get_name());
    });
  return result;
}

PYBIND11_MODULE(leanpy, m) {
  m.doc() = "Python module for LEAN";

  py::class_<LeanContext, std::shared_ptr<LeanContext>>(m, "LeanContext")
    .def("get", &LeanContext::get)
    .def("check", &LeanContext::check);
  py::class_<expr>(m, "expr")
    .def("__repr__", [](const expr& e) {
      std::stringstream ss;
      ss << e;
      return ss.str();
    })
    .def("const", [](const name& name, const list<level>& level) {
      return new expr(mk_constant(name, level));
    });
  py::class_<declaration>(m, "declaration")
    .def("__repr__", [](const declaration& d) {
        std::ostringstream out;
        out << d.get_name() << ":" << d.get_value();
        return out.str();
      })
    .def_property_readonly("value", &declaration::get_value);
  py::class_<certified_declaration>(m, "certified_declaration");
  py::class_<environment>(m, "environment")
    .def("get", &environment::get);
  py::class_<name>(m, "name")
    .def("__repr__", [](const name& n) {
        return n.to_string();
      })
    .def(py::init())
    .def("__init__", [](name& instance, const std::string& identifier) {
      new (&instance) name(identifier.c_str());
    })
    .def("__init__", [](name &instance, const name& prefix, const std::string& identifier) {
      new (&instance) name(prefix, identifier.c_str());
    });

  py::class_<level>(m, "level")
    .def_property_readonly_static("zero", [](py::object) {
        return mk_level_zero();
      }, py::return_value_policy::reference)
    .def_property_readonly_static("one", [](py::object) {
        return mk_level_one();
      }, py::return_value_policy::reference);

  py::class_<list<level>>(m, "LevelList")
    .def("__init__", [](list<level>& instance, const py::list& l) {
      std::list<level> result;
      for (auto item : l) {
        result.push_back(item.cast<level>());
      }
      new (&instance) list<level>(to_list(result.begin(), result.end()));
    })
    .def("__len__", [](const list<level> &l) {
      return length(l);
    })
    .def("__repr__", [](const list<level>& l) {
        std::ostringstream out;
        out << l;
        return out.str();
      });

  m.def("initialize", &initialize_lean, "Initialize PyLean");
  m.def("get_module_declarations", &get_module_declarations);
  m.def("mk_app", [](expr const & f, expr const & a) {
      return mk_app(f, a, nulltag);
    });
}
