#!/usr/bin/env python3
"""bundler is a utility that helps bundle dynamic libraries inside macOS app bundles.

It is a python3 translation of the c++ macdylibbundler utility by Marianne Gagnon 
which can be found at https://github.com/auriamg/macdylibbundler

usage: bundler [-h] [-d DEST_DIR] [-p INSTALL_PATH] [-s SEARCH_PATH] [-od]
               [-cd] [-ns] [-i IGNORE] [-dm] [-nc]
               target [target ...]

bundler is a utility that helps bundle dynamic libraries inside macOS app
bundles.

positional arguments:
  target                file to fix (executable or app plug-in)

options:
  -h, --help            show this help message and exit
  -d, --dest-dir DEST_DIR
                        directory to send bundled libraries (relative to cwd)
                        (default: ./libs/)
  -p, --install-path INSTALL_PATH
                        'inner' path of bundled libraries (usually relative to
                        executable (default: @executable_path/../libs/)
  -s, --search-path SEARCH_PATH
                        directory to add to list of locations searched
                        (default: None)
  -od, --overwrite-dir  overwrite output directory if it already exists.
                        implies --create-dir (default: False)
  -cd, --create-dir     creates output directory if necessary (default: False)
  -ns, --no-codesign    disables ad-hoc codesigning (default: True)
  -i, --ignore IGNORE   will ignore libraries in this directory (default: None)
  -dm, --debug-mode     enable debug mode (default: False)
  -nc, --no-color       disable color in logging (default: False)

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
from typing import Optional, List, Dict, NoReturn

CAVEAT = ("MAY NOT CORRECTLY HANDLE THIS DEPENDENCY: "
          "Manually check the executable with 'otool -L'")

# type aliases
Pathlike = str | Path

# ----------------------------------------------------------------------------
# error handling

class BundlerError(Exception):
    """Base exception class for bundler errors."""
    pass

class CommandError(BundlerError):
    """Exception raised when a command fails."""
    def __init__(self, command: str, returncode: int, output: Optional[str] = None):
        self.command = command
        self.returncode = returncode
        self.output = output
        super().__init__(f"Command '{command}' failed with return code {returncode}")

class FileError(BundlerError):
    """Exception raised when a file operation fails."""
    pass

class ConfigurationError(BundlerError):
    """Exception raised when configuration is invalid."""
    pass

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

    def __init__(self, use_color: bool = True):
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

def setup_logging(debug: bool = True, use_color: bool = True) -> None:
    """Configure logging for the application.
    
    Args:
        debug: Whether to enable debug logging
        use_color: Whether to use colored output
    """
    stream_handler = logging.StreamHandler()
    stream_handler.setFormatter(CustomFormatter(use_color))
    logging.basicConfig(
        level=logging.DEBUG if debug else logging.INFO,
        handlers=[stream_handler],
    )

# ----------------------------------------------------------------------------
# classes

class Dependency:
    """A dependency of a file."""

    def __init__(self, parent: "DylibBundler", path: Pathlike, dependent_file: Pathlike):
        """Initialize a new dependency.
        
        Args:
            parent: The parent DylibBundler instance
            path: The path to the dependency
            dependent_file: The file that depends on this dependency
            
        Raises:
            FileError: If the dependency cannot be resolved
            ConfigurationError: If the dependency configuration is invalid
        """
        self.parent = parent
        self.filename = ""
        self.prefix = Path()
        self.symlinks: List[Path] = []
        self.new_name = ""
        self.log = logging.getLogger(self.__class__.__name__)

        # Resolve the original file path
        path = Path(str(path).strip())
        dependent_file = Path(dependent_file)

        try:
            if self._is_rpath(path):
                original_file = self.search_filename_in_rpaths(path, dependent_file)
            else:
                try:
                    original_file = path.resolve()
                except OSError as e:
                    raise FileError(f"Cannot resolve path '{path}': {e}")

            # Check if given path is a symlink
            if original_file != path:
                self.add_symlink(path)

            self.filename = original_file.name
            self.prefix = original_file.parent

            # Check if this dependency should be bundled
            if not self.parent.is_bundled_prefix(self.prefix):
                return

            # Check if the lib is in a known location
            if not self.prefix or not (self.prefix / self.filename).exists():
                if not self.parent.search_paths:
                    self._init_search_paths()

                # Check if file is contained in one of the paths
                for search_path in self.parent.search_paths:
                    if (search_path / self.filename).exists():
                        self.log.info(f"FOUND {self.filename} in {search_path}")
                        self.prefix = search_path
                        break

            # If location still unknown, ask user for search path
            if not self.parent.is_ignored_prefix(self.prefix) and (
                not self.prefix or not (self.prefix / self.filename).exists()
            ):
                self.log.warning("Library %s has an incomplete name (location unknown)",
                               self.filename)
                self.parent.add_search_path(
                    self._get_user_input_dir_for_file(self.filename))

            self.new_name = self.filename

        except Exception as e:
            raise FileError(f"Failed to initialize dependency for {path}: {e}")

    def _get_user_input_dir_for_file(self, filename: str) -> Path:
        """Get a user input directory for a file.
        
        Args:
            filename: The name of the file to find
            
        Returns:
            The directory containing the file
            
        Raises:
            ConfigurationError: If no valid directory is provided
        """
        for search_path in self.parent.search_paths:
            if (search_path / filename).exists():
                self.log.info("%s was found. %s", search_path / filename, CAVEAT)
                return search_path

        while True:
            # sys.stdout.flush()
            prefix = input("Please specify the directory where this library is "
                         "located (or enter 'quit' to abort): ")

            if prefix == "quit":
                raise ConfigurationError("User aborted dependency resolution")

            prefix_path = Path(prefix)
            if not (prefix_path / filename).exists():
                self.log.info(f"{prefix_path / filename} does not exist. Try again")
                continue

            self.log.info("%s was found. %s", prefix_path / filename, CAVEAT)
            self.parent.add_search_path(prefix_path)
            return prefix_path

    def _is_rpath(self, path: Path) -> bool:
        """Check if a path is an rpath.
        
        Args:
            path: The path to check
            
        Returns:
            True if the path is an rpath, False otherwise
        """
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
            self.parent.add_search_path(path)

    def _change_install_name(self, binary_file: Path, old_name: Pathlike, new_name: str) -> None:
        """Change the install name of a file.
        
        Args:
            binary_file: The file to modify
            old_name: The old install name
            new_name: The new install name
            
        Raises:
            CommandError: If the install_name_tool command fails
        """
        command = f'install_name_tool -change "{old_name}" "{new_name}" "{binary_file}"'
        try:
            self.parent.run_command(command)
        except CommandError as e:
            raise CommandError(
                f"Failed to change install name for {binary_file}: {e}",
                e.returncode,
                e.output
            )

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
            for search_path in self.parent.search_paths:
                if (search_path / suffix).exists():
                    fullpath = search_path / suffix
                    break

            if not fullpath:
                self.log.warning("can't get path for '%s'", rpath_file)
                fullpath = self._get_user_input_dir_for_file(suffix) / suffix
                fullpath = fullpath.resolve()

        return fullpath

    def get_original_path(self) -> Path:
        """Get the original path."""
        return self.prefix / self.filename

    def get_install_path(self) -> Path:
        """Get the install path."""
        return self.parent.dest_dir / self.new_name

    def get_inner_path(self) -> str:
        """Get the inner path."""
        return f"{self.parent.inside_lib_path}{self.new_name}"

    def add_symlink(self, symlink: Path) -> None:
        """Add a symlink."""
        if symlink not in self.symlinks:
            self.symlinks.append(symlink)

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

    def merge_if_same_as(self, other: "Dependency") -> bool:
        """Compares this dependency with another. If both refer to the same file,
        returns true and merges both entries into one."""
        if other.filename == self.filename:
            for symlink in self.symlinks:
                other.add_symlink(symlink)
            return True
        return False

    def print(self) -> None:
        """Print the dependency."""
        lines = [f"{self.filename} from {self.prefix}"]
        for sym in self.symlinks:
            lines.append(f"    symlink --> {sym}")
        self.log.info("\n".join(lines))


class DylibBundler:
    """A DylibBundler instance."""

    def __init__(
        self,
        dest_dir: Pathlike = Path("./libs/"),
        overwrite_dir: bool = False,
        create_dir: bool = False,
        codesign: bool = True,
        inside_lib_path: str = "@executable_path/../libs/",
        files_to_fix: Optional[List[Pathlike]] = None,
        prefixes_to_ignore: Optional[List[Pathlike]] = None,
        search_paths: Optional[List[Pathlike]] = None,
    ):
        """Initialize a new DylibBundler instance.
        
        Args:
            dest_dir: Directory to send bundled libraries
            overwrite_dir: Whether to overwrite existing output directory
            create_dir: Whether to create output directory if needed
            codesign: Whether to apply ad-hoc codesigning
            inside_lib_path: Inner path of bundled libraries
            files_to_fix: List of files to process
            prefixes_to_ignore: List of prefixes to ignore
            search_paths: List of search paths
            
        Raises:
            ConfigurationError: If configuration is invalid
        """
        try:
            self.dest_dir = Path(dest_dir)
            self.can_overwrite_dir = overwrite_dir
            self.can_create_dir = create_dir
            self.can_codesign = codesign
            self.inside_lib_path = inside_lib_path
            self.files_to_fix = [Path(f) for f in (files_to_fix or [])]
            self.prefixes_to_ignore = [Path(p) for p in (prefixes_to_ignore or [])]
            self.search_paths = [Path(p) for p in (search_paths or [])]

            self.deps: List[Dependency] = []
            self.deps_per_file: Dict[Path, List[Dependency]] = {}
            self.deps_collected: Dict[Path, bool] = {}
            self.rpaths_per_file: Dict[Path, List[Path]] = {}
            self.rpath_to_fullpath: Dict[Path, Path] = {}
            self.log = logging.getLogger(self.__class__.__name__)

            # Validate configuration
            if not self.files_to_fix:
                raise ConfigurationError("No files to fix specified")
            if not self.dest_dir and not self.can_create_dir:
                raise ConfigurationError("Destination directory not specified and create_dir is False")

        except Exception as e:
            raise ConfigurationError(f"Failed to initialize DylibBundler: {e}")

    def add_search_path(self, path: Pathlike) -> None:
        """Add a search path."""
        self.search_paths.append(Path(path))

    def search_path(self, index: int) -> Path:
        """Get a search path by index."""
        return self.search_paths[index]

    def add_file_to_fix(self, path: Pathlike) -> None:
        """Add a file to fix."""
        self.files_to_fix.append(Path(path))

    def ignore_prefix(self, prefix: Pathlike) -> None:
        """Ignore a prefix."""
        self.prefixes_to_ignore.append(Path(prefix))

    def is_system_library(self, prefix: Pathlike) -> bool:
        """Check if a prefix is a system library."""
        prefix = str(prefix)
        return prefix.startswith("/usr/lib/") or prefix.startswith("/System/Library/")

    def is_ignored_prefix(self, prefix: Pathlike) -> bool:
        """Check if a prefix is ignored."""
        return Path(prefix) in self.prefixes_to_ignore

    def is_bundled_prefix(self, prefix: Pathlike) -> bool:
        """Check if a prefix is bundled."""
        prefix = str(prefix)
        if ".framework" in prefix:
            return False
        if "@executable_path" in prefix:
            return False
        if self.is_system_library(prefix):
            return False
        if self.is_ignored_prefix(prefix):
            return False
        return True

    def run_command(self, command: str, shell: bool = True) -> str:
        """Run a shell command and return its output.
        
        Args:
            command: The command to run
            shell: Whether to run in a shell
            
        Returns:
            The command output
            
        Raises:
            CommandError: If the command fails
        """
        self.log.debug("%s", command)
        try:
            result = subprocess.run(
                command,
                shell=shell,
                check=True,
                text=True,
                capture_output=True
            )
            return result.stdout
        except subprocess.CalledProcessError as e:
            raise CommandError(command, e.returncode, e.output)

    def chmod(self, path, perm=0o777):
        """Change permission of file"""
        self.log.info("change permission of %s to %s", path, perm)
        os.chmod(path, perm)

    def collect_dependencies(self, filename: Path) -> None:
        """Collect dependencies for a given file."""
        if filename in self.deps_collected:
            return

        self.collect_rpaths(filename)
        lines = self._collect_dependency_lines(filename)

        for line in lines:
            if not line.startswith("\t"):
                continue  # only lines beginning with a tab interest us
            if ".framework" in line:
                continue  # Ignore frameworks, we cannot handle them

            # trim useless info, keep only library name
            dep_path = line[1 : line.rfind(" (")]
            if self.is_system_library(dep_path):
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

        if not self.is_bundled_prefix(dep.prefix):
            return

        if not in_deps:
            self.deps.append(dep)
        if not in_deps_per_file:
            self.deps_per_file[filename] = self.deps_per_file.get(filename, []) + [dep]

    def collect_sub_dependencies(self) -> None:
        """Recursively collect each dependency's dependencies."""
        n_deps = len(self.deps)

        while True:
            n_deps = len(self.deps)
            for dep in self.deps[:n_deps]:
                original_path = dep.get_original_path()
                if dep._is_rpath(original_path):
                    original_path = dep.search_filename_in_rpaths(
                        original_path, original_path
                    )

                self.collect_dependencies(original_path)

            if len(self.deps) == n_deps:
                break  # no more dependencies were added on this iteration, stop searching

    def process_collected_deps(self) -> None:
        """Process all collected dependencies."""
        for dep in self.deps:
            dep.print()

        self.create_dest_dir()

        for dep in reversed(self.deps):
            self.log.info("Processing dependency %s", dep.get_install_path())
            dep.copy_yourself()
            self.change_lib_paths_on_file(dep.get_install_path())
            self.fix_rpaths_on_file(dep.get_original_path(), dep.get_install_path())
            self.adhoc_codesign(dep.get_install_path())

        for file in reversed(self.files_to_fix):
            self.log.info("Processing %s", file)
            # try:
            #     shutil.copy2(file, file)  # to set write permission
            # except shutil.SameFileError:
            #     pass
            self.change_lib_paths_on_file(file)
            self.fix_rpaths_on_file(file, file)
            self.adhoc_codesign(file)

    def create_dest_dir(self) -> None:
        """Create the destination directory if needed.
        
        Raises:
            FileError: if directory creation fails
        """
        dest_dir = self.dest_dir
        self.log.info("Checking output directory %s", dest_dir)

        dest_exists = dest_dir.exists()

        if dest_exists and self.can_overwrite_dir:
            self.log.info("Erasing old output directory %s", dest_dir)
            try:
                shutil.rmtree(dest_dir)
            except OSError as e:
                raise FileError(f"Failed to overwrite destination directory: {e}")
            dest_exists = False

        if not dest_exists:
            if self.can_create_dir:
                self.log.info("Creating output directory %s", dest_dir)
                try:
                    dest_dir.mkdir(parents=True)
                except OSError as e:
                    raise FileError(f"Failed to create destination directory: {e}")
            else:
                raise FileError("Destination directory does not exist and create_dir is False")

    def change_lib_paths_on_file(self, file_to_fix: Path) -> None:
        """Change library paths in a file."""
        if file_to_fix not in self.deps_collected:
            self.collect_dependencies(file_to_fix)

        self.log.info("Fixing dependencies on %s", file_to_fix)
        deps_in_file = self.deps_per_file.get(file_to_fix, [])
        for dep in deps_in_file:
            dep.fix_file_that_depends_on_me(file_to_fix)

    def fix_rpaths_on_file(self, original_file: Path, file_to_fix: Path) -> None:
        """Fix rpaths in a file."""
        rpaths_to_fix = self.rpaths_per_file.get(original_file, [])

        for rpath in rpaths_to_fix:
            command = f'install_name_tool -rpath "{rpath}" "{self.inside_lib_path}" "{file_to_fix}"'
            if subprocess.call(command, shell=True) != 0:
                self.log.error("An error occurred while trying to fix dependencies of %s", file_to_fix)

    def adhoc_codesign(self, file: Path) -> None:
        """Apply ad-hoc code signing to a file.
        
        Args:
            file: The file to sign
            
        Raises:
            CommandError: If codesigning fails
        """
        if not self.can_codesign:
            return

        self.log.info("codesign %s", file)
        sign_command = f'codesign --force --deep --preserve-metadata=entitlements,requirements,flags,runtime --sign - "{file}"'
        
        try:
            self.run_command(sign_command)
        except CommandError as e:
            self.log.error("An error occurred while applying ad-hoc signature to %s. Attempting workaround", file)

            try:
                machine = self.run_command("machine")
                is_arm = "arm" in machine
            except CommandError:
                is_arm = False

            try:
                temp_dir = Path(tempfile.mkdtemp(prefix="dylibbundler."))
                temp_file = temp_dir / file.name

                # Copy file to temp location
                shutil.copy2(file, temp_file)
                # Move it back
                shutil.move(temp_file, file)
                # Remove temp dir
                shutil.rmtree(temp_dir)
                # Try signing again
                try:
                    self.run_command(sign_command)
                except CommandError as e:
                    if is_arm:
                        raise CommandError(f"Failed to sign {file} on ARM: {e}", e.returncode, e.output)
                    self.log.error("An error occurred while applying ad-hoc signature to %s", file)
            except Exception as e:
                if is_arm:
                    raise CommandError(f"Failed to sign {file} on ARM: {e}", 1)
                self.log.error(" %s", str(e))

    @classmethod
    def commandline(cls) -> None:
        """Command line interface for DylibBundler."""
        try:
            parser = argparse.ArgumentParser(
                prog='bundler',
                description='bundler is a utility that helps bundle dynamic libraries inside macOS app bundles.',
                epilog="e.g: bundler -od -b -d My.app/Contents/libs/ My.app/Contents/MacOS/main",
                formatter_class=argparse.ArgumentDefaultsHelpFormatter,
            )

            arg = opt = parser.add_argument

            arg("target", nargs="+", help="file to fix (executable or app plug-in)")
            opt("-d",  "--dest-dir", default="./libs/", help="directory to send bundled libraries (relative to cwd)")
            opt("-p",  "--install-path", default="@executable_path/../libs/", help="'inner' path of bundled libraries (usually relative to executable")
            opt("-s",  "--search-path", help="directory to add to list of locations searched")
            opt("-od", "--overwrite-dir", help="overwrite output directory if it already exists. implies --create-dir", action="store_true")
            opt("-cd", "--create-dir", help="creates output directory if necessary", action="store_true")
            opt("-ns", "--no-codesign", help="disables ad-hoc codesigning", action="store_false")
            opt("-i",  "--ignore", help="will ignore libraries in this directory")
            opt("-dm", "--debug-mode", help="enable debug mode", action="store_true")
            opt("-nc", "--no-color", help="disable color in logging", action="store_true")

            args = parser.parse_args()
            
            # Setup logging
            setup_logging(args.debug_mode, not args.no_color)

            bundler = cls(
                dest_dir = Path(args.dest_dir),
                overwrite_dir = args.overwrite_dir,
                create_dir = args.create_dir or args.overwrite_dir,
                codesign = args.no_codesign,
                inside_lib_path = args.install_path,
                files_to_fix = [Path(f) for f in args.target],
                prefixes_to_ignore = [Path(args.ignore)] if args.ignore else [],
                search_paths = [Path(args.search_path)] if args.search_path else [],
            )

            bundler.log.info("Collecting dependencies")

            # Collect dependencies
            for file in bundler.files_to_fix:
                bundler.collect_dependencies(file)

            bundler.collect_sub_dependencies()
            bundler.process_collected_deps()

        except BundlerError as e:
            logging.error(str(e))
            sys.exit(1)
        except Exception as e:
            logging.error(f"Unexpected error: {e}")
            sys.exit(1)

if __name__ == "__main__":
    DylibBundler.commandline()
