#include "./options.hpp"

#include <dds/error/errors.hpp>
#include <dds/error/on_error.hpp>
#include <dds/error/toolchain.hpp>
#include <dds/pkg/db.hpp>
#include <dds/toolchain/from_json.hpp>
#include <dds/toolchain/toolchain.hpp>

#include <debate/enum.hpp>
#include <fansi/styled.hpp>

using namespace dds;
using namespace debate;
using namespace fansi::literals;

namespace {

struct setup {
    dds::cli::options& opts;

    explicit setup(dds::cli::options& opts)
        : opts(opts) {}

    // Util argument common to a lot of operations
    argument if_exists_arg{
        .long_spellings = {"if-exists"},
        .help           = "What to do if the resource already exists",
        .valname        = "{replace,skip,fail}",
        .action         = put_into(opts.if_exists),
    };

    argument if_missing_arg{
        .long_spellings = {"if-missing"},
        .help           = "What to do if the resource does not exist",
        .valname        = "{fail,ignore}",
        .action         = put_into(opts.if_missing),
    };

    argument toolchain_arg{
        .long_spellings  = {"toolchain"},
        .short_spellings = {"t"},
        .help            = "The toolchain to use when building",
        .valname         = "<file-or-id>",
        .action          = put_into(opts.toolchain),
    };

    argument project_arg{
        .long_spellings  = {"project"},
        .short_spellings = {"p"},
        .help            = "The project to build. If not given, uses the current working directory",
        .valname         = "<project-path>",
        .action          = put_into(opts.project_dir),
    };

    argument no_warn_arg{
        .long_spellings = {"no-warn", "no-warnings"},
        .help           = "Disable build warnings",
        .nargs          = 0,
        .action         = store_true(opts.disable_warnings),
    };

    argument out_arg{
        .long_spellings  = {"out", "output"},
        .short_spellings = {"o"},
        .help            = "Path to the output",
        .valname         = "<path>",
        .action          = put_into(opts.out_path),
    };

    argument lm_index_arg{
        .long_spellings = {"libman-index"},
        .help           = "Path to a libman index to use",
        .valname        = "<lmi-path>",
        .action         = put_into(opts.build.lm_index),
    };

    argument jobs_arg{
        .long_spellings  = {"jobs"},
        .short_spellings = {"j"},
        .help            = "Set the maximum number of parallel jobs to execute",
        .valname         = "<job-count>",
        .action          = put_into(opts.jobs),
    };

    argument repoman_repo_dir_arg{
        .help     = "The directory of the repository to manage",
        .valname  = "<repo-dir>",
        .required = true,
        .action   = put_into(opts.repoman.repo_dir),
    };

    argument tweaks_dir_arg{
        .long_spellings  = {"tweaks-dir"},
        .short_spellings = {"TD"},
        .help
        = "Base directory of "
          "\x1b]8;;https://vector-of-bool.github.io/2020/10/04/lib-configuration.html\x1b\\tweak "
          "headers\x1b]8;;\x1b\\ that should be available to the build.",
        .valname = "<dir>",
        .action  = put_into(opts.build.tweaks_dir),
    };

    void do_setup(argument_parser& parser) noexcept {
        parser.add_argument({
            .long_spellings  = {"log-level"},
            .short_spellings = {"l"},
            .help            = ""
                    "Set the dds logging level. One of 'trace', 'debug', 'info', \n"
                    "'warn', 'error', 'critical', or 'silent'",
            .valname = "<level>",
            .action  = put_into(opts.log_level),
        });
        parser.add_argument({
            .long_spellings = {"data-dir"},
            .help
            = ""
              "(Advanced) "
              "Override dds's data directory. This is used for various caches and databases.\n"
              "The default is a user-local directory that differs depending on platform.",
            .valname = "<directory>",
            .action  = put_into(opts.data_dir),
        });
        parser.add_argument({
            .long_spellings = {"pkg-cache-dir"},
            .help           = "(Advanced) Override dds's local package cache directory.",
            .valname        = "<directory>",
            .action         = put_into(opts.pkg_cache_dir),
        });
        parser.add_argument({
            .long_spellings = {"pkg-db-path"},
            .help           = "(Advanced) Override dds's default package database path.",
            .valname        = "<database-path>",
            .action         = put_into(opts.pkg_db_dir),
        });

        setup_main_commands(parser.add_subparsers({
            .description = "The operation to perform",
            .action      = put_into(opts.subcommand),
        }));
    }

    void setup_main_commands(subparser_group& group) {
        setup_build_cmd(group.add_parser({
            .name = "build",
            .help = "Build a project",
        }));
        setup_compile_file_cmd(group.add_parser({
            .name = "compile-file",
            .help = "Compile individual files in the project",
        }));
        setup_build_deps_cmd(group.add_parser({
            .name = "build-deps",
            .help = "Build a set of dependencies and generate a libman index",
        }));
        setup_pkg_cmd(group.add_parser({
            .name = "pkg",
            .help = "Manage packages and package remotes",
        }));
        setup_repoman_cmd(group.add_parser({
            .name = "repoman",
            .help = "Manage a dds package repository",
        }));
        setup_install_yourself_cmd(group.add_parser({
            .name = "install-yourself",
            .help = "Have this dds executable install itself onto your PATH",
        }));
    }

    void setup_build_cmd(argument_parser& build_cmd) {
        build_cmd.add_argument(toolchain_arg.dup());
        build_cmd.add_argument(project_arg.dup());
        build_cmd.add_argument({
            .long_spellings = {"no-tests"},
            .help           = "Do not build and run project tests",
            .nargs          = 0,
            .action         = debate::store_false(opts.build.want_tests),
        });
        build_cmd.add_argument({
            .long_spellings = {"no-apps"},
            .help           = "Do not build project applications",
            .nargs          = 0,
            .action         = debate::store_false(opts.build.want_apps),
        });
        build_cmd.add_argument(no_warn_arg.dup());
        build_cmd.add_argument(out_arg.dup()).help = "Directory where dds will write build results";

        build_cmd.add_argument({
            .long_spellings = {"add-repo"},
            .help           = ""
                    "Add remote repositories to the package database before building\n"
                    "(Implies --update-repos)",
            .valname    = "<repo-url>",
            .can_repeat = true,
            .action     = debate::push_back_onto(opts.build.add_repos),
        });
        build_cmd.add_argument({
            .long_spellings  = {"update-repos"},
            .short_spellings = {"U"},
            .help            = "Update package repositories before building",
            .nargs           = 0,
            .action          = debate::store_true(opts.build.update_repos),
        });
        build_cmd.add_argument(lm_index_arg.dup()).help
            = "Path to a libman index file to use for loading project dependencies";
        build_cmd.add_argument(jobs_arg.dup());
        build_cmd.add_argument(tweaks_dir_arg.dup());
    }

    void setup_compile_file_cmd(argument_parser& compile_file_cmd) noexcept {
        compile_file_cmd.add_argument(project_arg.dup());
        compile_file_cmd.add_argument(toolchain_arg.dup());
        compile_file_cmd.add_argument(no_warn_arg.dup()).help = "Disable compiler warnings";
        compile_file_cmd.add_argument(jobs_arg.dup()).help
            = "Set the maximum number of files to compile in parallel";
        compile_file_cmd.add_argument(lm_index_arg.dup());
        compile_file_cmd.add_argument(out_arg.dup());
        compile_file_cmd.add_argument(tweaks_dir_arg.dup());
        compile_file_cmd.add_argument({
            .help       = "One or more source files to compile",
            .valname    = "<source-files>",
            .can_repeat = true,
            .action     = debate::push_back_onto(opts.compile_file.files),
        });
    }

    void setup_build_deps_cmd(argument_parser& build_deps_cmd) noexcept {
        build_deps_cmd.add_argument(toolchain_arg.dup()).required;
        build_deps_cmd.add_argument(jobs_arg.dup());
        build_deps_cmd.add_argument(out_arg.dup());
        build_deps_cmd.add_argument(lm_index_arg.dup()).help
            = "Destination path for the generated libman index file";
        build_deps_cmd.add_argument({
            .long_spellings  = {"deps-file"},
            .short_spellings = {"d"},
            .help            = "Path to a JSON5 file listing dependencies",
            .valname         = "<deps-file>",
            .can_repeat      = true,
            .action          = debate::push_back_onto(opts.build_deps.deps_files),
        });
        build_deps_cmd.add_argument({
            .long_spellings = {"cmake"},
            .help = "Generate a CMake file at the given path that will create import targets for "
                    "the dependencies",
            .valname = "<file-path>",
            .action  = debate::put_into(opts.build_deps.cmake_file),
        });
        build_deps_cmd.add_argument(tweaks_dir_arg.dup());
        build_deps_cmd.add_argument({
            .help       = "Dependency statement strings",
            .valname    = "<dependency>",
            .can_repeat = true,
            .action     = debate::push_back_onto(opts.build_deps.deps),
        });
    }

    void setup_pkg_cmd(argument_parser& pkg_cmd) {
        auto& pkg_group = pkg_cmd.add_subparsers({
            .valname = "<pkg-subcommand>",
            .action  = put_into(opts.pkg.subcommand),
        });
        setup_pkg_init_db_cmd(pkg_group.add_parser({
            .name = "init-db",
            .help = "Initialize a new package database file (Path specified with '--pkg-db-path')",
        }));
        pkg_group.add_parser({
            .name = "ls",
            .help = "List locally available packages",
        });
        setup_pkg_create_cmd(pkg_group.add_parser({
            .name = "create",
            .help = "Create a source distribution archive of a project",
        }));
        setup_pkg_get_cmd(pkg_group.add_parser({
            .name = "get",
            .help = "Obtain a copy of a package from a remote",
        }));
        setup_pkg_import_cmd(pkg_group.add_parser({
            .name = "import",
            .help = "Import a source distribution archive into the local package cache",
        }));
        setup_pkg_repo_cmd(pkg_group.add_parser({
            .name = "repo",
            .help = "Manage package repositories",
        }));
        setup_pkg_search_cmd(pkg_group.add_parser({
            .name = "search",
            .help = "Search for packages available to download",
        }));
    }

    void setup_pkg_create_cmd(argument_parser& pkg_create_cmd) {
        pkg_create_cmd.add_argument(project_arg.dup()).help
            = "Path to the project for which to create a source distribution.\n"
              "Default is the current working directory.";
        pkg_create_cmd.add_argument(out_arg.dup()).help
            = "Destination path for the source distributioon archive";
        pkg_create_cmd.add_argument(if_exists_arg.dup()).help
            = "What to do if the destination names an existing file";
    }

    void setup_pkg_get_cmd(argument_parser& pkg_get_cmd) {
        pkg_get_cmd.add_argument({
            .valname    = "<pkg-id>",
            .can_repeat = true,
            .action     = push_back_onto(opts.pkg.get.pkgs),
        });
        pkg_get_cmd.add_argument(out_arg.dup()).help
            = "Directory where obtained packages will be placed.\n"
              "Default is the current working directory.";
    }

    void setup_pkg_init_db_cmd(argument_parser& pkg_init_db_cmd) {
        pkg_init_db_cmd.add_argument(if_exists_arg.dup()).help
            = "What to do if the database file already exists";
    }

    void setup_pkg_import_cmd(argument_parser& pkg_import_cmd) noexcept {
        pkg_import_cmd.add_argument({
            .long_spellings = {"stdin"},
            .help           = "Import a source distribution archive from standard input",
            .nargs          = 0,
            .action         = debate::store_true(opts.pkg.import.from_stdin),
        });
        pkg_import_cmd.add_argument(if_exists_arg.dup()).help
            = "What to do if the package already exists in the local cache";
        pkg_import_cmd.add_argument({
            .help       = "One or more paths/URLs to source distribution archives to import",
            .valname    = "<path-or-url>",
            .can_repeat = true,
            .action     = debate::push_back_onto(opts.pkg.import.items),
        });
    }

    void setup_pkg_repo_cmd(argument_parser& pkg_repo_cmd) noexcept {
        auto& pkg_repo_grp = pkg_repo_cmd.add_subparsers({
            .valname = "<pkg-repo-subcommand>",
            .action  = put_into(opts.pkg.repo.subcommand),
        });
        setup_pkg_repo_add_cmd(pkg_repo_grp.add_parser({
            .name = "add",
            .help = "Add a package repository",
        }));
        setup_pkg_repo_remove_cmd(pkg_repo_grp.add_parser({
            .name = "remove",
            .help = "Remove one or more package repositories",
        }));

        pkg_repo_grp.add_parser({
            .name = "update",
            .help = "Update package repository information",
        });
        pkg_repo_grp.add_parser({
            .name = "ls",
            .help = "List locally registered package repositories",
        });
    }

    void setup_pkg_repo_add_cmd(argument_parser& pkg_repo_add_cmd) noexcept {
        pkg_repo_add_cmd.add_argument({
            .help     = "URL of a repository to add",
            .valname  = "<url>",
            .required = true,
            .action   = debate::put_into(opts.pkg.repo.add.url),
        });
        pkg_repo_add_cmd.add_argument({
            .long_spellings = {"no-update"},
            .help           = "Do not immediately update for the new package repository",
            .nargs          = 0,
            .action         = debate::store_false(opts.pkg.repo.add.update),
        });
    }

    void setup_pkg_repo_remove_cmd(argument_parser& pkg_repo_remove_cmd) noexcept {
        pkg_repo_remove_cmd.add_argument({
            .help       = "Name of one or more repositories to remove",
            .valname    = "<repo-name>",
            .can_repeat = true,
            .action     = push_back_onto(opts.pkg.repo.remove.names),
        });
        pkg_repo_remove_cmd.add_argument(if_missing_arg.dup()).help
            = "What to do if any of the named repositories do not exist";
    }

    void setup_pkg_search_cmd(argument_parser& pkg_repo_search_cmd) noexcept {
        pkg_repo_search_cmd.add_argument({
            .help
            = "A name or glob-style pattern. Only matching packages will be returned. \n"
              "Searching is case-insensitive. Only the .italic[name] will be matched (not the \n"
              "version).\n\nIf this parameter is omitted, the search will return .italic[all] \n"
              "available packages."_styled,
            .valname = "<name-or-pattern>",
            .action  = put_into(opts.pkg.search.pattern),
        });
    }

    void setup_repoman_cmd(argument_parser& repoman_cmd) {
        auto& grp = repoman_cmd.add_subparsers({
            .valname = "<repoman-subcommand>",
            .action  = put_into(opts.repoman.subcommand),
        });

        setup_repoman_init_cmd(grp.add_parser({
            .name = "init",
            .help = "Initialize a directory as a new repository",
        }));
        auto& ls_cmd = grp.add_parser({
            .name = "ls",
            .help = "List the contents of a package repository directory",
        });
        ls_cmd.add_argument(repoman_repo_dir_arg.dup());
        setup_repoman_add_cmd(grp.add_parser({
            .name = "add",
            .help = "Add a package listing to the repository by URL",
        }));
        setup_repoman_import_cmd(grp.add_parser({
            .name = "import",
            .help = "Import a source distribution into the repository",
        }));
        setup_repoman_remove_cmd(grp.add_parser({
            .name = "remove",
            .help = "Remove packages from a package repository",
        }));
    }

    void setup_repoman_init_cmd(argument_parser& repoman_init_cmd) {
        repoman_init_cmd.add_argument(repoman_repo_dir_arg.dup());
        repoman_init_cmd.add_argument(if_exists_arg.dup()).help
            = "What to do if the directory exists and is already repository";
        repoman_init_cmd.add_argument({
            .long_spellings  = {"name"},
            .short_spellings = {"n"},
            .help            = "Specifiy the name of the new repository",
            .valname         = "<name>",
            .action          = put_into(opts.repoman.init.name),
        });
    }

    void setup_repoman_import_cmd(argument_parser& repoman_import_cmd) {
        repoman_import_cmd.add_argument(repoman_repo_dir_arg.dup());
        repoman_import_cmd.add_argument(if_exists_arg.dup()).help
            = "Behavior when the package already exists in the repository";
        repoman_import_cmd.add_argument({
            .help       = "Paths to source distribution archives to import",
            .valname    = "<sdist-file-path>",
            .can_repeat = true,
            .action     = push_back_onto(opts.repoman.import.files),
        });
    }

    void setup_repoman_add_cmd(argument_parser& repoman_add_cmd) {
        repoman_add_cmd.add_argument(repoman_repo_dir_arg.dup());
        repoman_add_cmd.add_argument({
            .help     = "URL to add to the repository",
            .valname  = "<url>",
            .required = true,
            .action   = put_into(opts.repoman.add.url_str),
        });
        repoman_add_cmd.add_argument({
            .long_spellings  = {"description"},
            .short_spellings = {"d"},
            .action          = put_into(opts.repoman.add.description),
        });
    }

    void setup_repoman_remove_cmd(argument_parser& repoman_remove_cmd) {
        repoman_remove_cmd.add_argument(repoman_repo_dir_arg.dup());
        repoman_remove_cmd.add_argument({
            .help       = "One or more identifiers of packages to remove",
            .valname    = "<pkg-id>",
            .can_repeat = true,
            .action     = push_back_onto(opts.repoman.remove.pkgs),
        });
    }

    void setup_install_yourself_cmd(argument_parser& install_yourself_cmd) {
        install_yourself_cmd.add_argument({
            .long_spellings = {"where"},
            .help = "The scope of the installation. For .bold[system], installs in a global \n"
                    "directory for all users of the system. For .bold[user], installs in a \n"
                    "user-specific directory for executable binaries."_styled,
            .valname = "{user,system}",
            .action  = put_into(opts.install_yourself.where),
        });
        install_yourself_cmd.add_argument({
            .long_spellings = {"dry-run"},
            .help
            = "Do not actually perform any operations, but log what .italic[would] happen"_styled,
            .nargs  = 0,
            .action = store_true(opts.dry_run),
        });
        install_yourself_cmd.add_argument({
            .long_spellings = {"no-modify-path"},
            .help           = "Do not attempt to modify the PATH environment variable",
            .nargs          = 0,
            .action         = store_false(opts.install_yourself.fixup_path_env),
        });
        install_yourself_cmd.add_argument({
            .long_spellings = {"symlink"},
            .help = "Create a symlink at the installed location to the existing 'dds' executable\n"
                    "instead of copying the executable file",
            .nargs  = 0,
            .action = store_true(opts.install_yourself.symlink),
        });
    }
};

}  // namespace

void cli::options::setup_parser(debate::argument_parser& parser) noexcept {
    setup{*this}.do_setup(parser);
}

pkg_db dds::cli::options::open_pkg_db() const {
    return pkg_db::open(this->pkg_db_dir.value_or(pkg_db::default_path()));
}

toolchain dds::cli::options::load_toolchain() const {
    if (!toolchain) {
        auto def = dds::toolchain::get_default();
        if (!def) {
            throw_user_error<errc::no_default_toolchain>();
        }
        return *def;
    }
    // Convert the given string to a toolchain
    auto& tc_str = *toolchain;
    DDS_E_SCOPE(e_loading_toolchain{tc_str});
    if (tc_str.starts_with(":")) {
        DDS_E_SCOPE(e_toolchain_builtin{tc_str});
        auto default_tc = tc_str.substr(1);
        auto tc         = dds::toolchain::get_builtin(default_tc);
        if (!tc.has_value()) {
            throw_user_error<
                errc::invalid_builtin_toolchain>("Invalid built-in toolchain name '{}'",
                                                 default_tc);
        }
        return std::move(*tc);
    } else {
        DDS_E_SCOPE(e_toolchain_file{tc_str});
        return parse_toolchain_json5(slurp_file(tc_str));
    }
}
