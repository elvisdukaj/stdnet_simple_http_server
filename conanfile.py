from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout, CMakeDeps, CMakeToolchain

class StdexecTestPackage(ConanFile):
  settings = "os", "arch", "compiler", "build_type"
  options = {
          "enable_static_analyzer": [True, False]
          }
  default_options = {
          "enable_static_analyzer": False
          }

  def requirements(self):
    self.requires("stdnet/0.1.0")
    self.requires("fmt/11.0.2")

  def layout(self):
    self.folders.build_folder_vars = [
            "settings.os",
            "settings.arch",
            "settings.compiler",
            "settings.compiler.version"
            ]
    cmake_layout(self)

  def generate(self):
    tc = CMakeToolchain(self)
    tc.cache_variables["CMAKE_EXPORT_COMPILE_COMMANDS"] = True
    tc.cache_variables["ENABLE_STATIC_ANALYZER"] = self.options.enable_static_analyzer
    tc.generate()

    deps = CMakeDeps(self)
    deps.generate()

  def build(self):
    cmake = CMake(self)
    cmake.configure()
    cmake.build()

  def package_id(self):
    self.info.options.rm_safe("enable_static_analyzer")

