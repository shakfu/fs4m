#!/usr/bin/env python3
"""bundler is a utility that helps bundle dynamic libraries inside macOS app bundles.

It is a python3 translation of the cpp macdylibbundler utility by Marianne Gagnon 
which can be found at https://github.com/auriamg/macdylibbundler

usage: bundler [-h] [-b] [-d DEST_DIR] [-p INSTALL_PATH] [-s SEARCH_PATH] [-of]
               [-od] [-cd] [-ns] [-i IGNORE]
               target [target ...]

bundler is a utility that helps bundle dynamic libraries inside macOS app
bundles.

positional arguments:
  target                file to fix (executable or app plug-in)

options:
  -h, --help            show this help message and exit
  -b, --bundle-deps     bundle dependencies
  -d, --dest-dir DEST_DIR
                        directory to send bundled libraries (relative to cwd)
  -p, --install-path INSTALL_PATH
                        'inner' path of bundled libraries (usually relative to
                        executable
  -s, --search-path SEARCH_PATH
                        directory to add to list of locations searched
  -of, --overwrite-files
                        allow overwriting files in output directory
  -od, --overwrite-dir  totally overwrite output directory if it already exists.
                        implies --create-dir
  -cd, --create-dir     creates output directory if necessary
  -ns, --no-codesign    disables ad-hoc codesigning
  -i, --ignore IGNORE   will ignore libraries in this directory

e.g: bundler -od -b -d My.app/Contents/libs/ My.app/Contents/MacOS/main
"""

import argparse
import datetime
import logging
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Optional, List, Dict

DEBUG = True
COLOR = True
CAVEAT = ("MAY NOT CORRECTLY HANDLE THIS DEPENDENCY: "
          "Manually check the executable with 'otool -L'")

# type aliases
Pathlike = str | Path

# ----------------------------------------------------------------------------
# logging config

class CustomFormatter(logging.Formatter):
    """Custom logging formatting class with color support."""

    class color:
        """text colors"""
        white = "\x1b[97;20m"
        grey = "\x1b[38;20m"
        green = "\x1b[32;20m"
        cyan = "\x1b[36;20m"
        yellow = "\x1b[33;20m"
        red = "\x1b[31;20m"
        bold_red = "\x1b[31;1m"
        reset = "\x1b[0m"

    cfmt = (
        f"{color.white}%(delta)s{color.reset} - "
        f"{{}}%(levelname)s{color.reset} - "
        f"{color.white}%(name)s.%(funcName)s{color.reset} - "
        f"{color.grey}%(message)s{color.reset}"
    )

    FORMATS = {
        logging.DEBUG: cfmt.format(color.grey),
        logging.INFO: cfmt.format(color.green),
        logging.WARNING: cfmt.format(color.yellow),
        logging.ERROR: cfmt.format(color.red),
        logging.CRITICAL: cfmt.format(color.bold_red),
    }

    def __init__(self, use_color: bool = COLOR):
        self.use_color = use_color
        self.fmt = "%(delta)s - %(levelname)s - %(name)s.%(funcName)s - %(message)s"

    def format(self, record: logging.LogRecord) -> str:
        """Format the log record with color if enabled."""
        if not self.use_color:
            log_fmt = self.fmt
        else:
            log_fmt = self.FORMATS[record.levelno]
        duration = datetime.datetime.fromtimestamp(
            record.relativeCreated / 1000, datetime.UTC
        )
        record.delta = duration.strftime("%H:%M:%S")
        formatter = logging.Formatter(log_fmt)
        return formatter.format(record)

# Configure logging
stream_handler = logging.StreamHandler()
stream_handler.setFormatter(CustomFormatter())
logging.basicConfig(
    level=logging.DEBUG if DEBUG else logging.INFO,
    handlers=[stream_handler],
)

# ----------------------------------------------------------------------------
# classes

class Settings:
    """Settings for a DylibBundler instance."""

    def __init__(
        self,
        dest_dir: Pathlike = Path("./libs/"),
        overwrite_files: bool = False,
        overwrite_dir: bool = False,
        create_dir: bool = False,
        codesign: bool = True,
        bundle_libs: bool = True,
        inside_lib_path: str = "@executable_path/../libs/",
        files_to_fix: Optional[List[Pathlike]] = None,
        prefixes_to_ignore: Optional[List[Pathlike]] = None,
        search_paths: Optional[List[Pathlike]] = None,
    ):
        self.dest_dir = Path(dest_dir)
        self.overwrite_files = overwrite_files
        self.overwrite_dir = overwrite_dir
        self.create_dir = create_dir
        self.codesign = codesign
        self.bundle_libs = bundle_libs
        self.inside_lib_path = inside_lib_path
        self.files_to_fix = [Path(f) for f in (files_to_fix or [])]
        self.prefixes_to_ignore = [Path(p) for p in (prefixes_to_ignore or [])]
        self.search_paths = [Path(p) for p in (search_paths or [])]

    @property
    def can_overwrite_files(self) -> bool:
        """Whether to overwrite existing files in the output directory."""
        return self.overwrite_files

    @property
    def can_overwrite_dir(self) -> bool:
        """Whether to overwrite the output directory if it already exists."""
        return self.overwrite_dir

    @property
    def can_create_dir(self) -> bool:
        """Whether to create the output directory if it doesn't exist."""
        return self.create_dir

    @property
    def can_codesign(self) -> bool:
        """Whether to codesign the bundled libraries."""
        return self.codesign

    @property
    def bundle_libs_enabled(self) -> bool:
        """Whether to bundle the libraries."""
        return self.bundle_libs

    @property
    def file_to_fix_amount(self) -> int:
        """The number of files to fix."""
        return len(self.files_to_fix)

    @property
    def search_path_amount(self) -> int:
        """The number of search paths."""
        return len(self.search_paths)

    def add_search_path(self, path: Pathlike) -> None:
        """Add a search path."""
        self.search_paths.append(Path(path))

    def search_path(self, index: int) -> Path:
        """Get a search path by index."""
        return self.search_paths[index]

    def add_file_to_fix(self, path: Pathlike) -> None:
        """Add a file to fix."""
        self.files_to_fix.append(Path(path))

    def file_to_fix(self, index: int) -> Path:
        """Get a file to fix by index."""
        return self.files_to_fix[index]

    def ignore_prefix(self, prefix: Pathlike) -> None:
        """Ignore a prefix."""
        self.prefixes_to_ignore.append(Path(prefix))

    def is_system_library(self, prefix: Pathlike) -> bool:
        """Check if a prefix is a system library."""
        prefix = str(prefix)
        return prefix.startswith("/usr/lib/") or prefix.startswith("/System/Library/")

    def is_prefix_ignored(self, prefix: Pathlike) -> bool:
        """Check if a prefix is ignored."""
        return Path(prefix) in self.prefixes_to_ignore

    def is_prefix_bundled(self, prefix: Pathlike) -> bool:
        """Check if a prefix is bundled."""
        prefix = str(prefix)
        if ".framework" in prefix:
            return False
        if "@executable_path" in prefix:
            return False
        if self.is_system_library(prefix):
            return False
        if self.is_prefix_ignored(prefix):
            return False
        return True


class Dependency:
    """A dependency of a file."""

    def __init__(self, parent: "DylibBundler", path: Pathlike, dependent_file: Pathlike):
        self.parent = parent
        self.settings = parent.settings
        self.filename = ""
        self.prefix = Path()
        self.symlinks: List[Path] = []
        self.new_name = ""
        self.log = logging.getLogger(self.__class__.__name__)

        # Resolve the original file path
        path = Path(str(path).strip())
        dependent_file = Path(dependent_file)

        if self._is_rpath(path):
            original_file = self.search_filename_in_rpaths(path, dependent_file)
        else:
            try:
                original_file = path.resolve()
            except OSError:
                self.log.warning("Cannot resolve path '%s'", path)
                original_file = path

        # Check if given path is a symlink
        if original_file != path:
            self.add_symlink(path)

        self.filename = original_file.name
        self.prefix = original_file.parent

        # Check if this dependency should be bundled
        if not self.settings.is_prefix_bundled(self.prefix):
            return

        # Check if the lib is in a known location
        if not self.prefix or not (self.prefix / self.filename).exists():
            if not self.settings.search_paths:
                self._init_search_paths()

            # Check if file is contained in one of the paths
            for search_path in self.settings.search_paths:
                if (search_path / self.filename).exists():
                    self.log.info(f"FOUND {self.filename} in {search_path}")
                    self.prefix = search_path
                    break

        # If location still unknown, ask user for search path
        if not self.settings.is_prefix_ignored(self.prefix) and (
            not self.prefix or not (self.prefix / self.filename).exists()
        ):
            self.log.warning("Library %s has an incomplete name (location unknown)", self.filename)
            self.settings.add_search_path(self._get_user_input_dir_for_file(self.filename))

        self.new_name = self.filename

    def _get_user_input_dir_for_file(self, filename: str) -> Path:
        """Get a user input directory for a file."""
        for search_path in self.settings.search_paths:
            if (search_path / filename).exists():
                self.log.info("%s was found. %s", search_path / filename, CAVEAT)
                return search_path

        while True:
            sys.stdout.flush()
            prefix = input("Please specify the directory where this library is "
                         "located (or enter 'quit' to abort): ")

            if prefix == "quit":
                sys.exit(1)

            prefix_path = Path(prefix)
            if not (prefix_path / filename).exists():
                self.log.info(f"{prefix_path / filename} does not exist. Try again")
                continue

            self.log.info("%s was found. %s", prefix_path / filename, CAVEAT)
            self.settings.add_search_path(prefix_path)
            return prefix_path

    def _is_rpath(self, path: Path) -> bool:
        """Check if a path is an rpath."""
        return str(path).startswith("@rpath") or str(path).startswith("@loader_path")

    def _init_search_paths(self) -> None:
        """Initialize search paths from environment variables."""
        search_paths: List[Pathlike] = []

        for env_var in [
            "DYLD_LIBRARY_PATH",
            "DYLD_FALLBACK_FRAMEWORK_PATH",
            "DYLD_FALLBACK_LIBRARY_PATH",
        ]:
            if env_var in os.environ:
                paths = os.environ[env_var].split(":")
                search_paths.extend(Path(p) for p in paths)

        for path in search_paths:
            self.settings.add_search_path(path)

    def _change_install_name(self, binary_file: Path, old_name: Path | str, new_name: str) -> None:
        """Change the install name of a file."""
        command = f'install_name_tool -change "{old_name}" "{new_name}" "{binary_file}"'
        if subprocess.call(command, shell=True) != 0:
            self.log.error("An error occurred while trying to fix dependencies of %s", binary_file)
            sys.exit(1)

    def search_filename_in_rpaths(self, rpath_file: Path, dependent_file: Path) -> Path:
        """Search for a filename in rpaths."""
        fullpath = Path()
        suffix = re.sub(r"^@[a-z_]+path/", "", str(rpath_file))

        def _check_path(path: Path) -> bool:
            """Check if a path is valid."""
            file_prefix = dependent_file.parent
            if dependent_file != rpath_file:
                path_to_check = Path()
                if "@loader_path" in str(path):
                    path_to_check = Path(str(path).replace("@loader_path/", str(file_prefix)))
                elif "@rpath" in str(path):
                    path_to_check = Path(str(path).replace("@rpath/", str(file_prefix)))

                try:
                    fullpath = path_to_check.resolve()
                    self.parent.rpath_to_fullpath[rpath_file] = fullpath
                    return True
                except OSError:
                    return False
            return False

        if rpath_file in self.parent.rpath_to_fullpath:
            fullpath = self.parent.rpath_to_fullpath[rpath_file]
        elif not _check_path(rpath_file):
            for rpath in self.parent.rpaths_per_file[dependent_file]:
                if _check_path(rpath / suffix):
                    break

            if rpath_file in self.parent.rpath_to_fullpath:
                fullpath = self.parent.rpath_to_fullpath[rpath_file]

        if not fullpath:
            for search_path in self.settings.search_paths:
                if (search_path / suffix).exists():
                    fullpath = search_path / suffix
                    break

            if not fullpath:
                self.log.warning("can't get path for '%s'", rpath_file)
                fullpath = self._get_user_input_dir_for_file(suffix) / suffix
                fullpath = fullpath.resolve()

        return fullpath

    def get_original_filename(self) -> str:
        """Get the original filename."""
        return self.filename

    def get_original_path(self) -> Path:
        """Get the original path."""
        return self.prefix / self.filename

    def get_install_path(self) -> Path:
        """Get the install path."""
        return self.settings.dest_dir / self.new_name

    def get_inner_path(self) -> str:
        """Get the inner path."""
        return f"{self.settings.inside_lib_path}{self.new_name}"

    def add_symlink(self, symlink: Path) -> None:
        """Add a symlink."""
        if symlink not in self.symlinks:
            self.symlinks.append(symlink)

    def get_symlink_amount(self) -> int:
        """Get the number of symlinks."""
        return len(self.symlinks)

    def get_symlink(self, index: int) -> Path:
        """Get a symlink by index."""
        return self.symlinks[index]

    def copy_yourself(self) -> None:
        """Copy the file."""
        shutil.copy2(self.get_original_path(), self.get_install_path())

        # Fix the lib's inner name
        command = f'install_name_tool -id "{self.get_inner_path()}" "{self.get_install_path()}"'
        if subprocess.call(command, shell=True) != 0:
            self.log.error("An error occurred while trying to change identity of library %s",
                self.get_install_path())
            sys.exit(1)

    def fix_file_that_depends_on_me(self, file_to_fix: Path) -> None:
        """Fix dependencies in a file."""
        self._change_install_name(
            file_to_fix, self.get_original_path(), self.get_inner_path()
        )

        # Fix symlinks
        for symlink in self.symlinks:
            self._change_install_name(file_to_fix, symlink, self.get_inner_path())

    def merge_if_same_as(self, dep2: "Dependency") -> bool:
        """Compares this dependency with another. If both refer to the same file,
        returns true and merges both entries into one."""
        if dep2.get_original_filename() == self.filename:
            for i in range(self.get_symlink_amount()):
                dep2.add_symlink(self.get_symlink(i))
            return True
        return False

    def print(self) -> None:
        """Print the dependency."""
        self.log.info(f"{self.filename} from {self.prefix}")
        for sym in self.symlinks:
            self.log.info(f"    symlink --> {sym}")


class DylibBundler:
    """A DylibBundler instance."""

    def __init__(self, settings: Optional[Settings] = None):
        self.settings = settings or Settings()
        self.deps: List[Dependency] = []
        self.deps_per_file: Dict[Path, List[Dependency]] = {}
        self.deps_collected: Dict[Path, bool] = {}
        self.rpaths_per_file: Dict[Path, List[Path]] = {}
        self.rpath_to_fullpath: Dict[Path, Path] = {}
        self.log = logging.getLogger(self.__class__.__name__)

    def collect_dependencies(self, filename: Path) -> None:
        """Collect dependencies for a given file."""
        if filename in self.deps_collected:
            return

        self.collect_rpaths(filename)
        lines = self._collect_dependency_lines(filename)

        print(".", end="", flush=True)

        for line in lines:
            print(".", end="", flush=True)
            if not line.startswith("\t"):
                continue  # only lines beginning with a tab interest us
            if ".framework" in line:
                continue  # Ignore frameworks, we cannot handle them

            # trim useless info, keep only library name
            dep_path = line[1 : line.rfind(" (")]
            if self.settings.is_system_library(dep_path):
                continue

            self.add_dependency(dep_path, filename)

        self.deps_collected[filename] = True

    def _collect_dependency_lines(self, filename: Path) -> List[str]:
        """Execute otool -l and collect dependency lines."""
        if not filename.exists():
            self.log.error("Cannot find file %s to read its dependencies", filename)
            sys.exit(1)

        cmd = f'otool -l "{filename}"'
        try:
            output = subprocess.check_output(cmd, shell=True, text=True)
        except subprocess.CalledProcessError:
            self.log.error("Error running otool on %s", filename)
            sys.exit(1)

        lines = []
        raw_lines = output.split("\n")
        searching = False

        for line in raw_lines:
            if "cmd LC_LOAD_DYLIB" in line or "cmd LC_REEXPORT_DYLIB" in line:
                if searching:
                    self.log.error("Failed to find name before next cmd")
                    sys.exit(1)
                searching = True
            elif searching:
                found = line.find("name ")
                if found != -1:
                    lines.append("\t" + line[found + 5 :])
                    searching = False

        return lines

    def collect_rpaths(self, filename: Path) -> None:
        """Collect rpaths for a given file."""
        if not filename.exists():
            self.log.warning("can't collect rpaths for nonexistent file '%s'", filename)
            return

        cmd = f'otool -l "{filename}"'
        try:
            output = subprocess.check_output(cmd, shell=True, text=True)
        except subprocess.CalledProcessError:
            return

        lc_lines = output.split("\n")
        pos = 0
        read_rpath = False

        while pos < len(lc_lines):
            line = lc_lines[pos]
            pos += 1

            if read_rpath:
                start_pos = line.find("path ")
                end_pos = line.find(" (")
                if start_pos == -1 or end_pos == -1:
                    self.log.warning("Unexpected LC_RPATH format")
                    continue
                start_pos += 5
                rpath = Path(line[start_pos:end_pos])
                if filename not in self.rpaths_per_file:
                    self.rpaths_per_file[filename] = []
                self.rpaths_per_file[filename].append(rpath)
                read_rpath = False
                continue

            if "LC_RPATH" in line:
                read_rpath = True
                pos += 1

    def add_dependency(self, path: Pathlike, filename: Path) -> None:
        """Add a new dependency."""
        dep = Dependency(self, path, filename)

        # Check if this library was already added to avoid duplicates
        in_deps = False
        for existing_dep in self.deps:
            if dep.merge_if_same_as(existing_dep):
                in_deps = True
                break

        # Check if this library was already added to deps_per_file[filename]
        in_deps_per_file = False
        deps_in_file = self.deps_per_file.get(filename, [])
        for existing_dep in deps_in_file:
            if dep.merge_if_same_as(existing_dep):
                in_deps_per_file = True
                break

        if not self.settings.is_prefix_bundled(dep.prefix):
            return

        if not in_deps:
            self.deps.append(dep)
        if not in_deps_per_file:
            self.deps_per_file[filename] = self.deps_per_file.get(filename, []) + [dep]

    def collect_sub_dependencies(self) -> None:
        """Recursively collect each dependency's dependencies."""
        print(".", end="", flush=True)
        dep_amount = len(self.deps)

        while True:
            dep_amount = len(self.deps)
            for dep in self.deps[:dep_amount]:
                print(".", end="", flush=True)
                original_path = dep.get_original_path()
                if dep._is_rpath(original_path):
                    original_path = dep.search_filename_in_rpaths(
                        original_path, original_path
                    )

                self.collect_dependencies(original_path)

            if len(self.deps) == dep_amount:
                break  # no more dependencies were added on this iteration, stop searching

    def done_with_deps_go(self) -> None:
        """Process all collected dependencies."""
        print()
        for dep in self.deps:
            dep.print()
        print()

        if self.settings.bundle_libs_enabled:
            self.create_dest_dir()

            for dep in reversed(self.deps):
                self.log.info("Processing dependency %s", dep.get_install_path())
                dep.copy_yourself()
                self.change_lib_paths_on_file(dep.get_install_path())
                self.fix_rpaths_on_file(dep.get_original_path(), dep.get_install_path())
                self.adhoc_codesign(dep.get_install_path())

        for i in range(self.settings.file_to_fix_amount - 1, -1, -1):
            file_to_fix = self.settings.file_to_fix(i)
            self.log.info("Processing %s", file_to_fix)
            try:
                shutil.copy2(file_to_fix, file_to_fix)  # to set write permission
            except shutil.SameFileError:
                pass
            self.change_lib_paths_on_file(file_to_fix)
            self.fix_rpaths_on_file(file_to_fix, file_to_fix)
            self.adhoc_codesign(file_to_fix)

    def create_dest_dir(self) -> None:
        """Create the destination directory if needed."""
        dest_dir = self.settings.dest_dir
        self.log.info("Checking output directory %s", dest_dir)

        dest_exists = dest_dir.exists()

        if dest_exists and self.settings.can_overwrite_dir:
            self.log.info("Erasing old output directory %s", dest_dir)
            try:
                shutil.rmtree(dest_dir)
            except OSError:
                self.log.error("An error occurred while attempting to overwrite dest folder.")
                sys.exit(1)
            dest_exists = False

        if not dest_exists:
            if self.settings.can_create_dir:
                self.log.info("Creating output directory %s", dest_dir)
                try:
                    dest_dir.mkdir(parents=True)
                except OSError:
                    self.log.error("An error occurred while creating dest folder.")
                    sys.exit(1)
            else:
                self.log.error("Dest folder does not exist. Create it or pass the appropriate flag for automatic dest dir creation.")
                sys.exit(1)

    def change_lib_paths_on_file(self, file_to_fix: Path) -> None:
        """Change library paths in a file."""
        if file_to_fix not in self.deps_collected:
            print("    ", end="")
            self.collect_dependencies(file_to_fix)
            print()

        self.log.info("Fixing dependencies on %s", file_to_fix)
        deps_in_file = self.deps_per_file.get(file_to_fix, [])
        for dep in deps_in_file:
            dep.fix_file_that_depends_on_me(file_to_fix)

    def fix_rpaths_on_file(self, original_file: Path, file_to_fix: Path) -> None:
        """Fix rpaths in a file."""
        rpaths_to_fix = self.rpaths_per_file.get(original_file, [])

        for rpath in rpaths_to_fix:
            command = f'install_name_tool -rpath "{rpath}" "{self.settings.inside_lib_path}" "{file_to_fix}"'
            if subprocess.call(command, shell=True) != 0:
                self.log.error("An error occurred while trying to fix dependencies of %s", file_to_fix)

    def adhoc_codesign(self, file: Path) -> None:
        """Apply ad-hoc code signing to a file."""
        if not self.settings.can_codesign:
            return

        sign_command = f'codesign --force --deep --preserve-metadata=entitlements,requirements,flags,runtime --sign - "{file}"'
        if subprocess.call(sign_command, shell=True) != 0:
            self.log.error("An error occurred while applying ad-hoc signature to %s. Attempting workaround", file)

            try:
                machine = subprocess.check_output("machine", shell=True, text=True)
                is_arm = "arm" in machine
            except subprocess.CalledProcessError:
                is_arm = False

            temp_dir = Path(tempfile.gettempdir()) / "dylibbundler.XXXXXXXX"
            filename = file.name
            try:
                temp_dir = Path(tempfile.mkdtemp(prefix="dylibbundler."))
                temp_file = temp_dir / filename

                # Copy file to temp location
                shutil.copy2(file, temp_file)
                # Move it back
                shutil.move(temp_file, file)
                # Remove temp dir
                shutil.rmtree(temp_dir)
                # Try signing again
                if subprocess.call(sign_command, shell=True) != 0:
                    self.log.error("An error occurred while applying ad-hoc signature to %s", file)
                    if is_arm:
                        sys.exit(1)
            except Exception as e:
                self.log.error(" %s", str(e))
                if is_arm:
                    sys.exit(1)

    @classmethod
    def commandline(cls) -> None:
        """Command line interface for DylibBundler."""
        settings = Settings()

        parser = argparse.ArgumentParser(
            prog='bundler',
            description='bundler is a utility that helps bundle dynamic libraries inside macOS app bundles.',
            epilog="e.g: bundler -od -b -d My.app/Contents/libs/ My.app/Contents/MacOS/main",
        )

        arg = opt = parser.add_argument

        arg("target", nargs="+", help="file to fix (executable or app plug-in)")
        opt("-b", "--bundle-deps", help="bundle dependencies", action="store_true")
        opt("-d", "--dest-dir", help="directory to send bundled libraries (relative to cwd)")
        opt("-p", "--install-path", default="@executable_path/../libs/", help="'inner' path of bundled libraries (usually relative to executable")
        opt("-s", "--search-path", help="directory to add to list of locations searched")
        opt("-of", "--overwrite-files", help="allow overwriting files in output directory", action="store_true")
        opt("-od", "--overwrite-dir", help="totally overwrite output directory if it already exists. implies --create-dir", action="store_true")
        opt("-cd", "--create-dir", help="creates output directory if necessary", action="store_true")
        opt("-ns", "--no-codesign", help="disables ad-hoc codesigning", action="store_true")
        opt("-i", "--ignore", help="will ignore libraries in this directory")

        args = parser.parse_args()
        for target in args.target:
            settings.add_file_to_fix(target)
        if args.bundle_deps:
            settings.bundle_libs = True
        if args.install_path:
            settings.inside_lib_path = args.install_path
        if args.ignore:
            settings.ignore_prefix(args.ignore)
        if args.dest_dir:
            settings.dest_dir = Path(args.dest_dir)
        if args.overwrite_files:
            settings.overwrite_files = True
        if args.overwrite_dir:
            settings.overwrite_dir = True
            settings.create_dir = True
        if args.create_dir:
            settings.create_dir = True
        if args.no_codesign:
            settings.codesign = False
        if args.search_path:
            settings.add_search_path(args.search_path)

        if not settings.bundle_libs_enabled and (settings.file_to_fix_amount < 1):
            parser.print_help()
            sys.exit(0)

        # create instance
        bundler = cls(settings)

        bundler.log.info("Collecting dependencies")

        # Collect dependencies
        for i in range(bundler.settings.file_to_fix_amount):
            bundler.collect_dependencies(bundler.settings.file_to_fix(i))

        bundler.collect_sub_dependencies()
        bundler.done_with_deps_go()

if __name__ == "__main__":
    DylibBundler.commandline()
