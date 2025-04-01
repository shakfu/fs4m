// https://github.com/auriamg/macdylibbundler
/*
The MIT License (MIT)

Copyright (c) 2014 Marianne Gagnon

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
 */

// usage: g++ -O3 -o bundler bundler.cpp

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <locale>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

using namespace std;

#ifdef __linux
#include <linux/limits.h>
#endif


class Library;

void tokenize(const std::string& str, const char* delimiters,
              std::vector<std::string>*);
bool fileExists(const std::string& filename);

void copyFile(const std::string& from, const std::string& to);

// executes a command in the native shell and returns output in string
std::string system_get_output(const std::string& cmd);

// like 'system', runs a command on the system shell, but also prints the
// command to stdout.
int systemp(const std::string& cmd);
void changeInstallName(const std::string& binary_file,
                       const std::string& old_name,
                       const std::string& new_name);
std::string getUserInputDirForFile(const std::string& filename);

// sign `file` with an ad-hoc code signature: required for ARM (Apple Silicon)
// binaries
void adhocCodeSign(const std::string& file);

void collectDependencies(const std::string& filename);
void collectSubDependencies();
void doneWithDeps_go();
bool isRpath(const std::string& path);
std::string searchFilenameInRpaths(const std::string& rpath_file,
                                   const std::string& dependent_file);
std::string searchFilenameInRpaths(const std::string& rpath_dep);


namespace Settings {

bool isSystemLibrary(const std::string& prefix);
bool isPrefixBundled(const std::string& prefix);
bool isPrefixIgnored(const std::string& prefix);
void ignore_prefix(std::string prefix);

bool canOverwriteFiles();
void canOverwriteFiles(bool permission);

bool canOverwriteDir();
void canOverwriteDir(bool permission);

bool canCreateDir();
void canCreateDir(bool permission);

bool canCodesign();
void canCodesign(bool permission);

bool bundleLibs();
void bundleLibs(bool on);

std::string destFolder();
void destFolder(const std::string& path);

void addFileToFix(const std::string& path);
int fileToFixAmount();
std::string fileToFix(const int n);

std::string inside_lib_path();
void inside_lib_path(const std::string& p);

void addSearchPath(const std::string& path);
int searchPathAmount();
std::string searchPath(const int n);

}


class Dependency {
    // origin
    std::string filename;
    std::string prefix;
    std::vector<std::string> symlinks;

    // installation
    std::string new_name;

public:
    Dependency(std::string path, const std::string& dependent_file);

    void print();

    std::string getOriginalFileName() const { return filename; }
    std::string getOriginalPath() const { return prefix + filename; }
    std::string getInstallPath();
    std::string getInnerPath();

    void addSymlink(const std::string& s);
    int getSymlinkAmount() const { return symlinks.size(); }

    std::string getSymlink(const int i) const { return symlinks[i]; }
    std::string getPrefix() const { return prefix; }

    void copyYourself();
    void fixFileThatDependsOnMe(const std::string& file);

    // Compares the given dependency with this one. If both refer to the same
    // file, it returns true and merges both entries into one.
    bool mergeIfSameAs(Dependency& dep2);
};


std::string stripPrefix(std::string in)
{
    return in.substr(in.rfind("/") + 1);
}

std::string& rtrim(std::string& s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         [](unsigned char c) { return !std::isspace(c); })
                .base(),
            s.end());
    return s;
}

// initialize the dylib search paths
void initSearchPaths()
{
    // Check the same paths the system would search for dylibs
    std::string searchPaths;
    char* dyldLibPath = std::getenv("DYLD_LIBRARY_PATH");
    if (dyldLibPath != nullptr)
        searchPaths = dyldLibPath;
    dyldLibPath = std::getenv("DYLD_FALLBACK_FRAMEWORK_PATH");
    if (dyldLibPath != nullptr) {
        if (!searchPaths.empty() && searchPaths[searchPaths.size() - 1] != ':')
            searchPaths += ":";
        searchPaths += dyldLibPath;
    }
    dyldLibPath = std::getenv("DYLD_FALLBACK_LIBRARY_PATH");
    if (dyldLibPath != nullptr) {
        if (!searchPaths.empty() && searchPaths[searchPaths.size() - 1] != ':')
            searchPaths += ":";
        searchPaths += dyldLibPath;
    }
    if (!searchPaths.empty()) {
        std::stringstream ss(searchPaths);
        std::string item;
        while (std::getline(ss, item, ':')) {
            if (item[item.size() - 1] != '/')
                item += "/";
            Settings::addSearchPath(item);
        }
    }
}

// if some libs are missing prefixes, this will be set to true
// more stuff will then be necessary to do
bool missing_prefixes = false;

Dependency::Dependency(std::string path, const std::string& dependent_file)
{
    char original_file_buffer[PATH_MAX];
    std::string original_file;

    rtrim(path);
    if (isRpath(path)) {
        original_file = searchFilenameInRpaths(path, dependent_file);
    } else if (realpath(rtrim(path).c_str(), original_file_buffer)) {
        original_file = original_file_buffer;
    } else {
        std::cerr << "\n/!\\ WARNING : Cannot resolve path '" << path.c_str()
                  << "'" << std::endl;
        original_file = path;
    }

    // check if given path is a symlink
    if (original_file != path)
        addSymlink(path);

    filename = stripPrefix(original_file);
    prefix = original_file.substr(0, original_file.rfind("/") + 1);

    if (!prefix.empty() && prefix[prefix.size() - 1] != '/')
        prefix += "/";

    // check if this dependency is in /usr/lib, /System/Library, or in ignored
    // list
    if (!Settings::isPrefixBundled(prefix))
        return;

    // check if the lib is in a known location
    if (prefix.empty() || !fileExists(prefix + filename)) {
        // the paths contains at least /usr/lib so if it is empty we have not
        // initialized it
        int searchPathAmount = Settings::searchPathAmount();
        if (searchPathAmount == 0) {
            initSearchPaths();
            searchPathAmount = Settings::searchPathAmount();
        }

        // check if file is contained in one of the paths
        for (int i = 0; i < searchPathAmount; ++i) {
            std::string search_path = Settings::searchPath(i);
            if (fileExists(search_path + filename)) {
                std::cout << "FOUND " << filename << " in " << search_path
                          << std::endl;
                prefix = search_path;
                missing_prefixes = true; // the prefix was missing
                break;
            }
        }
    }

    // If the location is still unknown, ask the user for search path
    if (!Settings::isPrefixIgnored(prefix)
        && (prefix.empty() || !fileExists(prefix + filename))) {
        std::cerr << "\n/!\\ WARNING : Library " << filename
                  << " has an incomplete name (location unknown)" << std::endl;
        missing_prefixes = true;

        Settings::addSearchPath(getUserInputDirForFile(filename));
    }

    new_name = filename;
}

void Dependency::print()
{
    std::cout << std::endl;
    std::cout << " * " << filename.c_str() << " from " << prefix.c_str()
              << std::endl;

    const int symamount = symlinks.size();
    for (int n = 0; n < symamount; n++)
        std::cout << "     symlink --> " << symlinks[n].c_str() << std::endl;
    ;
}

std::string Dependency::getInstallPath()
{
    return Settings::destFolder() + new_name;
}
std::string Dependency::getInnerPath()
{
    return Settings::inside_lib_path() + new_name;
}


void Dependency::addSymlink(const std::string& s)
{
    // calling std::find on this vector is not near as slow as an extra
    // invocation of install_name_tool
    if (std::find(symlinks.begin(), symlinks.end(), s) == symlinks.end())
        symlinks.push_back(s);
}

// Compares the given Dependency with this one. If both refer to the same file,
// it returns true and merges both entries into one.
bool Dependency::mergeIfSameAs(Dependency& dep2)
{
    if (dep2.getOriginalFileName().compare(filename) == 0) {
        const int samount = getSymlinkAmount();
        for (int n = 0; n < samount; n++) {
            dep2.addSymlink(getSymlink(n));
        }
        return true;
    }
    return false;
}

void Dependency::copyYourself()
{
    copyFile(getOriginalPath(), getInstallPath());

    // Fix the lib's inner name
    std::string command = std::string("install_name_tool -id \"")
        + getInnerPath() + "\" \"" + getInstallPath() + "\"";
    if (systemp(command) != 0) {
        std::cerr << "\n\nError : An error occured while trying to change "
                     "identity of library "
                  << getInstallPath() << std::endl;
        exit(1);
    }
}

void Dependency::fixFileThatDependsOnMe(const std::string& file_to_fix)
{
    // for main lib file
    changeInstallName(file_to_fix, getOriginalPath(), getInnerPath());
    // for symlinks
    const int symamount = symlinks.size();
    for (int n = 0; n < symamount; n++) {
        changeInstallName(file_to_fix, symlinks[n], getInnerPath());
    }

    // FIXME - hackish
    if (missing_prefixes) {
        // for main lib file
        changeInstallName(file_to_fix, filename, getInnerPath());
        // for symlinks
        const int symamount = symlinks.size();
        for (int n = 0; n < symamount; n++) {
            changeInstallName(file_to_fix, symlinks[n], getInnerPath());
        }
    }
}


std::vector<Dependency> deps;
std::map<std::string, std::vector<Dependency>> deps_per_file;
std::map<std::string, bool> deps_collected;
std::map<std::string, std::vector<std::string>> rpaths_per_file;
std::map<std::string, std::string> rpath_to_fullpath;

void changeLibPathsOnFile(std::string file_to_fix)
{
    if (deps_collected.find(file_to_fix) == deps_collected.end()) {
        std::cout << "    ";
        collectDependencies(file_to_fix);
        std::cout << "\n";
    }
    std::cout << "  * Fixing dependencies on " << file_to_fix.c_str()
              << std::endl;

    std::vector<Dependency> deps_in_file = deps_per_file[file_to_fix];
    const int dep_amount = deps_in_file.size();
    for (int n = 0; n < dep_amount; n++) {
        deps_in_file[n].fixFileThatDependsOnMe(file_to_fix);
    }
}

bool isRpath(const std::string& path)
{
    return path.find("@rpath") == 0 || path.find("@loader_path") == 0;
}

void collectRpaths(const std::string& filename)
{
    if (!fileExists(filename)) {
        std::cerr
            << "\n/!\\ WARNING : can't collect rpaths for nonexistent file '"
            << filename << "'\n";
        return;
    }

    std::string cmd = "otool -l \"" + filename + "\"";
    std::string output = system_get_output(cmd);

    std::vector<std::string> lc_lines;
    tokenize(output, "\n", &lc_lines);

    size_t pos = 0;
    bool read_rpath = false;
    while (pos < lc_lines.size()) {
        std::string line = lc_lines[pos];
        pos++;

        if (read_rpath) {
            size_t start_pos = line.find("path ");
            size_t end_pos = line.find(" (");
            if (start_pos == std::string::npos
                || end_pos == std::string::npos) {
                std::cerr << "\n/!\\ WARNING: Unexpected LC_RPATH format\n";
                continue;
            }
            start_pos += 5;
            std::string rpath = line.substr(start_pos, end_pos - start_pos);
            rpaths_per_file[filename].push_back(rpath);
            read_rpath = false;
            continue;
        }

        if (line.find("LC_RPATH") != std::string::npos) {
            read_rpath = true;
            pos++;
        }
    }
}

std::string searchFilenameInRpaths(const std::string& rpath_file,
                                   const std::string& dependent_file)
{
    char buffer[PATH_MAX];
    std::string fullpath;
    std::string suffix = std::regex_replace(rpath_file,
                                            std::regex("^@[a-z_]+path/"), "");

    const auto check_path = [&](std::string path) {
        char buffer[PATH_MAX];
        std::string file_prefix = dependent_file.substr(
            0, dependent_file.rfind('/') + 1);
        if (dependent_file != rpath_file) {
            std::string path_to_check;
            if (path.find("@loader_path") != std::string::npos) {
                path_to_check = std::regex_replace(
                    path, std::regex("@loader_path/"), file_prefix);
            } else if (path.find("@rpath") != std::string::npos) {
                path_to_check = std::regex_replace(path, std::regex("@rpath/"),
                                                   file_prefix);
            }
            if (realpath(path_to_check.c_str(), buffer)) {
                fullpath = buffer;
                rpath_to_fullpath[rpath_file] = fullpath;
                return true;
            }
        }
        return false;
    };

    // fullpath previously stored
    if (rpath_to_fullpath.find(rpath_file) != rpath_to_fullpath.end()) {
        fullpath = rpath_to_fullpath[rpath_file];
    } else if (!check_path(rpath_file)) {
        for (auto rpath : rpaths_per_file[dependent_file]) {
            if (rpath[rpath.size() - 1] != '/')
                rpath += "/";
            if (check_path(rpath + suffix))
                break;
        }
        if (rpath_to_fullpath.find(rpath_file) != rpath_to_fullpath.end()) {
            fullpath = rpath_to_fullpath[rpath_file];
        }
    }

    if (fullpath.empty()) {
        const int searchPathAmount = Settings::searchPathAmount();
        for (int n = 0; n < searchPathAmount; n++) {
            std::string search_path = Settings::searchPath(n);
            if (fileExists(search_path + suffix)) {
                fullpath = search_path + suffix;
                break;
            }
        }

        if (fullpath.empty()) {
            std::cerr << "\n/!\\ WARNING : can't get path for '" << rpath_file
                      << "'\n";
            fullpath = getUserInputDirForFile(suffix) + suffix;
            if (realpath(fullpath.c_str(), buffer)) {
                fullpath = buffer;
            }
        }
    }

    return fullpath;
}

std::string searchFilenameInRpaths(const std::string& rpath_dep)
{
    return searchFilenameInRpaths(rpath_dep, rpath_dep);
}

void fixRpathsOnFile(const std::string& original_file,
                     const std::string& file_to_fix)
{
    std::vector<std::string> rpaths_to_fix;
    std::map<std::string, std::vector<std::string>>::iterator found
        = rpaths_per_file.find(original_file);
    if (found != rpaths_per_file.end()) {
        rpaths_to_fix = found->second;
    }

    for (size_t i = 0; i < rpaths_to_fix.size(); ++i) {
        std::string command = std::string("install_name_tool -rpath \"")
            + rpaths_to_fix[i] + "\" \"" + Settings::inside_lib_path()
            + "\" \"" + file_to_fix + "\"";
        if (systemp(command) != 0) {
            std::cerr << "\n\nError : An error occured while trying to fix "
                         "dependencies of "
                      << file_to_fix << std::endl;
        }
    }
}

void addDependency(const std::string& path, const std::string& filename)
{
    Dependency dep(path, filename);

    // we need to check if this library was already added to avoid duplicates
    bool in_deps = false;
    const int dep_amount = deps.size();
    for (int n = 0; n < dep_amount; n++) {
        if (dep.mergeIfSameAs(deps[n]))
            in_deps = true;
    }

    // check if this library was already added to |deps_per_file[filename]| to
    // avoid duplicates
    std::vector<Dependency> deps_in_file = deps_per_file[filename];
    bool in_deps_per_file = false;
    const int deps_in_file_amount = deps_in_file.size();
    for (int n = 0; n < deps_in_file_amount; n++) {
        if (dep.mergeIfSameAs(deps_in_file[n]))
            in_deps_per_file = true;
    }

    if (!Settings::isPrefixBundled(dep.getPrefix()))
        return;

    if (!in_deps)
        deps.push_back(dep);
    if (!in_deps_per_file)
        deps_per_file[filename].push_back(dep);
}

/*
 *  Fill vector 'lines' with dependencies of given 'filename'
 */
void collectDependencies(const std::string& filename,
                         std::vector<std::string>& lines)
{
    // execute "otool -l" on the given file and collect the command's output
    std::string cmd = "otool -l \"" + filename + "\"";
    std::string output = system_get_output(cmd);

    if (output.find("can't open file") != std::string::npos
        or output.find("No such file") != std::string::npos
        or output.size() < 1) {
        std::cerr << "Cannot find file " << filename
                  << " to read its dependencies" << std::endl;
        exit(1);
    }

    // split output
    std::vector<std::string> raw_lines;
    tokenize(output, "\n", &raw_lines);

    bool searching = false;
    for (const auto& line : raw_lines) {
        const auto& is_prefix = [&line](const char* const p) {
            return line.find(p) != std::string::npos;
        };
        if (is_prefix("cmd LC_LOAD_DYLIB")
            || is_prefix("cmd LC_REEXPORT_DYLIB")) {
            if (searching) {
                std::cerr
                    << "\n\n/!\\ ERROR: Failed to find name before next cmd"
                    << std::endl;
                exit(1);
            }
            searching = true;
        } else if (searching) {
            size_t found = line.find("name ");
            if (found != std::string::npos) {
                lines.push_back('\t'
                                + line.substr(found + 5, std::string::npos));
                searching = false;
            }
        }
    }
}


void collectDependencies(const std::string& filename)
{
    if (deps_collected.find(filename) != deps_collected.end())
        return;

    collectRpaths(filename);

    std::vector<std::string> lines;
    collectDependencies(filename, lines);

    std::cout << ".";
    fflush(stdout);

    for (const auto& line : lines) {
        std::cout << ".";
        fflush(stdout);
        if (line[0] != '\t')
            continue; // only lines beginning with a tab interest us
        if (line.find(".framework") != std::string::npos)
            continue; // Ignore frameworks, we can not handle them

        // trim useless info, keep only library name
        std::string dep_path = line.substr(1, line.rfind(" (") - 1);
        if (Settings::isSystemLibrary(dep_path))
            continue;

        addDependency(dep_path, filename);
    }

    deps_collected[filename] = true;
}

void collectSubDependencies()
{
    // print status to user
    size_t dep_amount = deps.size();

    // recursively collect each dependencie's dependencies
    while (true) {
        dep_amount = deps.size();
        for (size_t n = 0; n < dep_amount; n++) {
            std::cout << ".";
            fflush(stdout);
            std::string original_path = deps[n].getOriginalPath();
            if (isRpath(original_path))
                original_path = searchFilenameInRpaths(original_path);

            collectDependencies(original_path);
        }

        if (deps.size() == dep_amount)
            break; // no more dependencies were added on this iteration, stop
                   // searching
    }
}

void createDestDir()
{
    std::string dest_folder = Settings::destFolder();
    std::cout << "* Checking output directory " << dest_folder.c_str()
              << std::endl;

    // ----------- check dest folder stuff ----------
    bool dest_exists = fileExists(dest_folder);

    if (dest_exists and Settings::canOverwriteDir()) {
        std::cout << "* Erasing old output directory " << dest_folder.c_str()
                  << std::endl;
        std::string command = std::string("rm -r \"") + dest_folder + "\"";
        if (systemp(command) != 0) {
            std::cerr << "\n\nError : An error occured while attempting to "
                         "overwrite dest folder."
                      << std::endl;
            exit(1);
        }
        dest_exists = false;
    }

    if (!dest_exists) {

        if (Settings::canCreateDir()) {
            std::cout << "* Creating output directory " << dest_folder.c_str()
                      << std::endl;
            std::string command = std::string("mkdir -p \"") + dest_folder
                + "\"";
            if (systemp(command) != 0) {
                std::cerr << "\n\nError : An error occured while creating "
                             "dest folder."
                          << std::endl;
                exit(1);
            }
        } else {
            std::cerr
                << "\n\nError : Dest folder does not exist. Create it or pass "
                   "the appropriate flag for automatic dest dir creation."
                << std::endl;
            exit(1);
        }
    }
}

void doneWithDeps_go()
{
    std::cout << std::endl;
    const int dep_amount = deps.size();
    // print info to user
    for (int n = 0; n < dep_amount; n++) {
        deps[n].print();
    }
    std::cout << std::endl;

    // copy files if requested by user
    if (Settings::bundleLibs()) {
        createDestDir();

        for (int n = dep_amount - 1; n >= 0; n--) {
            std::cout << "\n* Processing dependency "
                      << deps[n].getInstallPath() << std::endl;
            deps[n].copyYourself();
            changeLibPathsOnFile(deps[n].getInstallPath());
            fixRpathsOnFile(deps[n].getOriginalPath(),
                            deps[n].getInstallPath());
            adhocCodeSign(deps[n].getInstallPath());
        }
    }

    const int fileToFixAmount = Settings::fileToFixAmount();
    for (int n = fileToFixAmount - 1; n >= 0; n--) {
        std::cout << "\n* Processing " << Settings::fileToFix(n) << std::endl;
        copyFile(Settings::fileToFix(n),
                 Settings::fileToFix(n)); // to set write permission
        changeLibPathsOnFile(Settings::fileToFix(n));
        fixRpathsOnFile(Settings::fileToFix(n), Settings::fileToFix(n));
        adhocCodeSign(Settings::fileToFix(n));
    }
}


namespace Settings {

bool overwrite_files = false;
bool overwrite_dir = false;
bool create_dir = false;
bool codesign = true;

bool canOverwriteFiles() { return overwrite_files; }
bool canOverwriteDir() { return overwrite_dir; }
bool canCreateDir() { return create_dir; }
bool canCodesign() { return codesign; }

void canOverwriteFiles(bool permission) { overwrite_files = permission; }
void canOverwriteDir(bool permission) { overwrite_dir = permission; }
void canCreateDir(bool permission) { create_dir = permission; }
void canCodesign(bool permission) { codesign = permission; }


bool bundleLibs_bool = false;
bool bundleLibs() { return bundleLibs_bool; }
void bundleLibs(bool on) { bundleLibs_bool = on; }


std::string dest_folder_str = "./libs/";
std::string destFolder() { return dest_folder_str; }
void destFolder(const std::string& path)
{
    dest_folder_str = path;
    // fix path if needed so it ends with '/'
    if (dest_folder_str[dest_folder_str.size() - 1] != '/')
        dest_folder_str += "/";
}

std::vector<std::string> files;
void addFileToFix(const std::string& path) { files.push_back(path); }
int fileToFixAmount() { return files.size(); }
std::string fileToFix(const int n) { return files[n]; }

std::string inside_path_str = "@executable_path/../libs/";
std::string inside_lib_path() { return inside_path_str; }
void inside_lib_path(const std::string& p)
{
    inside_path_str = p;
    // fix path if needed so it ends with '/'
    if (inside_path_str[inside_path_str.size() - 1] != '/')
        inside_path_str += "/";
}

std::vector<std::string> prefixes_to_ignore;
void ignore_prefix(std::string prefix)
{
    if (prefix[prefix.size() - 1] != '/')
        prefix += "/";
    prefixes_to_ignore.push_back(prefix);
}

bool isSystemLibrary(const std::string& prefix)
{
    if (prefix.find("/usr/lib/") == 0)
        return true;
    if (prefix.find("/System/Library/") == 0)
        return true;

    return false;
}

bool isPrefixIgnored(const std::string& prefix)
{
    const int prefix_amount = prefixes_to_ignore.size();
    for (int n = 0; n < prefix_amount; n++) {
        if (prefix.compare(prefixes_to_ignore[n]) == 0)
            return true;
    }

    return false;
}

bool isPrefixBundled(const std::string& prefix)
{
    if (prefix.find(".framework") != std::string::npos)
        return false;
    if (prefix.find("@executable_path") != std::string::npos)
        return false;
    if (isSystemLibrary(prefix))
        return false;
    if (isPrefixIgnored(prefix))
        return false;

    return true;
}

std::vector<std::string> searchPaths;
void addSearchPath(const std::string& path) { searchPaths.push_back(path); }
int searchPathAmount() { return searchPaths.size(); }
std::string searchPath(const int n) { return searchPaths[n]; }

}


void tokenize(const string& str, const char* delim, vector<string>* vectorarg)
{
    vector<string>& tokens = *vectorarg;

    string delimiters(delim);

    // skip delimiters at beginning.
    string::size_type lastPos = str.find_first_not_of(delimiters, 0);

    // find first "non-delimiter".
    string::size_type pos = str.find_first_of(delimiters, lastPos);

    while (string::npos != pos || string::npos != lastPos) {
        // found a token, add it to the vector.
        tokens.push_back(str.substr(lastPos, pos - lastPos));

        // skip delimiters.  Note the "not_of"
        lastPos = str.find_first_not_of(delimiters, pos);

        // find next "non-delimiter"
        pos = str.find_first_of(delimiters, lastPos);
    }
}


bool fileExists(const std::string& filename)
{
    if (access(filename.c_str(), F_OK) != -1) {
        return true; // file exists
    } else {
        // std::cout << "access(filename) returned -1 on filename [" <<
        // filename << "] I will try trimming." << std::endl;
        std::string delims = " \f\n\r\t\v";
        std::string rtrimmed = filename.substr(
            0, filename.find_last_not_of(delims) + 1);
        std::string ftrimmed = rtrimmed.substr(
            rtrimmed.find_first_not_of(delims));
        if (access(ftrimmed.c_str(), F_OK) != -1) {
            return true;
        } else {
            // std::cout << "Still failed. Cannot find the specified file." <<
            // std::endl;
            return false; // file doesn't exist
        }
    }
}

void copyFile(const string& from, const string& to)
{
    bool override = Settings::canOverwriteFiles();
    if (from != to && !override) {
        if (fileExists(to)) {
            cerr << "\n\nError : File " << to.c_str()
                 << " already exists. Remove it or enable overwriting."
                 << endl;
            exit(1);
        }
    }

    string override_permission = string(override ? "-f " : "-n ");

    // copy file to local directory
    string command = string("cp ") + override_permission + string("\"") + from
        + string("\" \"") + to + string("\"");
    if (from != to && systemp(command) != 0) {
        cerr << "\n\nError : An error occured while trying to copy file "
             << from << " to " << to << endl;
        exit(1);
    }

    // give it write permission
    string command2 = string("chmod +w \"") + to + "\"";
    if (systemp(command2) != 0) {
        cerr << "\n\nError : An error occured while trying to set write "
                "permissions on file "
             << to << endl;
        exit(1);
    }
}

std::string system_get_output(const std::string& cmd)
{
    FILE* command_output;
    char output[128];
    int amount_read = 1;

    std::string full_output;

    try {
        command_output = popen(cmd.c_str(), "r");
        if (command_output == NULL)
            throw;

        while (amount_read > 0) {
            amount_read = fread(output, 1, 127, command_output);
            if (amount_read <= 0)
                break;
            else {
                output[amount_read] = '\0';
                full_output += output;
            }
        }
    } catch (...) {
        std::cerr << "An error occured while executing command " << cmd.c_str()
                  << std::endl;
        pclose(command_output);
        return "";
    }

    int return_value = pclose(command_output);
    if (return_value != 0)
        return "";

    return full_output;
}

int systemp(const std::string& cmd)
{
    std::cout << "    " << cmd.c_str() << std::endl;
    return system(cmd.c_str());
}

void changeInstallName(const std::string& binary_file,
                       const std::string& old_name,
                       const std::string& new_name)
{
    std::string command = std::string("install_name_tool -change \"")
        + old_name + "\" \"" + new_name + "\" \"" + binary_file + "\"";
    if (systemp(command) != 0) {
        std::cerr << "\n\nError: An error occured while trying to fix "
                     "dependencies of "
                  << binary_file << std::endl;
        exit(1);
    }
}

std::string getUserInputDirForFile(const std::string& filename)
{
    const int searchPathAmount = Settings::searchPathAmount();
    for (int n = 0; n < searchPathAmount; n++) {
        auto searchPath = Settings::searchPath(n);
        if (!searchPath.empty() && searchPath[searchPath.size() - 1] != '/')
            searchPath += "/";

        if (fileExists(searchPath + filename)) {
            std::cerr << (searchPath + filename)
                      << " was found. /!\\ DYLIBBUNDLER MAY NOT CORRECTLY "
                         "HANDLE THIS DEPENDENCY: Manually check the "
                         "executable with 'otool -L'"
                      << std::endl;
            return searchPath;
        }
    }

    while (true) {
        std::cout << "Please specify the directory where this library is "
                     "located (or enter 'quit' to abort): ";
        fflush(stdout);

        std::string prefix;
        std::cin >> prefix;
        std::cout << std::endl;

        if (prefix.compare("quit") == 0)
            exit(1);

        if (!prefix.empty() && prefix[prefix.size() - 1] != '/')
            prefix += "/";

        if (!fileExists(prefix + filename)) {
            std::cerr << (prefix + filename) << " does not exist. Try again"
                      << std::endl;
            continue;
        } else {
            std::cerr << (prefix + filename)
                      << " was found. /!\\ DYLIBBUNDLER MAY NOT CORRECTLY "
                         "HANDLE THIS DEPENDENCY: Manually check the "
                         "executable with 'otool -L'"
                      << std::endl;
            Settings::addSearchPath(prefix);
            return prefix;
        }
    }
}

void adhocCodeSign(const std::string& file)
{
    if (Settings::canCodesign() == false)
        return;

    // Add ad-hoc signature for ARM (Apple Silicon) binaries
    std::string signCommand = std::string(
                                  "codesign --force --deep "
                                  "--preserve-metadata=entitlements,"
                                  "requirements,flags,runtime --sign - \"")
        + file + "\"";
    if (systemp(signCommand) != 0) {
        // If the codesigning fails, it may be a bug in Apple's codesign
        // utility. A known workaround is to copy the file to another inode,
        // then move it back erasing the previous file. Then sign again.
        std::cerr << "  * Error : An error occurred while applying ad-hoc "
                     "signature to "
                  << file << ". Attempting workaround" << std::endl;

        std::string machine = system_get_output("machine");
        bool isArm = machine.find("arm") != std::string::npos;
        std::string tempDirTemplate = std::string(
            std::getenv("TMPDIR") + std::string("dylibbundler.XXXXXXXX"));
        std::string filename = file.substr(file.rfind("/") + 1);
        char* tmpDirCstr = mkdtemp((char*)(tempDirTemplate.c_str()));
        if (tmpDirCstr == NULL) {
            std::cerr << "  * Error : Unable to create temp directory for "
                         "signing workaround"
                      << std::endl;
            if (isArm) {
                exit(1);
            }
        }
        std::string tmpDir = std::string(tmpDirCstr);
        std::string tmpFile = tmpDir + "/" + filename;
        const auto runCommand = [isArm](const std::string& command,
                                        const std::string& errMsg) {
            if (systemp(command) != 0) {
                std::cerr << errMsg << std::endl;
                if (isArm) {
                    exit(1);
                }
            }
        };
        std::string command = std::string("cp -p \"") + file + "\" \""
            + tmpFile + "\"";
        runCommand(command,
                   "  * Error : An error occurred copying " + file + " to "
                       + tmpDir);
        command = std::string("mv -f \"") + tmpFile + "\" \"" + file + "\"";
        runCommand(command,
                   "  * Error : An error occurred moving " + tmpFile + " to "
                       + file);
        command = std::string("rm -rf \"") + tmpDir + "\"";
        systemp(command);
        runCommand(
            signCommand,
            "  * Error : An error occurred while applying ad-hoc signature to "
                + file);
    }
}


const std::string VERSION = "1.0.5";


// FIXME - no memory management is done at all (anyway the program closes
// immediately so who cares?)

std::string installPath = "";


void showHelp()
{
    std::cout << "dylibbundler " << VERSION << std::endl;
    std::cout << "dylibbundler is a utility that helps bundle dynamic "
                 "libraries inside macOS app bundles.\n"
              << std::endl;

    std::cout << "-x, --fix-file <file to fix (executable or app plug-in)>"
              << std::endl;
    std::cout << "-b, --bundle-deps" << std::endl;
    std::cout << "-d, --dest-dir <directory to send bundled libraries "
                 "(relative to cwd)>"
              << std::endl;
    std::cout
        << "-p, --install-path <'inner' path of bundled libraries (usually "
           "relative to executable, by default '@executable_path/../libs/')>"
        << std::endl;
    std::cout
        << "-s, --search-path <directory to add to list of locations searched>"
        << std::endl;
    std::cout << "-of, --overwrite-files (allow overwriting files in output "
                 "directory)"
              << std::endl;
    std::cout << "-od, --overwrite-dir (totally overwrite output directory if "
                 "it already exists. implies --create-dir)"
              << std::endl;
    std::cout << "-cd, --create-dir (creates output directory if necessary)"
              << std::endl;
    std::cout << "-ns, --no-codesign (disables ad-hoc codesigning)"
              << std::endl;
    std::cout << "-i, --ignore <location to ignore> (will ignore libraries in "
                 "this directory)"
              << std::endl;
    std::cout << "-h, --help" << std::endl;
}

int main(int argc, char* const argv[])
{

    // parse arguments
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-x") == 0 or strcmp(argv[i], "--fix-file") == 0) {
            i++;
            Settings::addFileToFix(argv[i]);
            continue;
        } else if (strcmp(argv[i], "-b") == 0
                   or strcmp(argv[i], "--bundle-deps") == 0) {
            Settings::bundleLibs(true);
            continue;
        } else if (strcmp(argv[i], "-p") == 0
                   or strcmp(argv[i], "--install-path") == 0) {
            i++;
            Settings::inside_lib_path(argv[i]);
            continue;
        } else if (strcmp(argv[i], "-i") == 0
                   or strcmp(argv[i], "--ignore") == 0) {
            i++;
            Settings::ignore_prefix(argv[i]);
            continue;
        } else if (strcmp(argv[i], "-d") == 0
                   or strcmp(argv[i], "--dest-dir") == 0) {
            i++;
            Settings::destFolder(argv[i]);
            continue;
        } else if (strcmp(argv[i], "-of") == 0
                   or strcmp(argv[i], "--overwrite-files") == 0) {
            Settings::canOverwriteFiles(true);
            continue;
        } else if (strcmp(argv[i], "-od") == 0
                   or strcmp(argv[i], "--overwrite-dir") == 0) {
            Settings::canOverwriteDir(true);
            Settings::canCreateDir(true);
            continue;
        } else if (strcmp(argv[i], "-cd") == 0
                   or strcmp(argv[i], "--create-dir") == 0) {
            Settings::canCreateDir(true);
            continue;
        } else if (strcmp(argv[i], "-ns") == 0
                   or strcmp(argv[i], "--no-codesign") == 0) {
            Settings::canCodesign(false);
            continue;
        } else if (strcmp(argv[i], "-h") == 0
                   or strcmp(argv[i], "--help") == 0) {
            showHelp();
            exit(0);
        }
        if (strcmp(argv[i], "-s") == 0
            or strcmp(argv[i], "--search-path") == 0) {
            i++;
            Settings::addSearchPath(argv[i]);
            continue;
        } else if (i > 0) {
            // if we meet an unknown flag, abort
            // ignore first one cause it's usually the path to the executable
            std::cerr << "Unknown flag " << argv[i] << std::endl << std::endl;
            showHelp();
            exit(1);
        }
    }

    if (not Settings::bundleLibs() and Settings::fileToFixAmount() < 1) {
        showHelp();
        exit(0);
    }

    std::cout << "* Collecting dependencies";
    fflush(stdout);

    const int amount = Settings::fileToFixAmount();
    for (int n = 0; n < amount; n++)
        collectDependencies(Settings::fileToFix(n));

    collectSubDependencies();
    doneWithDeps_go();

    return 0;
}
