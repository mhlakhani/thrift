/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <map>
#include <string>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <vector>
#include <list>
#include <cassert>
#include <unordered_map>

#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sstream>
#include "thrift/platform.h"
#include "thrift/version.h"

using std::map;
using std::ostream;
using std::ostringstream;
using std::string;
using std::stringstream;
using std::unordered_map;
using std::vector;

static const string episode_file_name = "thrift.js.episode";
// largest consecutive integer representable by a double (2 ^ 53 - 1)
static const int64_t max_safe_integer = 0x1fffffffffffff;
// smallest consecutive number representable by a double (-2 ^ 53 + 1)
static const int64_t min_safe_integer = -max_safe_integer;

#include "thrift/generate/t_oop_generator.h"


/**
 * JS code generator.
 */
class t_js_generator : public t_oop_generator {
public:
  t_js_generator(t_program* program,
                 const std::map<std::string, std::string>& parsed_options,
                 const std::string& option_string)
    : t_oop_generator(program) {
    (void)option_string;
    std::map<std::string, std::string>::const_iterator iter;

    gen_node_ = false;
    gen_jquery_ = false;
    gen_ts_ = false;
    gen_es6_ = false;
    gen_esm_ = false;
    gen_episode_file_ = false;

    bool with_ns_ = false;

    for (iter = parsed_options.begin(); iter != parsed_options.end(); ++iter) {
      if( iter->first.compare("node") == 0) {
        gen_node_ = true;
      } else if( iter->first.compare("jquery") == 0) {
        gen_jquery_ = true;
      } else if( iter->first.compare("ts") == 0) {
        gen_ts_ = true;
      } else if( iter->first.compare("with_ns") == 0) {
        with_ns_ = true;
      } else if( iter->first.compare("es6") == 0) {
        gen_es6_ = true;
      } else if ( iter->first.compare("esm") == 0) {
        gen_esm_ = true;
      } else if( iter->first.compare("imports") == 0) {
        parse_imports(program, iter->second);
      } else if (iter->first.compare("thrift_package_output_directory") == 0) {
        parse_thrift_package_output_directory(iter->second);
      } else {
        throw std::invalid_argument("unknown option js:" + iter->first);
      }
    }

    if (gen_es6_ && gen_jquery_) {
      throw std::invalid_argument("invalid switch: [-gen js:es6,jquery] options not compatible");
    }

    if (gen_node_ && gen_jquery_) {
      throw std::invalid_argument("invalid switch: [-gen js:node,jquery] options not compatible, try: [-gen js:node -gen "
            "js:jquery]");
    }

    if (!gen_node_ && with_ns_) {
      throw std::invalid_argument("invalid switch: [-gen js:with_ns] is only valid when using node.js");
    }

    if (!gen_node_ && gen_esm_) {
      throw std::invalid_argument("invalid switch: [-gen js:esm] is only valid when using node.js");
    }

    // Depending on the processing flags, we will update these to be ES6 compatible
    js_const_type_ = "var ";
    js_let_type_ = "var ";
    js_var_type_ = "var ";
    if (gen_es6_) {
      js_const_type_ = "const ";
      js_let_type_ = "let ";
    }

    if (gen_node_) {
      out_dir_base_ = "gen-nodejs";
      no_ns_ = !with_ns_;
    } else {
      out_dir_base_ = "gen-js";
      no_ns_ = false;
    }

    escape_['\''] = "\\'";
  }

  /**
   * Init and close methods
   */

  void init_generator() override;
  void close_generator() override;
  std::string display_name() const override;

  /**
   * Program-level generation functions
   */

  void generate_typedef(t_typedef* ttypedef) override;
  void generate_enum(t_enum* tenum) override;
  void generate_const(t_const* tconst) override;
  void generate_struct(t_struct* tstruct) override;
  void generate_xception(t_struct* txception) override;
  void generate_service(t_service* tservice) override;

  std::string render_recv_throw(std::string var);
  std::string render_recv_return(std::string var);

  std::string render_const_value(t_type* type, t_const_value* value);

  /**
   * Structs!
   */

  void generate_js_struct(t_struct* tstruct, bool is_exception);
  void generate_js_struct_definition(std::ostream& out,
                                     t_struct* tstruct,
                                     bool is_xception = false,
                                     bool is_exported = true);
  void generate_js_struct_reader(std::ostream& out, t_struct* tstruct);
  void generate_js_struct_writer(std::ostream& out, t_struct* tstruct);
  void generate_js_function_helpers(t_function* tfunction);

  /**
   * Service-level generation functions
   */

  void generate_service_helpers(t_service* tservice);
  void generate_service_interface(t_service* tservice);
  void generate_service_rest(t_service* tservice);
  void generate_service_client(t_service* tservice);
  void generate_service_processor(t_service* tservice);
  void generate_process_function(t_service* tservice, t_function* tfunction);

  /**
   * Serialization constructs
   */

  void generate_deserialize_field(std::ostream& out,
                                  t_field* tfield,
                                  std::string prefix = "",
                                  bool inclass = false);

  void generate_deserialize_struct(std::ostream& out, t_struct* tstruct, std::string prefix = "");

  void generate_deserialize_container(std::ostream& out, t_type* ttype, std::string prefix = "");

  void generate_deserialize_set_element(std::ostream& out, t_set* tset, std::string prefix = "");

  void generate_deserialize_map_element(std::ostream& out, t_map* tmap, std::string prefix = "");

  void generate_deserialize_list_element(std::ostream& out,
                                         t_list* tlist,
                                         std::string prefix = "");

  void generate_serialize_field(std::ostream& out, t_field* tfield, std::string prefix = "");

  void generate_serialize_struct(std::ostream& out, t_struct* tstruct, std::string prefix = "");

  void generate_serialize_container(std::ostream& out, t_type* ttype, std::string prefix = "");

  void generate_serialize_map_element(std::ostream& out,
                                      t_map* tmap,
                                      std::string kiter,
                                      std::string viter);

  void generate_serialize_set_element(std::ostream& out, t_set* tmap, std::string iter);

  void generate_serialize_list_element(std::ostream& out, t_list* tlist, std::string iter);

  /**
   * Helper rendering functions
   */

  std::string js_includes();
  std::string ts_includes();
  std::string ts_service_includes();
  std::string render_includes();
  std::string render_ts_includes();
  std::string get_import_path(t_program* program);
  std::string declare_field(t_field* tfield, bool init = false, bool obj = false);
  std::string function_signature(t_function* tfunction,
                                 std::string prefix = "",
                                 bool include_callback = false);
  std::string argument_list(t_struct* tstruct, bool include_callback = false);
  std::string type_to_enum(t_type* ttype);
  std::string make_valid_nodeJs_identifier(std::string const& name);
  std::string next_identifier_name(std::vector<t_field*> const& fields, std::string const& base_name);
  bool find_field(std::vector<t_field*> const& fields, std::string const& name);

  /**
   * Helper parser functions
   */

  void parse_imports(t_program* program, const std::string& imports_string);
  void parse_thrift_package_output_directory(const std::string& thrift_package_output_directory);

  std::string autogen_comment() override {
    return std::string("//\n") + "// Autogenerated by Thrift Compiler (" + THRIFT_VERSION + ")\n"
           + "//\n" + "// DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING\n"
           + "//\n";
  }

  t_type* get_contained_type(t_type* t);

  std::vector<std::string> js_namespace_pieces(t_program* p) {
    std::string ns = p->get_namespace("js");

    std::string::size_type loc;
    std::vector<std::string> pieces;

    if (no_ns_) {
      return pieces;
    }

    if (ns.size() > 0) {
      while ((loc = ns.find(".")) != std::string::npos) {
        pieces.push_back(ns.substr(0, loc));
        ns = ns.substr(loc + 1);
      }
    }

    if (ns.size() > 0) {
      pieces.push_back(ns);
    }

    return pieces;
  }

  std::string js_type_namespace(t_program* p) {
    if (gen_node_) {
      if (p != nullptr && p != program_) {
        return make_valid_nodeJs_identifier(p->get_name()) + "_ttypes.";
      }
      return "ttypes.";
    }
    return js_namespace(p);
  }

  bool has_js_namespace(t_program* p) {
    if (no_ns_) {
      return false;
    }
    std::string ns = p->get_namespace("js");
    return (ns.size() > 0);
  }

  std::string js_namespace(t_program* p) {
    if (no_ns_) {
      return "";
    }
    std::string ns = p->get_namespace("js");
    if (ns.size() > 0) {
      ns += ".";
    }

    return ns;
  }

  /**
   * TypeScript Definition File helper functions
   */

  string ts_function_signature(t_function* tfunction, bool include_callback);
  string ts_get_type(t_type* type);

  /**
   * Special indentation for TypeScript Definitions because of the module.
   * Returns the normal indentation + "  " if a module was defined.
   * @return string
   */
  string ts_indent() { return indent() + (!ts_module_.empty() ? "  " : ""); }

  /**
   * Returns "declare " if no module was defined.
   * @return string
   */
  string ts_declare() { return (ts_module_.empty() ? (gen_node_ ? "declare " : "export declare ") : ""); }

  /**
   * Returns "?" if the given field is optional or has a default value.
   * @param t_field The field to check
   * @return string
   */
  string ts_get_req(t_field* field) {return (field->get_req() == t_field::T_OPTIONAL || field->get_value() != nullptr ? "?" : ""); }

  /**
   * Returns the documentation, if the provided documentable object has one.
   * @param t_doc The object to get the documentation from
   * @return string The documentation
   */
  string ts_print_doc(t_doc* tdoc) {
    string result = "\n";

    if (tdoc->has_doc()) {
      std::stringstream doc(tdoc->get_doc());
      string item;

      result += ts_indent() + "/**" + "\n";
      while (std::getline(doc, item)) {
        result += ts_indent() + " * " + item + "\n";
      }
      result += ts_indent() + " */" + "\n";
    }
    return result;
  }

private:
  /**
   * True if we should generate NodeJS-friendly RPC services.
   */
  bool gen_node_;

  /**
   * True if we should generate services that use jQuery ajax (async/sync).
   */
  bool gen_jquery_;

  /**
   * True if we should generate a TypeScript Definition File for each service.
   */
  bool gen_ts_;

  /**
   * True if we should generate ES6 code, i.e. with Promises
   */
  bool gen_es6_;

  /**
   * True if we should generate ES modules, instead of CommonJS.
   */
  bool gen_esm_;

  /**
   * True if we will generate an episode file.
   */
  bool gen_episode_file_;

  /**
   * The name of the defined module(s), for TypeScript Definition Files.
   */
  string ts_module_;

  /**
   * True if we should not generate namespace objects for node.
   */
  bool no_ns_;

  /**
   * The node modules to use when importing the previously generated files.
   */
  vector<string> imports;

  /**
   * Cache for imported modules.
   */
  unordered_map<string, string> module_name_2_import_path;

  /**
   * Cache for TypeScript includes to generated import name.
   */
  unordered_map<t_program*, string> include_2_import_name;

  /**
   * The prefix to use when generating the episode file.
   */
  string thrift_package_output_directory_;

  /**
   * The variable decorator for "const" variables. Will default to "var" if in an incompatible language.
   */
  string js_const_type_;

   /**
   * The variable decorator for "let" variables. Will default to "var" if in an incompatible language.
   */
  string js_let_type_;

   /**
   * The default variable decorator. Supports all javascript languages, but is not scoped to functions or closures.
   */
  string js_var_type_;

  /**
   * File streams
   */
  ofstream_with_content_based_conditional_update f_episode_;
  ofstream_with_content_based_conditional_update f_types_;
  ofstream_with_content_based_conditional_update f_service_;
  ofstream_with_content_based_conditional_update f_types_ts_;
  ofstream_with_content_based_conditional_update f_service_ts_;
};

/**
 * Prepares for file generation by opening up the necessary file output
 * streams.
 *
 * @param tprogram The program to generate
 */
void t_js_generator::init_generator() {
  // Make output directory
  MKDIR(get_out_dir().c_str());

  const auto outdir = get_out_dir();

  // Make output file(s)
  if (gen_episode_file_) {
    const auto f_episode_file_path = outdir + episode_file_name;
    f_episode_.open(f_episode_file_path);
  }

  const auto f_types_name = outdir + program_->get_name() + "_types" + (gen_esm_ ? ".mjs" : ".js");
  f_types_.open(f_types_name.c_str());
  if (gen_episode_file_) {
    const auto types_module = program_->get_name() + "_types";
    f_episode_ << types_module << ":" << thrift_package_output_directory_ << "/" << types_module << '\n';
  }

  if (gen_ts_) {
    const auto f_types_ts_name = outdir + program_->get_name() + "_types.d.ts";
    f_types_ts_.open(f_types_ts_name.c_str());
  }

  // Print header
  f_types_ << autogen_comment();

  if ((gen_node_ || gen_es6_) && no_ns_) {
    f_types_ << "\"use strict\";" << '\n' << '\n';
  }

  f_types_ << js_includes() << '\n' << render_includes() << '\n';

  if (gen_ts_) {
    f_types_ts_ << autogen_comment() << ts_includes() << '\n' << render_ts_includes() << '\n';
  }

  if (gen_node_) {
    if (gen_esm_) {
      // Import the current module, so we can reference it as ttypes. This is
      // fine in ESM, because it allows circular imports.
      f_types_ << "import * as ttypes from './" + program_->get_name() + "_types.mjs';" << '\n';
    } else {
      f_types_ << js_const_type_ << "ttypes = module.exports = {};" << '\n';
    }
  }

  string pns;

  // setup the namespace
  // TODO should the namespace just be in the directory structure for node?
  vector<string> ns_pieces = js_namespace_pieces(program_);
  if (ns_pieces.size() > 0) {
    for (size_t i = 0; i < ns_pieces.size(); ++i) {
      pns += ((i == 0) ? "" : ".") + ns_pieces[i];
      f_types_ << "if (typeof " << pns << " === 'undefined') {" << '\n';
      f_types_ << "  " << pns << " = {};" << '\n';
      f_types_ << "}" << '\n';
      f_types_ << "" << "if (typeof module !== 'undefined' && module.exports) {" << '\n';
      f_types_ << "  module.exports." << pns << " = " << pns << ";" << '\n' << "}" << '\n';
    }
    if (gen_ts_) {
      ts_module_ = pns;
      f_types_ts_ << "declare module " << ts_module_ << " {";
    }
  }
}

/**
 * Prints standard js imports
 */
string t_js_generator::js_includes() {
  if (gen_node_) {
    string result;
    
    if (gen_esm_) {
      result += "import { Thrift } from 'thrift';\n";
    } else {
      result += js_const_type_ + "thrift = require('thrift');\n"
          + js_const_type_ + "Thrift = thrift.Thrift;\n";
    }
    if (!gen_es6_) {
      if (gen_esm_) {
        result += "import { Q } from 'thrift';\n";
      } else {
        result += js_const_type_ + "Q = thrift.Q;\n";
      }
    }
    if (gen_esm_) {
      result += "import Int64 from 'node-int64';";
    } else {
      result += js_const_type_ + "Int64 = require('node-int64');\n";
    }
    return result;
  }
  string result = "if (typeof Int64 === 'undefined' && typeof require === 'function') {\n  " + js_const_type_ + "Int64 = require('node-int64');\n}\n";
  return result;
}

/**
 * Prints standard ts imports
 */
string t_js_generator::ts_includes() {
  if (gen_node_) {
    return string(
        "import thrift = require('thrift');\n"
        "import Thrift = thrift.Thrift;\n"
        "import Q = thrift.Q;\n"
        "import Int64 = require('node-int64');");
  }
  return string("import Int64 = require('node-int64');");
}

/**
 * Prints service ts imports
 */
string t_js_generator::ts_service_includes() {
  if (gen_node_) {
    return string(
        "import thrift = require('thrift');\n"
        "import Thrift = thrift.Thrift;\n"
        "import Q = thrift.Q;\n"
        "import Int64 = require('node-int64');");
  }
  return string("import Int64 = require('node-int64');");
}

/**
 * Renders all the imports necessary for including another Thrift program
 */
string t_js_generator::render_includes() {
  string result = "";

  if (gen_node_) {
    const vector<t_program*>& includes = program_->get_includes();
    for (auto include : includes) {
      if (gen_esm_) {
        result += "import * as " + make_valid_nodeJs_identifier(include->get_name()) + "_ttypes from '" + get_import_path(include) + "';\n";
      } else {
        result += js_const_type_ + make_valid_nodeJs_identifier(include->get_name()) + "_ttypes = require('" + get_import_path(include) + "');\n";
      }
    }
    if (includes.size() > 0) {
      result += "\n";
    }
  }

  return result;
}

/**
 * Renders all the imports necessary for including another Thrift program
 */
string t_js_generator::render_ts_includes() {
  string result;

  if (!gen_node_) {
    return result;
  }
  const vector<t_program*>& includes = program_->get_includes();
  for (auto include : includes) {
    string include_name = make_valid_nodeJs_identifier(include->get_name()) + "_ttypes";
    include_2_import_name.insert({include, include_name});
    result += "import " + include_name + " = require('" + get_import_path(include) + "');\n";
  }
  if (includes.size() > 0) {
    result += "\n";
  }

  return result;
}

string t_js_generator::get_import_path(t_program* program) {
  const string import_file_name(program->get_name() + "_types");
  const string import_file_name_with_extension = import_file_name + (gen_esm_ ? ".mjs" : ".js");

  if (program->get_recursive()) {
    return "./" + import_file_name_with_extension;
  }

  auto module_name_and_import_path_iterator = module_name_2_import_path.find(import_file_name);
  if (module_name_and_import_path_iterator != module_name_2_import_path.end()) {
    return module_name_and_import_path_iterator->second;
  }
  return "./" + import_file_name_with_extension;
}

/**
 * Close up (or down) some filez.
 */
void t_js_generator::close_generator() {
  // Close types file(s)

  f_types_.close();

  if (gen_ts_) {
    if (!ts_module_.empty()) {
      f_types_ts_ << "}";
    }
    f_types_ts_.close();
  }
  if (gen_episode_file_){
    f_episode_.close();
  }
}

/**
 * Generates a typedef. This is not done in JS, types are all implicit.
 *
 * @param ttypedef The type definition
 */
void t_js_generator::generate_typedef(t_typedef* ttypedef) {
  (void)ttypedef;
}

/**
 * Generates code for an enumerated type. Since define is expensive to lookup
 * in JS, we use a global array for this.
 *
 * @param tenum The enumeration
 */
void t_js_generator::generate_enum(t_enum* tenum) {
  if (gen_esm_) {
    f_types_ << "export const " << tenum->get_name() << " = {" << '\n';
  } else {
    f_types_ << js_type_namespace(tenum->get_program()) << tenum->get_name() << " = {" << '\n';
  }

  if (gen_ts_) {
    f_types_ts_ << ts_print_doc(tenum) << ts_indent() << ts_declare() << "enum "
                << tenum->get_name() << " {" << '\n';
  }

  indent_up();

  vector<t_enum_value*> const& constants = tenum->get_constants();
  vector<t_enum_value*>::const_iterator c_iter;
  for (c_iter = constants.begin(); c_iter != constants.end(); ++c_iter) {
    int value = (*c_iter)->get_value();
    if (gen_ts_) {
      f_types_ts_ << ts_indent() << (*c_iter)->get_name() << " = " << value << "," << '\n';
      // add 'value: key' in addition to 'key: value' for TypeScript enums
      f_types_ << indent() << "'" << value << "' : '" << (*c_iter)->get_name() << "'," << '\n';
    }
    f_types_ << indent() << "'" << (*c_iter)->get_name() << "' : " << value;
    if (c_iter != constants.end() - 1) {
      f_types_ << ",";
    }
    f_types_ << '\n';
  }

  indent_down();

  f_types_ << "};" << '\n';

  if (gen_ts_) {
    f_types_ts_ << ts_indent() << "}" << '\n';
  }
}

/**
 * Generate a constant value
 */
void t_js_generator::generate_const(t_const* tconst) {
  t_type* type = tconst->get_type();
  string name = tconst->get_name();
  t_const_value* value = tconst->get_value();

  if (gen_esm_) {
    f_types_ << "export const " << name << " = ";
  } else {
    f_types_ << js_type_namespace(program_) << name << " = ";
  }
  f_types_ << render_const_value(type, value) << ";" << '\n';

  if (gen_ts_) {
    f_types_ts_ << ts_print_doc(tconst) << ts_indent() << ts_declare() << js_const_type_ << name << ": "
                << ts_get_type(type) << ";" << '\n';
  }
}

/**
 * Prints the value of a constant with the given type. Note that type checking
 * is NOT performed in this function as it is always run beforehand using the
 * validate_types method in main.cc
 */
string t_js_generator::render_const_value(t_type* type, t_const_value* value) {
  std::ostringstream out;

  type = get_true_type(type);

  if (type->is_base_type()) {
    t_base_type::t_base tbase = ((t_base_type*)type)->get_base();
    switch (tbase) {
    case t_base_type::TYPE_STRING:
      out << "'" << get_escaped_string(value) << "'";
      break;
    case t_base_type::TYPE_BOOL:
      out << (value->get_integer() > 0 ? "true" : "false");
      break;
    case t_base_type::TYPE_I8:
    case t_base_type::TYPE_I16:
    case t_base_type::TYPE_I32:
      out << value->get_integer();
      break;
    case t_base_type::TYPE_I64:
      {
        int64_t const& integer_value = value->get_integer();
        if (integer_value <= max_safe_integer && integer_value >= min_safe_integer) {
          out << "new Int64(" << integer_value << ")";
        } else {
          out << "new Int64('" << std::hex << integer_value << std::dec << "')";
        }
      }
      break;
    case t_base_type::TYPE_DOUBLE:
      if (value->get_type() == t_const_value::CV_INTEGER) {
        out << value->get_integer();
      } else {
        out << emit_double_as_string(value->get_double());
      }
      break;
    default:
      throw std::runtime_error("compiler error: no const of base type " + t_base_type::t_base_name(tbase));
    }
  } else if (type->is_enum()) {
    out << value->get_integer();
  } else if (type->is_struct() || type->is_xception()) {
    out << "new " << js_type_namespace(type->get_program()) << type->get_name() << "({";
    indent_up();
    const vector<t_field*>& fields = ((t_struct*)type)->get_members();
    vector<t_field*>::const_iterator f_iter;
    const map<t_const_value*, t_const_value*, t_const_value::value_compare>& val = value->get_map();
    map<t_const_value*, t_const_value*, t_const_value::value_compare>::const_iterator v_iter;
    for (v_iter = val.begin(); v_iter != val.end(); ++v_iter) {
      t_type* field_type = nullptr;
      for (f_iter = fields.begin(); f_iter != fields.end(); ++f_iter) {
        if ((*f_iter)->get_name() == v_iter->first->get_string()) {
          field_type = (*f_iter)->get_type();
        }
      }
      if (field_type == nullptr) {
        throw std::runtime_error("type error: " + type->get_name() + " has no field " + v_iter->first->get_string());
      }
      if (v_iter != val.begin())
        out << ",";
      out << '\n' << indent() << render_const_value(g_type_string, v_iter->first);
      out << " : ";
      out << render_const_value(field_type, v_iter->second);
    }
    indent_down();
    out << '\n' << indent() << "})";
  } else if (type->is_map()) {
    t_type* ktype = ((t_map*)type)->get_key_type();

    t_type* vtype = ((t_map*)type)->get_val_type();
    out << "{" << '\n';
    indent_up();

    const map<t_const_value*, t_const_value*, t_const_value::value_compare>& val = value->get_map();
    map<t_const_value*, t_const_value*, t_const_value::value_compare>::const_iterator v_iter;
    for (v_iter = val.begin(); v_iter != val.end(); ++v_iter) {
      if (v_iter != val.begin())
        out << "," << '\n';

      if (ktype->is_base_type() && ((t_base_type*)get_true_type(ktype))->get_base() == t_base_type::TYPE_I64){
        out << indent() << "\"" << v_iter->first->get_integer() << "\"";
      } else {
        out << indent() << render_const_value(ktype, v_iter->first);
      }

      out << " : ";
      out << render_const_value(vtype, v_iter->second);
    }
    indent_down();
    out << '\n' << indent() << "}";
  } else if (type->is_list() || type->is_set()) {
    t_type* etype;
    if (type->is_list()) {
      etype = ((t_list*)type)->get_elem_type();
    } else {
      etype = ((t_set*)type)->get_elem_type();
    }
    out << "[";
    const vector<t_const_value*>& val = value->get_list();
    vector<t_const_value*>::const_iterator v_iter;
    for (v_iter = val.begin(); v_iter != val.end(); ++v_iter) {
      if (v_iter != val.begin())
        out << ",";
      out << render_const_value(etype, *v_iter);
    }
    out << "]";
  }
  return out.str();
}

/**
 * Make a struct
 */
void t_js_generator::generate_struct(t_struct* tstruct) {
  generate_js_struct(tstruct, false);
}

/**
 * Generates a struct definition for a thrift exception. Basically the same
 * as a struct but extends the Exception class.
 *
 * @param txception The struct definition
 */
void t_js_generator::generate_xception(t_struct* txception) {
  generate_js_struct(txception, true);
}

/**
 * Structs can be normal or exceptions.
 */
void t_js_generator::generate_js_struct(t_struct* tstruct, bool is_exception) {
  generate_js_struct_definition(f_types_, tstruct, is_exception);
}

/**
 * Return type of contained elements for a container type. For maps
 * this is type of value (keys are always strings in js)
 */
t_type* t_js_generator::get_contained_type(t_type* t) {
  t_type* etype;
  if (t->is_list()) {
    etype = ((t_list*)t)->get_elem_type();
  } else if (t->is_set()) {
    etype = ((t_set*)t)->get_elem_type();
  } else {
    etype = ((t_map*)t)->get_val_type();
  }
  return etype;
}

/**
 * Generates a struct definition for a thrift data type. This is nothing in JS
 * where the objects are all just associative arrays (unless of course we
 * decide to start using objects for them...)
 *
 * @param tstruct The struct definition
 */
void t_js_generator::generate_js_struct_definition(ostream& out,
                                                   t_struct* tstruct,
                                                   bool is_exception,
                                                   bool is_exported) {
  const vector<t_field*>& members = tstruct->get_members();
  vector<t_field*>::const_iterator m_iter;

  if (gen_node_) {
    string commonjs_export = "";

    if (is_exported) {
      if (gen_esm_) {
        out << "export ";
      } else {
        commonjs_export = " = module.exports." + tstruct->get_name();
      }
    }

    string prefix = has_js_namespace(tstruct->get_program()) ? js_namespace(tstruct->get_program()) : js_const_type_;
    out << prefix << tstruct->get_name() << commonjs_export;
    if (gen_ts_) {
      f_types_ts_ << ts_print_doc(tstruct) << ts_indent() << ts_declare() << "class "
                  << tstruct->get_name() << (is_exception ? " extends Thrift.TException" : "")
                  << " {" << '\n';
    }
  } else {
    out << js_namespace(tstruct->get_program()) << tstruct->get_name();
    if (gen_ts_) {
      f_types_ts_ << ts_print_doc(tstruct) << ts_indent() << ts_declare() << "class "
                  << tstruct->get_name() << (is_exception ? " extends Thrift.TException" : "")
                  << " {" << '\n';
    }
  }

  if (gen_es6_) {
    if (gen_node_ && is_exception) {
      out << " = class extends Thrift.TException {" << '\n';
    } else {
      out << " = class {" << '\n';
    }
    indent_up();
    indent(out) << "constructor(args) {" << '\n';
  } else {
    out << " = function(args) {" << '\n';
  }

  indent_up();

  // Call super() method on inherited Error class
  if (gen_node_ && is_exception) {
    if (gen_es6_) {
      indent(out) << "super(args);" << '\n';
    } else {
      indent(out) << "Thrift.TException.call(this, \"" << js_namespace(tstruct->get_program())
        << tstruct->get_name() << "\");" << '\n';
    }
    out << indent() << "this.name = \"" << js_namespace(tstruct->get_program())
        << tstruct->get_name() << "\";" << '\n';
  }

  // members with arguments
  for (m_iter = members.begin(); m_iter != members.end(); ++m_iter) {
    string dval = declare_field(*m_iter, false, true);
    t_type* t = get_true_type((*m_iter)->get_type());
    if ((*m_iter)->get_value() != nullptr && !(t->is_struct() || t->is_xception())) {
      dval = render_const_value((*m_iter)->get_type(), (*m_iter)->get_value());
      out << indent() << "this." << (*m_iter)->get_name() << " = " << dval << ";" << '\n';
    } else {
      out << indent() << dval << ";" << '\n';
    }
    if (gen_ts_) {
      string ts_access = gen_node_ ? "public " : "";
      string member_name = (*m_iter)->get_name();

      // Special case. Exceptions derive from Error, and error has a non optional message field.
      // Ignore the optional flag in this case, otherwise we will generate a incompatible field
      // in the eyes of typescript.
      string optional_flag = is_exception && member_name == "message" ? "" : ts_get_req(*m_iter);

      f_types_ts_ << ts_indent() << ts_access << member_name << optional_flag << ": "
                  << ts_get_type((*m_iter)->get_type()) << ";" << '\n';
    }
  }

  // Generate constructor from array
  if (members.size() > 0) {

    for (m_iter = members.begin(); m_iter != members.end(); ++m_iter) {
      t_type* t = get_true_type((*m_iter)->get_type());
      if ((*m_iter)->get_value() != nullptr && (t->is_struct() || t->is_xception())) {
        indent(out) << "this." << (*m_iter)->get_name() << " = "
                    << render_const_value(t, (*m_iter)->get_value()) << ";" << '\n';
      }
    }

    // Early returns for exceptions
    for (m_iter = members.begin(); m_iter != members.end(); ++m_iter) {
      t_type* t = get_true_type((*m_iter)->get_type());
      if (t->is_xception()) {
        out << indent() << "if (args instanceof " << js_type_namespace(t->get_program())
            << t->get_name() << ") {" << '\n' << indent() << indent() << "this."
            << (*m_iter)->get_name() << " = args;" << '\n' << indent() << indent() << "return;"
            << '\n' << indent() << "}" << '\n';
      }
    }

    indent(out) << "if (args) {" << '\n';
    indent_up();
    if (gen_ts_) {
      f_types_ts_ << '\n' << ts_indent() << "constructor(args?: { ";
    }

    for (m_iter = members.begin(); m_iter != members.end(); ++m_iter) {
      t_type* t = get_true_type((*m_iter)->get_type());
      indent(out) << "if (args." << (*m_iter)->get_name() << " !== undefined && args." << (*m_iter)->get_name() << " !== null) {" << '\n';
      indent_up();
      indent(out) << "this." << (*m_iter)->get_name();

      if (t->is_struct()) {
        out << (" = new " + js_type_namespace(t->get_program()) + t->get_name() +
                "(args."+(*m_iter)->get_name() +");");
        out << '\n';
      } else if (t->is_container()) {
        t_type* etype = get_contained_type(t);
        string copyFunc = t->is_map() ? "Thrift.copyMap" : "Thrift.copyList";
        string type_list = "";

        while (etype->is_container()) {
          if (type_list.length() > 0) {
            type_list += ", ";
          }
          type_list += etype->is_map() ? "Thrift.copyMap" : "Thrift.copyList";
          etype = get_contained_type(etype);
        }

        if (etype->is_struct()) {
          if (type_list.length() > 0) {
            type_list += ", ";
          }
          type_list += js_type_namespace(etype->get_program()) + etype->get_name();
        }
        else {
          if (type_list.length() > 0) {
            type_list += ", ";
          }
          type_list += "null";
        }

        out << (" = " + copyFunc + "(args." + (*m_iter)->get_name() +
                ", [" + type_list + "]);");
        out << '\n';
      } else {
        out << " = args." << (*m_iter)->get_name() << ";" << '\n';
      }

      indent_down();
      if (!(*m_iter)->get_req()) {
        indent(out) << "} else {" << '\n';
         indent(out)
            << "  throw new Thrift.TProtocolException(Thrift.TProtocolExceptionType.UNKNOWN, "
               "'Required field " << (*m_iter)->get_name() << " is unset!');" << '\n';
      }
      indent(out) << "}" << '\n';
      if (gen_ts_) {
        f_types_ts_ << (*m_iter)->get_name() << ts_get_req(*m_iter) << ": "
                    << ts_get_type((*m_iter)->get_type()) << "; ";
      }
    }
    indent_down();
    out << indent() << "}" << '\n';
    if (gen_ts_) {
      f_types_ts_ << "});" << '\n';
    }
  }

  // Done with constructor
  indent_down();
  if (gen_es6_) {
    indent(out) << "}" << '\n' << '\n';
  } else {
    indent(out) << "};" << '\n';
  }

  if (gen_ts_) {
    f_types_ts_ << ts_indent() << "}" << '\n';
  }

  if (!gen_es6_) {
    if (is_exception) {
      out << "Thrift.inherits(" << js_namespace(tstruct->get_program()) << tstruct->get_name()
          << ", Thrift.TException);" << '\n';
      out << js_namespace(tstruct->get_program()) << tstruct->get_name() << ".prototype.name = '"
          << tstruct->get_name() << "';" << '\n';
    } else {
      // init prototype manually if we aren't using es6
      out << js_namespace(tstruct->get_program()) << tstruct->get_name() << ".prototype = {};"
          << '\n';
    }

  }

  generate_js_struct_reader(out, tstruct);
  generate_js_struct_writer(out, tstruct);

  // Close out the class definition
  if (gen_es6_) {
    indent_down();
    indent(out) << "};" << '\n';
  }
}

/**
 * Generates the read() method for a struct
 */
void t_js_generator::generate_js_struct_reader(ostream& out, t_struct* tstruct) {
  const vector<t_field*>& fields = tstruct->get_members();
  vector<t_field*>::const_iterator f_iter;

  if (gen_es6_) {
    indent(out) << "[Symbol.for(\"read\")] (input) {" << '\n';
  } else {
    indent(out) << js_namespace(tstruct->get_program()) << tstruct->get_name()
        << ".prototype[Symbol.for(\"read\")] = function(input) {" << '\n';
  }

  indent_up();

  indent(out) << "input.readStructBegin();" << '\n';

  // Loop over reading in fields
  indent(out) << "while (true) {" << '\n';

  indent_up();

  indent(out) << js_const_type_ << "ret = input.readFieldBegin();" << '\n';
  indent(out) << js_const_type_ << "ftype = ret.ftype;" << '\n';
  if (!fields.empty()) {
    indent(out) << js_const_type_ << "fid = ret.fid;" << '\n';
  }

  // Check for field STOP marker and break
  indent(out) << "if (ftype == Thrift.Type.STOP) {" << '\n';
  indent_up();
  indent(out) << "break;" << '\n';
  indent_down();
  indent(out) << "}" << '\n';
  if (!fields.empty()) {
    // Switch statement on the field we are reading
    indent(out) << "switch (fid) {" << '\n';

    indent_up();

    // Generate deserialization code for known cases
    for (f_iter = fields.begin(); f_iter != fields.end(); ++f_iter) {

      indent(out) << "case " << (*f_iter)->get_key() << ":" << '\n';
      indent(out) << "if (ftype == " << type_to_enum((*f_iter)->get_type()) << ") {" << '\n';

      indent_up();
      generate_deserialize_field(out, *f_iter, "this.");
      indent_down();

      indent(out) << "} else {" << '\n';

      indent(out) << "  input.skip(ftype);" << '\n';

      out << indent() << "}" << '\n' << indent() << "break;" << '\n';
    }
    if (fields.size() == 1) {
      // pseudo case to make jslint happy
      indent(out) << "case 0:" << '\n';
      indent(out) << "  input.skip(ftype);" << '\n';
      indent(out) << "  break;" << '\n';
    }
    // In the default case we skip the field
    indent(out) << "default:" << '\n';
    indent(out) << "  input.skip(ftype);" << '\n';

    scope_down(out);
  } else {
    indent(out) << "input.skip(ftype);" << '\n';
  }

  indent(out) << "input.readFieldEnd();" << '\n';

  scope_down(out);

  indent(out) << "input.readStructEnd();" << '\n';

  indent(out) << "return;" << '\n';

  indent_down();

  if (gen_es6_) {
    indent(out) << "}" << '\n' << '\n';
  } else {
    indent(out) << "};" << '\n' << '\n';
  }
}

/**
 * Generates the write() method for a struct
 */
void t_js_generator::generate_js_struct_writer(ostream& out, t_struct* tstruct) {
  string name = tstruct->get_name();
  const vector<t_field*>& fields = tstruct->get_members();
  vector<t_field*>::const_iterator f_iter;

  if (gen_es6_) {
    indent(out) << "[Symbol.for(\"write\")] (output) {" << '\n';
  } else {
    indent(out) << js_namespace(tstruct->get_program()) << tstruct->get_name()
        << ".prototype[Symbol.for(\"write\")] = function(output) {" << '\n';
  }

  indent_up();

  indent(out) << "output.writeStructBegin('" << name << "');" << '\n';

  for (f_iter = fields.begin(); f_iter != fields.end(); ++f_iter) {
    out << indent() << "if (this." << (*f_iter)->get_name() << " !== null && this."
        << (*f_iter)->get_name() << " !== undefined) {" << '\n';
    indent_up();

    indent(out) << "output.writeFieldBegin("
                << "'" << (*f_iter)->get_name() << "', " << type_to_enum((*f_iter)->get_type())
                << ", " << (*f_iter)->get_key() << ");" << '\n';

    // Write field contents
    generate_serialize_field(out, *f_iter, "this.");

    indent(out) << "output.writeFieldEnd();" << '\n';

    indent_down();
    indent(out) << "}" << '\n';
  }

  out << indent() << "output.writeFieldStop();" << '\n' << indent() << "output.writeStructEnd();"
      << '\n';

  out << indent() << "return;" << '\n';

  indent_down();
  if (gen_es6_) {
    out << indent() << "}" << '\n' << '\n';
  } else {
    out << indent() << "};" << '\n' << '\n';
  }
}

/**
 * Generates a thrift service.
 *
 * @param tservice The service definition
 */
void t_js_generator::generate_service(t_service* tservice) {
  string f_service_name = get_out_dir() + service_name_ + (gen_esm_ ? ".mjs" : ".js");
  f_service_.open(f_service_name.c_str());
  if (gen_episode_file_) {
    f_episode_ << service_name_ << ":" << thrift_package_output_directory_ << "/" << service_name_ << '\n';
  }

  if (gen_ts_) {
    string f_service_ts_name = get_out_dir() + service_name_ + ".d.ts";
    f_service_ts_.open(f_service_ts_name.c_str());
  }

  f_service_ << autogen_comment();

  if ((gen_node_ || gen_es6_) && no_ns_) {
    f_service_ << "\"use strict\";" << '\n' << '\n';
  }

  f_service_ << js_includes() << '\n' << render_includes() << '\n';

  if (gen_ts_) {
    if (tservice->get_extends() != nullptr) {
      f_service_ts_ << "/// <reference path=\"" << tservice->get_extends()->get_name()
                    << ".d.ts\" />" << '\n';
    }
    f_service_ts_ << autogen_comment() << '\n' << ts_includes() << '\n' << render_ts_includes() << '\n';
    if (gen_node_) {
      f_service_ts_ << "import ttypes = require('./" + program_->get_name() + "_types');" << '\n';
      // Generate type aliases
      // enum
      vector<t_enum*> const& enums = program_->get_enums();
      vector<t_enum*>::const_iterator e_iter;
      for (e_iter = enums.begin(); e_iter != enums.end(); ++e_iter) {
        f_service_ts_ << "import " << (*e_iter)->get_name() << " = ttypes."
                  << js_namespace(program_) << (*e_iter)->get_name() << '\n';
      }
      // const
      vector<t_const*> const& consts = program_->get_consts();
      vector<t_const*>::const_iterator c_iter;
      for (c_iter = consts.begin(); c_iter != consts.end(); ++c_iter) {
        f_service_ts_ << "import " << (*c_iter)->get_name() << " = ttypes."
                  << js_namespace(program_) << (*c_iter)->get_name() << '\n';
      }
      // exception
      vector<t_struct*> const& exceptions = program_->get_xceptions();
      vector<t_struct*>::const_iterator x_iter;
      for (x_iter = exceptions.begin(); x_iter != exceptions.end(); ++x_iter) {
        f_service_ts_ << "import " << (*x_iter)->get_name() << " = ttypes."
                  << js_namespace(program_) << (*x_iter)->get_name() << '\n';
      }
      // structs
      vector<t_struct*> const& structs = program_->get_structs();
      vector<t_struct*>::const_iterator s_iter;
      for (s_iter = structs.begin(); s_iter != structs.end(); ++s_iter) {
        f_service_ts_ << "import " << (*s_iter)->get_name() << " = ttypes."
                  << js_namespace(program_) << (*s_iter)->get_name() << '\n';
      }
    } else {
      f_service_ts_ << "import { " << program_->get_name() << " } from \"./" << program_->get_name() << "_types\";" << '\n' << '\n';
    }
    if (!ts_module_.empty()) {
      if (gen_node_) {
        f_service_ts_ << "declare module " << ts_module_ << " {";
      } else {
        f_service_ts_ << "declare module \"./" << program_->get_name() << "_types\" {" << '\n';
        indent_up();
        f_service_ts_ << ts_indent() << "module " << program_->get_name() << " {" << '\n';
        indent_up();
      }
    }
  }

  if (gen_node_) {
    if (tservice->get_extends() != nullptr) {
      f_service_ << js_const_type_ <<  tservice->get_extends()->get_name() << " = require('./"
                 << tservice->get_extends()->get_name() << "');" << '\n' << js_const_type_
                 << tservice->get_extends()->get_name()
                 << "Client = " << tservice->get_extends()->get_name() << ".Client;" << '\n'
                 << js_const_type_ << tservice->get_extends()->get_name()
                 << "Processor = " << tservice->get_extends()->get_name() << ".Processor;" << '\n';

      f_service_ts_ << "import " << tservice->get_extends()->get_name() << " = require('./"
                    << tservice->get_extends()->get_name() << "');" << '\n';
    }

    if (gen_esm_) {
      f_service_ << "import * as ttypes from './" + program_->get_name() + "_types.mjs';" << '\n';
    } else {
      f_service_ << js_const_type_ << "ttypes = require('./" + program_->get_name() + "_types');" << '\n';
    }
  }

  generate_service_helpers(tservice);
  generate_service_interface(tservice);
  generate_service_client(tservice);

  if (gen_node_) {
    generate_service_processor(tservice);
  }

  f_service_.close();
  if (gen_ts_) {
    if (!ts_module_.empty()) {
      if (gen_node_) {
        f_service_ts_ << "}" << '\n';
      } else {
        indent_down();
        f_service_ts_ << ts_indent() << "}" << '\n';
        f_service_ts_ << "}" << '\n';
      }
    }
    f_service_ts_.close();
  }
}

/**
 * Generates a service server definition.
 *
 * @param tservice The service to generate a server for.
 */
void t_js_generator::generate_service_processor(t_service* tservice) {
  vector<t_function*> functions = tservice->get_functions();
  vector<t_function*>::iterator f_iter;

  std::string service_var;
  if (!gen_node_ || has_js_namespace(tservice->get_program())) {
    service_var = js_namespace(tservice->get_program()) + service_name_ + "Processor";
    f_service_ << service_var;
  } else {
    service_var = service_name_ + "Processor";
    f_service_ << js_const_type_ << service_var;
  };
  if (gen_node_ && gen_ts_) {
    f_service_ts_ << '\n' << "declare class Processor ";
    if (tservice->get_extends() != nullptr) {
      f_service_ts_ << "extends " << tservice->get_extends()->get_name() << ".Processor ";
    }
    f_service_ts_ << "{" << '\n';
    indent_up();

    if(tservice->get_extends() == nullptr) {
      f_service_ts_ << ts_indent() << "private _handler: object;" << '\n' << '\n';
    }
    f_service_ts_ << ts_indent() << "constructor(handler: object);" << '\n';
    f_service_ts_ << ts_indent() << "process(input: thrift.TProtocol, output: thrift.TProtocol): void;" << '\n';
    indent_down();
  }

  bool is_subclass_service = tservice->get_extends() != nullptr;

  // ES6 Constructor
  if (gen_es6_) {
    if (is_subclass_service) {
      f_service_ << " = class " << service_name_ << "Processor extends " << tservice->get_extends()->get_name() << "Processor {" << '\n';
    } else {
      f_service_ << " = class " << service_name_ << "Processor {" << '\n';
    }
    indent_up();
    indent(f_service_) << "constructor(handler) {" << '\n';
  } else {
    f_service_ << " = function(handler) {" << '\n';
  }

  indent_up();
  if (gen_es6_ && is_subclass_service) {
    indent(f_service_) << "super(handler);" << '\n';
  }
  indent(f_service_) << "this._handler = handler;" << '\n';
  indent_down();

  // Done with constructor
  if (gen_es6_) {
    indent(f_service_) << "}" << '\n';
  } else {
    indent(f_service_) << "};" << '\n';
  }

  // ES5 service inheritance
  if (!gen_es6_ && is_subclass_service) {
    indent(f_service_) << "Thrift.inherits(" << js_namespace(tservice->get_program())
                       << service_name_ << "Processor, " << tservice->get_extends()->get_name()
                       << "Processor);" << '\n';
  }

  // Generate the server implementation
  if (gen_es6_) {
    indent(f_service_) << "process (input, output) {" << '\n';
  } else {
    indent(f_service_) << js_namespace(tservice->get_program()) << service_name_
                      << "Processor.prototype.process = function(input, output) {" << '\n';
  }

  indent_up();

  indent(f_service_) << js_const_type_ << "r = input.readMessageBegin();" << '\n' << indent()
             << "if (this['process_' + r.fname]) {" << '\n' << indent()
             << "  return this['process_' + r.fname].call(this, r.rseqid, input, output);" << '\n'
             << indent() << "} else {" << '\n' << indent() << "  input.skip(Thrift.Type.STRUCT);"
             << '\n' << indent() << "  input.readMessageEnd();" << '\n' << indent()
             << "  " << js_const_type_ << "x = new "
                "Thrift.TApplicationException(Thrift.TApplicationExceptionType.UNKNOWN_METHOD, "
                "'Unknown function ' + r.fname);" << '\n' << indent()
             << "  output.writeMessageBegin(r.fname, Thrift.MessageType.EXCEPTION, r.rseqid);"
             << '\n' << indent() << "  x[Symbol.for(\"write\")](output);" << '\n' << indent()
             << "  output.writeMessageEnd();" << '\n' << indent() << "  output.flush();" << '\n'
             << indent() << "}" << '\n';

  indent_down();
  if (gen_es6_) {
    indent(f_service_) << "}" << '\n';
  } else {
    indent(f_service_) << "};" << '\n';
  }

  // Generate the process subfunctions
  for (f_iter = functions.begin(); f_iter != functions.end(); ++f_iter) {
    generate_process_function(tservice, *f_iter);
  }

  // Close off the processor class definition
  if (gen_es6_) {
    indent_down();
    indent(f_service_) << "};" << '\n';
  }
  if (gen_node_ && gen_ts_) {
    f_service_ts_ << "}" << '\n';
  }

  if(gen_esm_) {
    f_service_ << "export { " << service_var << " as Processor };" << '\n';
  } else {
    f_service_ << "exports.Processor = " << service_var << ";" << '\n';
  }
}

/**
 * Generates a process function definition.
 *
 * @param tfunction The function to write a dispatcher for
 */
void t_js_generator::generate_process_function(t_service* tservice, t_function* tfunction) {
  if (gen_es6_) {
    indent(f_service_) << "process_" + tfunction->get_name() + " (seqid, input, output) {" << '\n';
  } else {
    indent(f_service_) << js_namespace(tservice->get_program()) << service_name_
                      << "Processor.prototype.process_" + tfunction->get_name()
                          + " = function(seqid, input, output) {" << '\n';
  }
  if (gen_ts_) {
    indent_up();
    f_service_ts_ << ts_indent() << "process_" << tfunction->get_name() << "(seqid: number, input: thrift.TProtocol, output: thrift.TProtocol): void;" << '\n';
    indent_down();
  }

  indent_up();

  string argsname = js_namespace(program_) + service_name_ + "_" + tfunction->get_name() + "_args";
  string resultname = js_namespace(program_) + service_name_ + "_" + tfunction->get_name()
                      + "_result";

  indent(f_service_) << js_const_type_ << "args = new " << argsname << "();" << '\n' << indent()
             << "args[Symbol.for(\"read\")](input);" << '\n' << indent() << "input.readMessageEnd();" << '\n';

  // Generate the function call
  t_struct* arg_struct = tfunction->get_arglist();
  const std::vector<t_field*>& fields = arg_struct->get_members();
  vector<t_field*>::const_iterator f_iter;

  // Shortcut out here for oneway functions
  if (tfunction->is_oneway()) {
    indent(f_service_) << "this._handler." << tfunction->get_name() << "(";

    bool first = true;
    for (f_iter = fields.begin(); f_iter != fields.end(); ++f_iter) {
      if (first) {
        first = false;
      } else {
        f_service_ << ", ";
      }
      f_service_ << "args." << (*f_iter)->get_name();
    }

    f_service_ << ");" << '\n';
    indent_down();

    if (gen_es6_) {
      indent(f_service_) << "}" << '\n';
    } else {
      indent(f_service_) << "};" << '\n';
    }
    return;
  }

  // Promise style invocation
  indent(f_service_) << "if (this._handler." << tfunction->get_name()
             << ".length === " << fields.size() << ") {" << '\n';
  indent_up();

  if (gen_es6_) {
    indent(f_service_) << "new Promise((resolve) => resolve(this._handler." << tfunction->get_name() << ".bind(this._handler)(" << '\n';
  } else {
    string maybeComma = (fields.size() > 0 ? "," : "");
    indent(f_service_) << "Q.fcall(this._handler." << tfunction->get_name() << ".bind(this._handler)"
                       << maybeComma << '\n';
  }

  indent_up();
  for (f_iter = fields.begin(); f_iter != fields.end(); ++f_iter) {
    string maybeComma = (f_iter != fields.end() - 1 ? "," : "");
    indent(f_service_) << "args." << (*f_iter)->get_name() << maybeComma << '\n';
  }
  indent_down();

  if (gen_es6_) {
    indent(f_service_) << "))).then(result => {" << '\n';
  } else {
    indent(f_service_) << ").then(function(result) {" << '\n';
  }

  indent_up();
  f_service_ << indent() << js_const_type_ << "result_obj = new " << resultname << "({success: result});" << '\n'
             << indent() << "output.writeMessageBegin(\"" << tfunction->get_name()
             << "\", Thrift.MessageType.REPLY, seqid);" << '\n' << indent()
             << "result_obj[Symbol.for(\"write\")](output);" << '\n' << indent() << "output.writeMessageEnd();" << '\n'
             << indent() << "output.flush();" << '\n';
  indent_down();

  if (gen_es6_) {
    indent(f_service_) << "}).catch(err => {" << '\n';
  } else {
    indent(f_service_) << "}).catch(function (err) {" << '\n';
  }
  indent_up();
  indent(f_service_) << js_let_type_ << "result;" << '\n';

  bool has_exception = false;
  t_struct* exceptions = tfunction->get_xceptions();
  if (exceptions) {
    const vector<t_field*>& members = exceptions->get_members();
    for (auto member : members) {
      t_type* t = get_true_type(member->get_type());
      if (t->is_xception()) {
        if (!has_exception) {
          has_exception = true;
          indent(f_service_) << "if (err instanceof " << js_type_namespace(t->get_program())
                             << t->get_name();
        } else {
          f_service_ << " || err instanceof " << js_type_namespace(t->get_program())
                     << t->get_name();
        }
      }
    }
  }

  if (has_exception) {
    f_service_ << ") {" << '\n';
    indent_up();
    f_service_ << indent() << "result = new " << resultname << "(err);" << '\n' << indent()
               << "output.writeMessageBegin(\"" << tfunction->get_name()
               << "\", Thrift.MessageType.REPLY, seqid);" << '\n';

    indent_down();
    indent(f_service_) << "} else {" << '\n';
    indent_up();
  }

  f_service_ << indent() << "result = new "
                            "Thrift.TApplicationException(Thrift.TApplicationExceptionType.UNKNOWN,"
                            " err.message);" << '\n' << indent() << "output.writeMessageBegin(\""
             << tfunction->get_name() << "\", Thrift.MessageType.EXCEPTION, seqid);" << '\n';

  if (has_exception) {
    indent_down();
    indent(f_service_) << "}" << '\n';
  }

  f_service_ << indent() << "result[Symbol.for(\"write\")](output);" << '\n' << indent()
             << "output.writeMessageEnd();" << '\n' << indent() << "output.flush();" << '\n';
  indent_down();
  indent(f_service_) << "});" << '\n';
  indent_down();
  // End promise style invocation

  // Callback style invocation
  indent(f_service_) << "} else {" << '\n';
  indent_up();
  indent(f_service_) << "this._handler." << tfunction->get_name() << "(";

  for (f_iter = fields.begin(); f_iter != fields.end(); ++f_iter) {
    f_service_ << "args." << (*f_iter)->get_name() << ", ";
  }

  if (gen_es6_) {
    f_service_ << "(err, result) => {" << '\n';
  } else {
    f_service_ << "function (err, result) {" << '\n';
  }
  indent_up();
  indent(f_service_) << js_let_type_ << "result_obj;" << '\n';

  indent(f_service_) << "if ((err === null || typeof err === 'undefined')";
  if (has_exception) {
    const vector<t_field*>& members = exceptions->get_members();
    for (auto member : members) {
      t_type* t = get_true_type(member->get_type());
      if (t->is_xception()) {
        f_service_ << " || err instanceof " << js_type_namespace(t->get_program()) << t->get_name();
      }
    }
  }
  f_service_ << ") {" << '\n';
  indent_up();
  f_service_ << indent() << "result_obj = new " << resultname
             << "((err !== null || typeof err === 'undefined') ? err : {success: result});" << '\n' << indent()
             << "output.writeMessageBegin(\"" << tfunction->get_name()
             << "\", Thrift.MessageType.REPLY, seqid);" << '\n';
  indent_down();
  indent(f_service_) << "} else {" << '\n';
  indent_up();
  f_service_ << indent() << "result_obj = new "
                            "Thrift.TApplicationException(Thrift.TApplicationExceptionType.UNKNOWN,"
                            " err.message);" << '\n' << indent() << "output.writeMessageBegin(\""
             << tfunction->get_name() << "\", Thrift.MessageType.EXCEPTION, seqid);" << '\n';
  indent_down();
  f_service_ << indent() << "}" << '\n' << indent() << "result_obj[Symbol.for(\"write\")](output);" << '\n' << indent()
             << "output.writeMessageEnd();" << '\n' << indent() << "output.flush();" << '\n';

  indent_down();
  indent(f_service_) << "});" << '\n';
  indent_down();
  indent(f_service_) << "}" << '\n';
  // End callback style invocation

  indent_down();

  if (gen_es6_) {
    indent(f_service_) << "}" << '\n';
  } else {
    indent(f_service_) << "};" << '\n';
  }
}

/**
 * Generates helper functions for a service.
 *
 * @param tservice The service to generate a header definition for
 */
void t_js_generator::generate_service_helpers(t_service* tservice) {
  // Do not generate TS definitions for helper functions
  bool gen_ts_tmp = gen_ts_;
  gen_ts_ = false;

  vector<t_function*> functions = tservice->get_functions();
  vector<t_function*>::iterator f_iter;

  f_service_ << "//HELPER FUNCTIONS AND STRUCTURES" << '\n' << '\n';

  for (f_iter = functions.begin(); f_iter != functions.end(); ++f_iter) {
    t_struct* ts = (*f_iter)->get_arglist();
    string name = ts->get_name();
    ts->set_name(service_name_ + "_" + name);
    generate_js_struct_definition(f_service_, ts, false, false);
    generate_js_function_helpers(*f_iter);
    ts->set_name(name);
  }

  gen_ts_ = gen_ts_tmp;
}

/**
 * Generates a struct and helpers for a function.
 *
 * @param tfunction The function
 */
void t_js_generator::generate_js_function_helpers(t_function* tfunction) {
  t_struct result(program_, service_name_ + "_" + tfunction->get_name() + "_result");
  t_field success(tfunction->get_returntype(), "success", 0);
  if (!tfunction->get_returntype()->is_void()) {
    result.append(&success);
  }

  t_struct* xs = tfunction->get_xceptions();
  const vector<t_field*>& fields = xs->get_members();
  vector<t_field*>::const_iterator f_iter;
  for (f_iter = fields.begin(); f_iter != fields.end(); ++f_iter) {
    result.append(*f_iter);
  }

  generate_js_struct_definition(f_service_, &result, false, false);
}

/**
 * Generates a service interface definition.
 *
 * @param tservice The service to generate a header definition for
 */
void t_js_generator::generate_service_interface(t_service* tservice) {
  (void)tservice;
}

/**
 * Generates a REST interface
 */
void t_js_generator::generate_service_rest(t_service* tservice) {
  (void)tservice;
}

/**
 * Generates a service client definition.
 *
 * @param tservice The service to generate a server for.
 */
void t_js_generator::generate_service_client(t_service* tservice) {

  bool is_subclass_service = tservice->get_extends() != nullptr;

  string client_var = js_namespace(tservice->get_program()) + service_name_ + "Client";
  if (gen_node_) {
    string prefix = has_js_namespace(tservice->get_program()) ? "" : js_const_type_;
    f_service_ << prefix << client_var;
    if (gen_ts_) {
      f_service_ts_ << ts_print_doc(tservice) << ts_indent() << ts_declare() << "class "
                    << "Client ";
      if (tservice->get_extends() != nullptr) {
        f_service_ts_ << "extends " << tservice->get_extends()->get_name() << ".Client ";
      }
      f_service_ts_ << "{" << '\n';
    }
  } else {
    f_service_ << client_var;
    if (gen_ts_) {
      f_service_ts_ << ts_print_doc(tservice) << ts_indent() << ts_declare() << "class "
                    << service_name_ << "Client ";
      if (is_subclass_service) {
        f_service_ts_ << "extends " << tservice->get_extends()->get_name() << "Client ";
      }
      f_service_ts_ << "{" << '\n';
    }
  }

  // ES6 Constructor
  if (gen_es6_) {

    if (is_subclass_service) {
      f_service_ << " = class " << service_name_ << "Client extends " << js_namespace(tservice->get_extends()->get_program())
                       << tservice->get_extends()->get_name() << "Client {" << '\n';
    } else {
      f_service_ << " = class " << service_name_ << "Client {" << '\n';
    }
    indent_up();
    if (gen_node_) {
      indent(f_service_) << "constructor(output, pClass) {" << '\n';
    } else {
      indent(f_service_) << "constructor(input, output) {" << '\n';
    }
  } else {
    if (gen_node_) {
      f_service_ << " = function(output, pClass) {" << '\n';
    } else {
      f_service_ << " = function(input, output) {" << '\n';
    }
  }

  indent_up();

  if (gen_node_) {
    if (gen_es6_ && is_subclass_service) {
      indent(f_service_) << "super(output, pClass);" << '\n';
    }
    indent(f_service_) << "this.output = output;" << '\n';
    indent(f_service_) << "this.pClass = pClass;" << '\n';
    indent(f_service_) << "this._seqid = 0;" << '\n';
    indent(f_service_) << "this._reqs = {};" << '\n';
    if (gen_ts_) {
      if(!is_subclass_service) {
        f_service_ts_ << ts_indent() << "private output: thrift.TTransport;" << '\n'
                      << ts_indent() << "private pClass: thrift.TProtocol;" << '\n'
                      << ts_indent() << "private _seqid: number;" << '\n'
                      << '\n';
      }

      f_service_ts_ << ts_indent() << "constructor(output: thrift.TTransport, pClass: { new(trans: thrift.TTransport): thrift.TProtocol });"
                    << '\n';
    }
  } else {
    indent(f_service_) << "this.input = input;" << '\n';
    indent(f_service_) << "this.output = (!output) ? input : output;" << '\n';
    indent(f_service_) << "this.seqid = 0;" << '\n';
    if (gen_ts_) {
      f_service_ts_ << ts_indent() << "input: Thrift.TJSONProtocol;" << '\n' << ts_indent()
                    << "output: Thrift.TJSONProtocol;" << '\n' << ts_indent() << "seqid: number;"
                    << '\n' << '\n' << ts_indent()
                    << "constructor(input: Thrift.TJSONProtocol, output?: Thrift.TJSONProtocol);"
                    << '\n';
    }
  }

  indent_down();

  if (gen_es6_) {
    indent(f_service_) << "}" << '\n';
  } else {
    indent(f_service_) << "};" << '\n';
    if (is_subclass_service) {
      indent(f_service_) << "Thrift.inherits(" << js_namespace(tservice->get_program())
                        << service_name_ << "Client, "
                        << js_namespace(tservice->get_extends()->get_program())
                        << tservice->get_extends()->get_name() << "Client);" << '\n';
    } else {
      // init prototype
      indent(f_service_) << js_namespace(tservice->get_program()) << service_name_
                        << "Client.prototype = {};" << '\n';
    }
  }

  // utils for multiplexed services
  if (gen_node_) {
    if (gen_es6_) {
      indent(f_service_) << "seqid () { return this._seqid; }" << '\n';
      indent(f_service_) << "new_seqid () { return this._seqid += 1; }" << '\n';
    } else {
      indent(f_service_) << js_namespace(tservice->get_program()) << service_name_
                        << "Client.prototype.seqid = function() { return this._seqid; };" << '\n'
                        << js_namespace(tservice->get_program()) << service_name_
                        << "Client.prototype.new_seqid = function() { return this._seqid += 1; };"
                        << '\n';
    }
  }

  // Generate client method implementations
  vector<t_function*> functions = tservice->get_functions();
  vector<t_function*>::const_iterator f_iter;
  for (f_iter = functions.begin(); f_iter != functions.end(); ++f_iter) {
    t_struct* arg_struct = (*f_iter)->get_arglist();
    const vector<t_field*>& fields = arg_struct->get_members();
    vector<t_field*>::const_iterator fld_iter;
    string funname = (*f_iter)->get_name();
    string arglist = argument_list(arg_struct);

    // Open function
    f_service_ << '\n';
    if (gen_es6_) {
      indent(f_service_) << funname << " (" << arglist << ") {" << '\n';
    } else {
      indent(f_service_) << js_namespace(tservice->get_program()) << service_name_ << "Client.prototype."
                << function_signature(*f_iter, "", !gen_es6_) << " {" << '\n';
    }

    indent_up();

    if (gen_ts_) {
      // function definition without callback
      f_service_ts_ << ts_print_doc(*f_iter) << ts_indent() << ts_function_signature(*f_iter, false) << '\n';
      if (!gen_es6_) {
        // overload with callback
        f_service_ts_ << ts_print_doc(*f_iter) << ts_indent() << ts_function_signature(*f_iter, true) << '\n';
      } else {
        // overload with callback
        f_service_ts_ << ts_print_doc(*f_iter) << ts_indent() << ts_function_signature(*f_iter, true) << '\n';
      }
    }

    if (gen_es6_ && gen_node_) {
      indent(f_service_) << "this._seqid = this.new_seqid();" << '\n';
      indent(f_service_) << js_const_type_ << "self = this;" << '\n' << indent()
                 << "return new Promise((resolve, reject) => {" << '\n';
      indent_up();
      indent(f_service_) << "self._reqs[self.seqid()] = (error, result) => {" << '\n';
      indent_up();
      indent(f_service_) << "return error ? reject(error) : resolve(result);" << '\n';
      indent_down();
      indent(f_service_) << "};" << '\n';
      indent(f_service_) << "self.send_" << funname << "(" << arglist << ");" << '\n';
      indent_down();
      indent(f_service_) << "});" << '\n';
    } else if (gen_node_) { // Node.js output      ./gen-nodejs
      f_service_ << indent() << "this._seqid = this.new_seqid();" << '\n' << indent()
                 << "if (callback === undefined) {" << '\n';
      indent_up();
      f_service_ << indent() << js_const_type_ << "_defer = Q.defer();" << '\n' << indent()
                 << "this._reqs[this.seqid()] = function(error, result) {" << '\n';
      indent_up();
      indent(f_service_) << "if (error) {" << '\n';
      indent_up();
      indent(f_service_) << "_defer.reject(error);" << '\n';
      indent_down();
      indent(f_service_) << "} else {" << '\n';
      indent_up();
      indent(f_service_) << "_defer.resolve(result);" << '\n';
      indent_down();
      indent(f_service_) << "}" << '\n';
      indent_down();
      indent(f_service_) << "};" << '\n';
      f_service_ << indent() << "this.send_" << funname << "(" << arglist << ");" << '\n'
                 << indent() << "return _defer.promise;" << '\n';
      indent_down();
      indent(f_service_) << "} else {" << '\n';
      indent_up();
      f_service_ << indent() << "this._reqs[this.seqid()] = callback;" << '\n' << indent()
                 << "this.send_" << funname << "(" << arglist << ");" << '\n';
      indent_down();
      indent(f_service_) << "}" << '\n';
    } else if (gen_es6_) {
      f_service_ << indent() << js_const_type_ << "self = this;" << '\n' << indent()
                 << "return new Promise((resolve, reject) => {" << '\n';
      indent_up();
      f_service_ << indent() << "self.send_" << funname << "(" << arglist
                 << (arglist.empty() ? "" : ", ") << "(error, result) => {" << '\n';
      indent_up();
      indent(f_service_) << "return error ? reject(error) : resolve(result);" << '\n';
      indent_down();
      f_service_ << indent() << "});" << '\n';
      indent_down();
      f_service_ << indent() << "});" << '\n';

    } else if (gen_jquery_) { // jQuery output       ./gen-js
      f_service_ << indent() << "if (callback === undefined) {" << '\n';
      indent_up();
      f_service_ << indent() << "this.send_" << funname << "(" << arglist << ");" << '\n';
      if (!(*f_iter)->is_oneway()) {
        f_service_ << indent();
        if (!(*f_iter)->get_returntype()->is_void()) {
          f_service_ << "return ";
        }
        f_service_ << "this.recv_" << funname << "();" << '\n';
      }
      indent_down();
      f_service_ << indent() << "} else {" << '\n';
      indent_up();
      f_service_ << indent() << js_const_type_ << "postData = this.send_" << funname << "(" << arglist
                 << (arglist.empty() ? "" : ", ") << "true);" << '\n';
      f_service_ << indent() << "return this.output.getTransport()" << '\n';
      indent_up();
      f_service_ << indent() << ".jqRequest(this, postData, arguments, this.recv_" << funname
                 << ");" << '\n';
      indent_down();
      indent_down();
      f_service_ << indent() << "}" << '\n';
    } else { // Standard JavaScript ./gen-js
      f_service_ << indent() << "this.send_" << funname << "(" << arglist
                 << (arglist.empty() ? "" : ", ") << "callback); " << '\n';
      if (!(*f_iter)->is_oneway()) {
        f_service_ << indent() << "if (!callback) {" << '\n';
        f_service_ << indent();
        if (!(*f_iter)->get_returntype()->is_void()) {
          f_service_ << "  return ";
        }
        f_service_ << "this.recv_" << funname << "();" << '\n';
        f_service_ << indent() << "}" << '\n';
      }
    }

    indent_down();

    if (gen_es6_) {
      indent(f_service_) << "}" << '\n' << '\n';
    } else {
      indent(f_service_) << "};" << '\n' << '\n';
    }

    // Send function
    if (gen_es6_) {
      if (gen_node_) {
        indent(f_service_) << "send_" << funname << " (" << arglist << ") {" << '\n';
      } else {
        // ES6 js still uses callbacks here. Should refactor this to promise style later..
        indent(f_service_) << "send_" << funname << " (" << argument_list(arg_struct, true) << ") {" << '\n';
      }
    } else {
      indent(f_service_) << js_namespace(tservice->get_program()) << service_name_ << "Client.prototype.send_"
                << function_signature(*f_iter, "", !gen_node_) << " {" << '\n';
    }

    indent_up();

    std::string outputVar;
    if (gen_node_) {
      f_service_ << indent() << js_const_type_ << "output = new this.pClass(this.output);" << '\n';
      outputVar = "output";
    } else {
      outputVar = "this.output";
    }

    std::string argsname = js_namespace(program_) + service_name_ + "_" + (*f_iter)->get_name()
                           + "_args";

    std::string messageType = (*f_iter)->is_oneway() ? "Thrift.MessageType.ONEWAY"
                                                     : "Thrift.MessageType.CALL";
    // Build args
    if (fields.size() > 0){
      // It is possible that a method argument is named "params", we need to ensure the locally
      // generated identifier "params" is uniquely named
      std::string params_identifier = this->next_identifier_name(fields, "params");
      f_service_ << indent() << js_const_type_ << params_identifier << " = {" << '\n';
      indent_up();
      for (fld_iter = fields.begin(); fld_iter != fields.end(); ++fld_iter) {
        indent(f_service_) << (*fld_iter)->get_name() << ": " << (*fld_iter)->get_name();
        if (fld_iter != fields.end()-1) {
          f_service_ << "," << '\n';
        } else {
          f_service_ << '\n';
        }
      }
      indent_down();
      indent(f_service_) << "};" << '\n';

      // NOTE: "args" is a reserved keyword, so no need to generate a unique identifier
      indent(f_service_) << js_const_type_ << "args = new " << argsname << "(" << params_identifier << ");" << '\n';
    } else {
      indent(f_service_) << js_const_type_ << "args = new " << argsname << "();" << '\n';
    }


    // Serialize the request header within try/catch
    indent(f_service_) << "try {" << '\n';
    indent_up();

    if (gen_node_) {
      f_service_ << indent() << outputVar << ".writeMessageBegin('" << (*f_iter)->get_name()
                 << "', " << messageType << ", this.seqid());" << '\n';
    } else {
      f_service_ << indent() << outputVar << ".writeMessageBegin('" << (*f_iter)->get_name()
                 << "', " << messageType << ", this.seqid);" << '\n';
    }


    // Write to the stream
    f_service_ << indent() << "args[Symbol.for(\"write\")](" << outputVar << ");" << '\n' << indent() << outputVar
               << ".writeMessageEnd();" << '\n';

    if (gen_node_) {
      if((*f_iter)->is_oneway()) {
        f_service_ << indent() << "this.output.flush();" << '\n';
        f_service_ << indent() << js_const_type_ << "callback = this._reqs[this.seqid()] || function() {};" << '\n';
        f_service_ << indent() << "delete this._reqs[this.seqid()];" << '\n';
        f_service_ << indent() << "callback(null);" << '\n';
      } else {
        f_service_ << indent() << "return this.output.flush();" << '\n';
      }
    } else {
      if (gen_jquery_) {
        f_service_ << indent() << "return this.output.getTransport().flush(callback);" << '\n';
      } else if (gen_es6_) {
        f_service_ << indent() << js_const_type_ << "self = this;" << '\n';
        if((*f_iter)->is_oneway()) {
          f_service_ << indent() << "this.output.getTransport().flush(true, null);" << '\n';
          f_service_ << indent() << "callback();" << '\n';
        } else {
          f_service_ << indent() << "this.output.getTransport().flush(true, () => {" << '\n';
          indent_up();
          f_service_ << indent() << js_let_type_ << "error = null, result = null;" << '\n';
          f_service_ << indent() << "try {" << '\n';
          f_service_ << indent() << "  result = self.recv_" << funname << "();" << '\n';
          f_service_ << indent() << "} catch (e) {" << '\n';
          f_service_ << indent() << "  error = e;" << '\n';
          f_service_ << indent() << "}" << '\n';
          f_service_ << indent() << "callback(error, result);" << '\n';
          indent_down();
          f_service_ << indent() << "});" << '\n';
        }
      } else {
        f_service_ << indent() << "if (callback) {" << '\n';
        indent_up();
        if((*f_iter)->is_oneway()) {
          f_service_ << indent() << "this.output.getTransport().flush(true, null);" << '\n';
          f_service_ << indent() << "callback();" << '\n';
        } else {
          f_service_ << indent() << js_const_type_ << "self = this;" << '\n';
          f_service_ << indent() << "this.output.getTransport().flush(true, function() {" << '\n';
          indent_up();
          f_service_ << indent() << js_let_type_ << "result = null;" << '\n';
          f_service_ << indent() << "try {" << '\n';
          f_service_ << indent() << "  result = self.recv_" << funname << "();" << '\n';
          f_service_ << indent() << "} catch (e) {" << '\n';
          f_service_ << indent() << "  result = e;" << '\n';
          f_service_ << indent() << "}" << '\n';
          f_service_ << indent() << "callback(result);" << '\n';
          indent_down();
          f_service_ << indent() << "});" << '\n';
        }
        indent_down();
        f_service_ << indent() << "} else {" << '\n';
        f_service_ << indent() << "  return this.output.getTransport().flush();" << '\n';
        f_service_ << indent() << "}" << '\n';
      }
    }

    indent_down();
    f_service_ << indent() << "}" << '\n';

    // Reset the transport and delete registered callback if there was a serialization error
    f_service_ << indent() << "catch (e) {" << '\n';
    indent_up();
    if (gen_node_) {
      f_service_ << indent() << "delete this._reqs[this.seqid()];" << '\n';
      f_service_ << indent() << "if (typeof " << outputVar << ".reset === 'function') {" << '\n';
      f_service_ << indent() << "  " << outputVar << ".reset();" << '\n';
      f_service_ << indent() << "}" << '\n';
    } else {
      f_service_ << indent() << "if (typeof " << outputVar << ".getTransport().reset === 'function') {" << '\n';
      f_service_ << indent() << "  " << outputVar << ".getTransport().reset();" << '\n';
      f_service_ << indent() << "}" << '\n';
    }
    f_service_ << indent() << "throw e;" << '\n';
    indent_down();
    f_service_ << indent() << "}" << '\n';

    indent_down();

    // Close send function
    if (gen_es6_) {
      indent(f_service_) << "}" << '\n';
    } else {
      indent(f_service_) << "};" << '\n';
    }

    // Receive function
    if (!(*f_iter)->is_oneway()) {
      std::string resultname = js_namespace(tservice->get_program()) + service_name_ + "_"
                               + (*f_iter)->get_name() + "_result";

      f_service_ << '\n';
      // Open receive function
      if (gen_node_) {
        if (gen_es6_) {
          indent(f_service_) << "recv_" << (*f_iter)->get_name() << " (input, mtype, rseqid) {" << '\n';
        } else {
          indent(f_service_) << js_namespace(tservice->get_program()) << service_name_
                      << "Client.prototype.recv_" << (*f_iter)->get_name()
                      << " = function(input,mtype,rseqid) {" << '\n';
        }
      } else {
        if (gen_es6_) {
          indent(f_service_) << "recv_" << (*f_iter)->get_name() << " () {" << '\n';
        } else {
          t_struct noargs(program_);

          t_function recv_function((*f_iter)->get_returntype(),
                                  string("recv_") + (*f_iter)->get_name(),
                                  &noargs);
          indent(f_service_) << js_namespace(tservice->get_program()) << service_name_
                    << "Client.prototype." << function_signature(&recv_function) << " {" << '\n';
        }
      }

      indent_up();

      std::string inputVar;
      if (gen_node_) {
        inputVar = "input";
      } else {
        inputVar = "this.input";
      }

      if (gen_node_) {
        f_service_ << indent() << js_const_type_ << "callback = this._reqs[rseqid] || function() {};" << '\n'
                   << indent() << "delete this._reqs[rseqid];" << '\n';
      } else {
        f_service_ << indent() << js_const_type_ << "ret = this.input.readMessageBegin();" << '\n'
                   << indent() << js_const_type_ << "mtype = ret.mtype;" << '\n';
      }

      f_service_ << indent() << "if (mtype == Thrift.MessageType.EXCEPTION) {" << '\n';

      indent_up();
      f_service_ << indent() << js_const_type_ << "x = new Thrift.TApplicationException();" << '\n'
                 << indent() << "x[Symbol.for(\"read\")](" << inputVar << ");" << '\n'
                 << indent() << inputVar << ".readMessageEnd();" << '\n'
                 << indent() << render_recv_throw("x") << '\n';
      scope_down(f_service_);

      f_service_ << indent() << js_const_type_ << "result = new " << resultname << "();" << '\n' << indent()
                 << "result[Symbol.for(\"read\")](" << inputVar << ");" << '\n';

      f_service_ << indent() << inputVar << ".readMessageEnd();" << '\n' << '\n';

      t_struct* xs = (*f_iter)->get_xceptions();
      const std::vector<t_field*>& xceptions = xs->get_members();
      vector<t_field*>::const_iterator x_iter;
      for (x_iter = xceptions.begin(); x_iter != xceptions.end(); ++x_iter) {
        f_service_ << indent() << "if (null !== result." << (*x_iter)->get_name() << ") {" << '\n'
                   << indent() << "  " << render_recv_throw("result." + (*x_iter)->get_name())
                   << '\n' << indent() << "}" << '\n';
      }

      // Careful, only return result if not a void function
      if (!(*f_iter)->get_returntype()->is_void()) {
        f_service_ << indent() << "if (null !== result.success) {" << '\n' << indent() << "  "
                   << render_recv_return("result.success") << '\n' << indent() << "}" << '\n';
        f_service_ << indent()
                   << render_recv_throw("'" + (*f_iter)->get_name() + " failed: unknown result'")
                   << '\n';
      } else {
        if (gen_node_) {
          indent(f_service_) << "callback(null);" << '\n';
        } else {
          indent(f_service_) << "return;" << '\n';
        }
      }

      // Close receive function
      indent_down();
      if (gen_es6_) {
        indent(f_service_) << "}" << '\n';
      } else {
        indent(f_service_) << "};" << '\n';
      }
    }
  }

  // Finish class definitions
  if (gen_ts_) {
    f_service_ts_ << ts_indent() << "}" << '\n';
  }
  if (gen_es6_) {
    indent_down();
    f_service_ << "};" << '\n';
  }

  if(gen_esm_) {
    f_service_ << "export { " << client_var << " as Client };" << '\n';
  } else if(gen_node_) {
    f_service_ << "exports.Client = " << client_var << ";" << '\n';
  }
}

std::string t_js_generator::render_recv_throw(std::string var) {
  if (gen_node_) {
    return "return callback(" + var + ");";
  } else {
    return "throw " + var + ";";
  }
}

std::string t_js_generator::render_recv_return(std::string var) {
  if (gen_node_) {
    return "return callback(null, " + var + ");";
  } else {
    return "return " + var + ";";
  }
}

/**
 * Deserializes a field of any type.
 */
void t_js_generator::generate_deserialize_field(ostream& out,
                                                t_field* tfield,
                                                string prefix,
                                                bool inclass) {
  (void)inclass;
  t_type* type = get_true_type(tfield->get_type());

  if (type->is_void()) {
    throw std::runtime_error("CANNOT GENERATE DESERIALIZE CODE FOR void TYPE: " + prefix + tfield->get_name());
  }

  string name = prefix + tfield->get_name();

  if (type->is_struct() || type->is_xception()) {
    generate_deserialize_struct(out, (t_struct*)type, name);
  } else if (type->is_container()) {
    generate_deserialize_container(out, type, name);
  } else if (type->is_base_type() || type->is_enum()) {
    indent(out) << name << " = input.";

    if (type->is_base_type()) {
      t_base_type::t_base tbase = ((t_base_type*)type)->get_base();
      switch (tbase) {
      case t_base_type::TYPE_VOID:
        throw std::runtime_error("compiler error: cannot serialize void field in a struct: " + name);
        break;
      case t_base_type::TYPE_STRING:
        out << (type->is_binary() ? "readBinary()" : "readString()");
        break;
      case t_base_type::TYPE_BOOL:
        out << "readBool()";
        break;
      case t_base_type::TYPE_I8:
        out << "readByte()";
        break;
      case t_base_type::TYPE_I16:
        out << "readI16()";
        break;
      case t_base_type::TYPE_I32:
        out << "readI32()";
        break;
      case t_base_type::TYPE_I64:
        out << "readI64()";
        break;
      case t_base_type::TYPE_DOUBLE:
        out << "readDouble()";
        break;
      default:
        throw std::runtime_error("compiler error: no JS name for base type " + t_base_type::t_base_name(tbase));
      }
    } else if (type->is_enum()) {
      out << "readI32()";
    }

    if (!gen_node_) {
      out << ".value";
    }

    out << ";" << '\n';
  } else {
    printf("DO NOT KNOW HOW TO DESERIALIZE FIELD '%s' TYPE '%s'\n",
           tfield->get_name().c_str(),
           type->get_name().c_str());
  }
}

/**
 * Generates an unserializer for a variable. This makes two key assumptions,
 * first that there is a const char* variable named data that points to the
 * buffer for deserialization, and that there is a variable protocol which
 * is a reference to a TProtocol serialization object.
 */
void t_js_generator::generate_deserialize_struct(ostream& out, t_struct* tstruct, string prefix) {
  out << indent() << prefix << " = new " << js_type_namespace(tstruct->get_program())
      << tstruct->get_name() << "();" << '\n' << indent() << prefix << "[Symbol.for(\"read\")](input);" << '\n';
}

void t_js_generator::generate_deserialize_container(ostream& out, t_type* ttype, string prefix) {
  string size = tmp("_size");
  string rtmp3 = tmp("_rtmp3");

  t_field fsize(g_type_i32, size);

  // Declare variables, read header
  if (ttype->is_map()) {
    out << indent() << prefix << " = {};" << '\n';

    out << indent() << js_const_type_ << rtmp3 << " = input.readMapBegin();" << '\n';
    out << indent() << js_const_type_ << size << " = " << rtmp3 << ".size || 0;" << '\n';

  } else if (ttype->is_set()) {

    out << indent() << prefix << " = [];" << '\n'
        << indent() << js_const_type_ << rtmp3 << " = input.readSetBegin();" << '\n'
        << indent() << js_const_type_ << size << " = " << rtmp3 << ".size || 0;" << '\n';

  } else if (ttype->is_list()) {

    out << indent() << prefix << " = [];" << '\n'
        << indent() << js_const_type_ << rtmp3 << " = input.readListBegin();" << '\n'
        << indent() << js_const_type_ << size << " = " << rtmp3 << ".size || 0;" << '\n';
  }

  // For loop iterates over elements
  string i = tmp("_i");
  indent(out) << "for (" << js_let_type_ << i << " = 0; " << i << " < " << size << "; ++" << i << ") {" << '\n';

  indent_up();

  if (ttype->is_map()) {
    if (!gen_node_) {
      out << indent() << "if (" << i << " > 0 ) {" << '\n' << indent()
          << "  if (input.rstack.length > input.rpos[input.rpos.length -1] + 1) {" << '\n'
          << indent() << "    input.rstack.pop();" << '\n' << indent() << "  }" << '\n' << indent()
          << "}" << '\n';
    }

    generate_deserialize_map_element(out, (t_map*)ttype, prefix);
  } else if (ttype->is_set()) {
    generate_deserialize_set_element(out, (t_set*)ttype, prefix);
  } else if (ttype->is_list()) {
    generate_deserialize_list_element(out, (t_list*)ttype, prefix);
  }

  scope_down(out);

  // Read container end
  if (ttype->is_map()) {
    indent(out) << "input.readMapEnd();" << '\n';
  } else if (ttype->is_set()) {
    indent(out) << "input.readSetEnd();" << '\n';
  } else if (ttype->is_list()) {
    indent(out) << "input.readListEnd();" << '\n';
  }
}

/**
 * Generates code to deserialize a map
 */
void t_js_generator::generate_deserialize_map_element(ostream& out, t_map* tmap, string prefix) {
  string key = tmp("key");
  string val = tmp("val");
  t_field fkey(tmap->get_key_type(), key);
  t_field fval(tmap->get_val_type(), val);

  indent(out) << declare_field(&fkey, false, false) << ";" << '\n';
  indent(out) << declare_field(&fval, false, false) << ";" << '\n';

  generate_deserialize_field(out, &fkey);
  generate_deserialize_field(out, &fval);

  indent(out) << prefix << "[" << key << "] = " << val << ";" << '\n';
}

void t_js_generator::generate_deserialize_set_element(ostream& out, t_set* tset, string prefix) {
  string elem = tmp("elem");
  t_field felem(tset->get_elem_type(), elem);

  indent(out) << js_let_type_ << elem << " = null;" << '\n';

  generate_deserialize_field(out, &felem);

  indent(out) << prefix << ".push(" << elem << ");" << '\n';
}

void t_js_generator::generate_deserialize_list_element(ostream& out,
                                                       t_list* tlist,
                                                       string prefix) {
  string elem = tmp("elem");
  t_field felem(tlist->get_elem_type(), elem);

  indent(out) << js_let_type_ << elem << " = null;" << '\n';

  generate_deserialize_field(out, &felem);

  indent(out) << prefix << ".push(" << elem << ");" << '\n';
}

/**
 * Serializes a field of any type.
 *
 * @param tfield The field to serialize
 * @param prefix Name to prepend to field name
 */
void t_js_generator::generate_serialize_field(ostream& out, t_field* tfield, string prefix) {
  t_type* type = get_true_type(tfield->get_type());

  // Do nothing for void types
  if (type->is_void()) {
    throw std::runtime_error("CANNOT GENERATE SERIALIZE CODE FOR void TYPE: " + prefix + tfield->get_name());
  }

  if (type->is_struct() || type->is_xception()) {
    generate_serialize_struct(out, (t_struct*)type, prefix + tfield->get_name());
  } else if (type->is_container()) {
    generate_serialize_container(out, type, prefix + tfield->get_name());
  } else if (type->is_base_type() || type->is_enum()) {

    string name = tfield->get_name();

    // Hack for when prefix is defined (always a hash ref)
    if (!prefix.empty())
      name = prefix + tfield->get_name();

    indent(out) << "output.";

    if (type->is_base_type()) {
      t_base_type::t_base tbase = ((t_base_type*)type)->get_base();
      switch (tbase) {
      case t_base_type::TYPE_VOID:
        throw std::runtime_error("compiler error: cannot serialize void field in a struct: " + name);
        break;
      case t_base_type::TYPE_STRING:
        out << (type->is_binary() ? "writeBinary(" : "writeString(") << name << ")";
        break;
      case t_base_type::TYPE_BOOL:
        out << "writeBool(" << name << ")";
        break;
      case t_base_type::TYPE_I8:
        out << "writeByte(" << name << ")";
        break;
      case t_base_type::TYPE_I16:
        out << "writeI16(" << name << ")";
        break;
      case t_base_type::TYPE_I32:
        out << "writeI32(" << name << ")";
        break;
      case t_base_type::TYPE_I64:
        out << "writeI64(" << name << ")";
        break;
      case t_base_type::TYPE_DOUBLE:
        out << "writeDouble(" << name << ")";
        break;
      default:
        throw std::runtime_error("compiler error: no JS name for base type " + t_base_type::t_base_name(tbase));
      }
    } else if (type->is_enum()) {
      out << "writeI32(" << name << ")";
    }
    out << ";" << '\n';

  } else {
    printf("DO NOT KNOW HOW TO SERIALIZE FIELD '%s%s' TYPE '%s'\n",
           prefix.c_str(),
           tfield->get_name().c_str(),
           type->get_name().c_str());
  }
}

/**
 * Serializes all the members of a struct.
 *
 * @param tstruct The struct to serialize
 * @param prefix  String prefix to attach to all fields
 */
void t_js_generator::generate_serialize_struct(ostream& out, t_struct* tstruct, string prefix) {
  (void)tstruct;
  indent(out) << prefix << "[Symbol.for(\"write\")](output);" << '\n';
}

/**
 * Writes out a container
 */
void t_js_generator::generate_serialize_container(ostream& out, t_type* ttype, string prefix) {
  if (ttype->is_map()) {
    indent(out) << "output.writeMapBegin(" << type_to_enum(((t_map*)ttype)->get_key_type()) << ", "
                << type_to_enum(((t_map*)ttype)->get_val_type()) << ", "
                << "Thrift.objectLength(" << prefix << "));" << '\n';
  } else if (ttype->is_set()) {
    indent(out) << "output.writeSetBegin(" << type_to_enum(((t_set*)ttype)->get_elem_type()) << ", "
                << prefix << ".length);" << '\n';

  } else if (ttype->is_list()) {

    indent(out) << "output.writeListBegin(" << type_to_enum(((t_list*)ttype)->get_elem_type())
                << ", " << prefix << ".length);" << '\n';
  }

  if (ttype->is_map()) {
    string kiter = tmp("kiter");
    string viter = tmp("viter");
    indent(out) << "for (" << js_let_type_ << kiter << " in " << prefix << ") {" << '\n';
    indent_up();
    indent(out) << "if (" << prefix << ".hasOwnProperty(" << kiter << ")) {" << '\n';
    indent_up();
    indent(out) << js_let_type_ << viter << " = " << prefix << "[" << kiter << "];" << '\n';
    generate_serialize_map_element(out, (t_map*)ttype, kiter, viter);
    scope_down(out);
    scope_down(out);

  } else if (ttype->is_set()) {
    string iter = tmp("iter");
    indent(out) << "for (" << js_let_type_ << iter << " in " << prefix << ") {" << '\n';
    indent_up();
    indent(out) << "if (" << prefix << ".hasOwnProperty(" << iter << ")) {" << '\n';
    indent_up();
    indent(out) << iter << " = " << prefix << "[" << iter << "];" << '\n';
    generate_serialize_set_element(out, (t_set*)ttype, iter);
    scope_down(out);
    scope_down(out);

  } else if (ttype->is_list()) {
    string iter = tmp("iter");
    indent(out) << "for (" << js_let_type_ << iter << " in " << prefix << ") {" << '\n';
    indent_up();
    indent(out) << "if (" << prefix << ".hasOwnProperty(" << iter << ")) {" << '\n';
    indent_up();
    indent(out) << iter << " = " << prefix << "[" << iter << "];" << '\n';
    generate_serialize_list_element(out, (t_list*)ttype, iter);
    scope_down(out);
    scope_down(out);
  }

  if (ttype->is_map()) {
    indent(out) << "output.writeMapEnd();" << '\n';
  } else if (ttype->is_set()) {
    indent(out) << "output.writeSetEnd();" << '\n';
  } else if (ttype->is_list()) {
    indent(out) << "output.writeListEnd();" << '\n';
  }
}

/**
 * Serializes the members of a map.
 *
 */
void t_js_generator::generate_serialize_map_element(ostream& out,
                                                    t_map* tmap,
                                                    string kiter,
                                                    string viter) {
  t_field kfield(tmap->get_key_type(), kiter);
  generate_serialize_field(out, &kfield);

  t_field vfield(tmap->get_val_type(), viter);
  generate_serialize_field(out, &vfield);
}

/**
 * Serializes the members of a set.
 */
void t_js_generator::generate_serialize_set_element(ostream& out, t_set* tset, string iter) {
  t_field efield(tset->get_elem_type(), iter);
  generate_serialize_field(out, &efield);
}

/**
 * Serializes the members of a list.
 */
void t_js_generator::generate_serialize_list_element(ostream& out, t_list* tlist, string iter) {
  t_field efield(tlist->get_elem_type(), iter);
  generate_serialize_field(out, &efield);
}

/**
 * Declares a field, which may include initialization as necessary.
 *
 * @param ttype The type
 */
string t_js_generator::declare_field(t_field* tfield, bool init, bool obj) {
  string result = "this." + tfield->get_name();

  if (!obj) {
    result = js_let_type_ + tfield->get_name();
  }

  if (init) {
    t_type* type = get_true_type(tfield->get_type());
    if (type->is_base_type()) {
      t_base_type::t_base tbase = ((t_base_type*)type)->get_base();
      switch (tbase) {
      case t_base_type::TYPE_VOID:
        break;
      case t_base_type::TYPE_STRING:
      case t_base_type::TYPE_BOOL:
      case t_base_type::TYPE_I8:
      case t_base_type::TYPE_I16:
      case t_base_type::TYPE_I32:
      case t_base_type::TYPE_I64:
      case t_base_type::TYPE_DOUBLE:
        result += " = null";
        break;
      default:
        throw std::runtime_error("compiler error: no JS initializer for base type " + t_base_type::t_base_name(tbase));
      }
    } else if (type->is_enum()) {
      result += " = null";
    } else if (type->is_map()) {
      result += " = null";
    } else if (type->is_container()) {
      result += " = null";
    } else if (type->is_struct() || type->is_xception()) {
      if (obj) {
        result += " = new " + js_type_namespace(type->get_program()) + type->get_name() + "()";
      } else {
        result += " = null";
      }
    }
  } else {
    result += " = null";
  }
  return result;
}

/**
 * Renders a function signature of the form 'type name(args)'
 *
 * @param tfunction Function definition
 * @return String of rendered function definition
 */
string t_js_generator::function_signature(t_function* tfunction,
                                          string prefix,
                                          bool include_callback) {

  string str;

  str = prefix + tfunction->get_name() + " = function(";

  str += argument_list(tfunction->get_arglist(), include_callback);

  str += ")";
  return str;
}

/**
 * Renders a field list
 */
string t_js_generator::argument_list(t_struct* tstruct, bool include_callback) {
  string result = "";

  const vector<t_field*>& fields = tstruct->get_members();
  vector<t_field*>::const_iterator f_iter;
  bool first = true;
  for (f_iter = fields.begin(); f_iter != fields.end(); ++f_iter) {
    if (first) {
      first = false;
    } else {
      result += ", ";
    }
    result += (*f_iter)->get_name();
  }

  if (include_callback) {
    if (!fields.empty()) {
      result += ", ";
    }
    result += "callback";
  }

  return result;
}

/**
 * Converts the parse type to a C++ enum string for the given type.
 */
string t_js_generator::type_to_enum(t_type* type) {
  type = get_true_type(type);

  if (type->is_base_type()) {
    t_base_type::t_base tbase = ((t_base_type*)type)->get_base();
    switch (tbase) {
    case t_base_type::TYPE_VOID:
      throw std::runtime_error("NO T_VOID CONSTRUCT");
    case t_base_type::TYPE_STRING:
      return "Thrift.Type.STRING";
    case t_base_type::TYPE_BOOL:
      return "Thrift.Type.BOOL";
    case t_base_type::TYPE_I8:
      return "Thrift.Type.BYTE";
    case t_base_type::TYPE_I16:
      return "Thrift.Type.I16";
    case t_base_type::TYPE_I32:
      return "Thrift.Type.I32";
    case t_base_type::TYPE_I64:
      return "Thrift.Type.I64";
    case t_base_type::TYPE_DOUBLE:
      return "Thrift.Type.DOUBLE";
    default:
      throw "compiler error: unhandled type";
    }
  } else if (type->is_enum()) {
    return "Thrift.Type.I32";
  } else if (type->is_struct() || type->is_xception()) {
    return "Thrift.Type.STRUCT";
  } else if (type->is_map()) {
    return "Thrift.Type.MAP";
  } else if (type->is_set()) {
    return "Thrift.Type.SET";
  } else if (type->is_list()) {
    return "Thrift.Type.LIST";
  }

  throw std::runtime_error("INVALID TYPE IN type_to_enum: " + type->get_name());
}

/**
 * Converts a t_type to a TypeScript type (string).
 * @param t_type Type to convert to TypeScript
 * @return String TypeScript type
 */
string t_js_generator::ts_get_type(t_type* type) {
  std::string ts_type;

  type = get_true_type(type);

  if (type->is_base_type()) {
    t_base_type::t_base tbase = ((t_base_type*)type)->get_base();
    switch (tbase) {
    case t_base_type::TYPE_STRING:
      ts_type = type->is_binary() ? "Buffer" : "string";
      break;
    case t_base_type::TYPE_BOOL:
      ts_type = "boolean";
      break;
    case t_base_type::TYPE_I8:
      ts_type = "any";
      break;
    case t_base_type::TYPE_I16:
    case t_base_type::TYPE_I32:
    case t_base_type::TYPE_DOUBLE:
      ts_type = "number";
      break;
    case t_base_type::TYPE_I64:
      ts_type = "Int64";
      break;
    case t_base_type::TYPE_VOID:
      ts_type = "void";
      break;
    default:
      throw "compiler error: unhandled type";
    }
  } else if (type->is_enum() || type->is_struct() || type->is_xception()) {
    std::string type_name;

    if (type->get_program()) {
      type_name = js_namespace(type->get_program());

      // If the type is not defined within the current program, we need to prefix it with the same name as
      // the generated "import" statement for the types containing program
      if(type->get_program() != program_)  {
        auto prefix = include_2_import_name.find(type->get_program());

        if(prefix != include_2_import_name.end()) {
          type_name.append(prefix->second);
          type_name.append(".");
        }
      }
    }

    type_name.append(type->get_name());
    ts_type = type_name;
  } else if (type->is_list() || type->is_set()) {
    t_type* etype;

    if (type->is_list()) {
      etype = ((t_list*)type)->get_elem_type();
    } else {
      etype = ((t_set*)type)->get_elem_type();
    }

    ts_type = ts_get_type(etype) + "[]";
  } else if (type->is_map()) {
    string ktype = ts_get_type(((t_map*)type)->get_key_type());
    string vtype = ts_get_type(((t_map*)type)->get_val_type());


    if (ktype == "number" || ktype == "string" ) {
      ts_type = "{ [k: " + ktype + "]: " + vtype + "; }";
    } else if ((((t_map*)type)->get_key_type())->is_enum()) {
      // Not yet supported (enum map): https://github.com/Microsoft/TypeScript/pull/2652
      //ts_type = "{ [k: " + ktype + "]: " + vtype + "; }";
      ts_type = "{ [k: number /*" + ktype + "*/]: " + vtype + "; }";
    } else {
      ts_type = "any";
    }
  }

  return ts_type;
}

/**
 * Renders a TypeScript function signature of the form 'name(args: types): type;'
 *
 * @param t_function Function definition
 * @param bool in-/exclude the callback argument
 * @return String of rendered function definition
 */
std::string t_js_generator::ts_function_signature(t_function* tfunction, bool include_callback) {
  string str;
  const vector<t_field*>& fields = tfunction->get_arglist()->get_members();
  vector<t_field*>::const_iterator f_iter;

  str = tfunction->get_name() + "(";

  bool has_written_optional = false;

  for (f_iter = fields.begin(); f_iter != fields.end(); ++f_iter) {
    // Ensure that non optional parameters do not follow optional parameters
    // E.g. public foo(a: string, b?: string; c: string) is invalid, c must be optional, or b non-optional
    string original_optional = ts_get_req(*f_iter);
    string optional = has_written_optional ? "?" : original_optional;
    has_written_optional = has_written_optional || optional.size() > 0;

    str += (*f_iter)->get_name() + optional + ": " + ts_get_type((*f_iter)->get_type());

    if (f_iter + 1 != fields.end() || (include_callback && fields.size() > 0)) {
      str += ", ";
    }
  }

  if (include_callback) {
    if (gen_node_) {
      t_struct* exceptions = tfunction->get_xceptions();
      string exception_types;
      if (exceptions) {
        const vector<t_field*>& members = exceptions->get_members();
        for (vector<t_field*>::const_iterator it = members.begin(); it != members.end(); ++it) {
          t_type* t = get_true_type((*it)->get_type());
          if (it == members.begin()) {
            exception_types = js_type_namespace(t->get_program()) + t->get_name();
          } else {
            exception_types += " | " + js_type_namespace(t->get_program()) + t->get_name();
          }
        }
      }
      if (exception_types == "") {
        str += "callback: (error: void, response: " + ts_get_type(tfunction->get_returntype()) + ")=>void): ";
      } else {
        str += "callback: (error: " + exception_types + ", response: " + ts_get_type(tfunction->get_returntype()) + ")=>void): ";
      }
    } else {
      str += "callback: (data: " + ts_get_type(tfunction->get_returntype()) + ")=>void): ";
    }

    if (gen_jquery_) {
      str += "JQueryPromise<" + ts_get_type(tfunction->get_returntype()) +">;";
    } else {
      str += "void;";
    }
  } else {
    if (gen_es6_) {
      str += "): Promise<" + ts_get_type(tfunction->get_returntype()) + ">;";
    }
    else {
      str += "): " + ts_get_type(tfunction->get_returntype()) + ";";
    }
  }

  return str;
}

/**
 * Takes a name and produces a valid NodeJS identifier from it
 *
 * @param name The name which shall become a valid NodeJS identifier
 * @return The modified name with the updated identifier
 */
std::string t_js_generator::make_valid_nodeJs_identifier(std::string const& name) {
  std::string str = name;
  if (str.empty()) {
    return str;
  }

  // tests rely on this
  assert(('A' < 'Z') && ('a' < 'z') && ('0' < '9'));

  // if the first letter is a number, we add an additional underscore in front of it
  char c = str.at(0);
  if (('0' <= c) && (c <= '9')) {
    str = "_" + str;
  }

  // following chars: letter, number or underscore
  for (size_t i = 0; i < str.size(); ++i) {
    c = str.at(i);
    if ((('A' > c) || (c > 'Z')) && (('a' > c) || (c > 'z')) && (('0' > c) || (c > '9'))
        && ('_' != c) && ('$' != c)) {
      str.replace(i, 1, "_");
    }
  }

  return str;
}

void t_js_generator::parse_imports(t_program* program, const std::string& imports_string) {
  if (program->get_recursive()) {
    throw std::invalid_argument("[-gen js:imports=] option is not usable in recursive code generation mode");
  }
  std::stringstream sstream(imports_string);
  std::string import;
  while (std::getline(sstream, import, ':')) {
    imports.emplace_back(import);
  }
  if (imports.empty()) {
    throw std::invalid_argument("invalid usage: [-gen js:imports=] requires at least one path "
          "(multiple paths are separated by ':')");
  }
  for (auto& import : imports) {
    // Strip trailing '/'
    if (!import.empty() && import[import.size() - 1] == '/') {
      import = import.substr(0, import.size() - 1);
    }
    if (import.empty()) {
      throw std::invalid_argument("empty paths are not allowed in imports");
    }
    std::ifstream episode_file;
    string line;
    const auto episode_file_path = import + "/" + episode_file_name;
    episode_file.open(episode_file_path);
    if (!episode_file) {
      throw std::runtime_error("failed to open the file '" + episode_file_path + "'");
    }
    while (std::getline(episode_file, line)) {
      const auto separator_position = line.find(':');
      if (separator_position == string::npos) {
        // could not find the separator in the line
        throw std::runtime_error("the episode file '" + episode_file_path + "' is malformed, the line '" + line
          + "' does not have a key:value separator ':'");
      }
      const auto module_name = line.substr(0, separator_position);
      const auto import_path = line.substr(separator_position + 1);
      if (module_name.empty()) {
        throw std::runtime_error("the episode file '" + episode_file_path + "' is malformed, the module name is empty");
      }
      if (import_path.empty()) {
        throw std::runtime_error("the episode file '" + episode_file_path + "' is malformed, the import path is empty");
      }
      const auto module_import_path = import.substr(import.find_last_of('/') + 1) + "/" + import_path;
      const auto result = module_name_2_import_path.emplace(module_name, module_import_path);
      if (!result.second) {
        throw std::runtime_error("multiple providers of import path found for " + module_name
          + "\n\t" + module_import_path + "\n\t" + result.first->second);
      }
    }
  }
}
void t_js_generator::parse_thrift_package_output_directory(const std::string& thrift_package_output_directory) {
  thrift_package_output_directory_ = thrift_package_output_directory;
  // Strip trailing '/'
  if (!thrift_package_output_directory_.empty() && thrift_package_output_directory_[thrift_package_output_directory_.size() - 1] == '/') {
    thrift_package_output_directory_ = thrift_package_output_directory_.substr(0, thrift_package_output_directory_.size() - 1);
  }
  // Check that the thrift_package_output_directory is not empty after stripping
  if (thrift_package_output_directory_.empty()) {
    throw std::invalid_argument("the thrift_package_output_directory argument must not be empty");
  } else {
    gen_episode_file_ = true;
  }
}

/**
 * Checks is the specified field name is contained in the specified field vector
 */
bool t_js_generator::find_field(const std::vector<t_field*>& fields, const std::string& name) {
    vector<t_field*>::const_iterator f_iter;
    for (f_iter = fields.begin(); f_iter != fields.end(); ++f_iter) {
        if ((*f_iter)->get_name() == name) {
          return true;
        }
    }

    return false;
}

/**
 * Given a vector of fields, generate a valid identifier name that does not conflict with avaliable field names
 */
std::string t_js_generator::next_identifier_name(const std::vector<t_field*>& fields, const std::string& base_name) {
  // Search through fields until a match is not found, if a match is found prepend "_" to the identifier name
  std::string current_name = this->make_valid_nodeJs_identifier(base_name);
  while(this->find_field(fields, current_name)) {
    current_name = this->make_valid_nodeJs_identifier("_" + current_name);
  }

  return current_name;
}

std::string t_js_generator::display_name() const {
  return "Javascript";
}


THRIFT_REGISTER_GENERATOR(js,
                          "Javascript",
                          "    jquery:          Generate jQuery compatible code.\n"
                          "    node:            Generate node.js compatible code.\n"
                          "    ts:              Generate TypeScript definition files.\n"
                          "    with_ns:         Create global namespace objects when using node.js\n"
                          "    es6:             Create ES6 code with Promises\n"
                          "    thrift_package_output_directory=<path>:\n"
                          "                     Generate episode file and use the <path> as prefix\n"
                          "    imports=<paths_to_modules>:\n"
                          "                     ':' separated list of paths of modules that has episode files in their root\n")
