/*
Author: Philipp C. Moritz
*/

#include <pybind11/pybind11.h>

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

using namespace lean;

void initialize_lean() {
  try {
    lean::initializer init;
    unsigned trust_lvl = LEAN_BELIEVER_TRUST_LEVEL+1;
    environment env = mk_environment(trust_lvl);
    options opts;
    io_state ios(opts, lean::mk_pretty_formatter_factory());
    auto tq = std::make_shared<st_task_queue>();
    set_task_queue(tq.get());
    log_tree lt;
    fs_module_vfs vfs;
    module_mgr mod_mgr(&vfs, lt.get_root(), env, ios);
    auto mod_info = mod_mgr.get_module("foobar"); // This should raise an exception
    /*
    try {
      auto res = get(mod_info->m_result);
    } catch (...) {
      std::cout << "exception occured" << std::endl;
    }
    buffer<std::shared_ptr<module_info const>> mod_infos;
    mod_infos.push_back(mod_info);
    auto combined_env = get_combined_environment(mod_mgr.get_initial_env(), mod_infos);
    export_all_as_lowtext(std::cout, combined_env);
    */
  } catch (lean::throwable &ex) {
    std::cout << "exception has been thrown: " << ex.what() << std::endl;
  }
}

PYBIND11_PLUGIN(leanpy) {
    py::module m("leanpy", "Python module for LEAN");

    m.def("initialize", &initialize_lean, "Initialize PyLean");

    return m.ptr();
}
