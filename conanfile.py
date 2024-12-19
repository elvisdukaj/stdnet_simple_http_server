from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout, CMakeDeps, CMakeToolchain

class StdexecTestPackage(ConanFile):
  settings = "os", "arch", "compiler", "build_type"

  def requirements(self):
    self.requires("stdnet/0.1.0")
    self.requires("fmt/11.0.2")

  def generate(self):
    tc = CMakeToolchain(self)
    tc.variables["CMAKE_EXPORT_COMPILE_COMMANDS"] = True
    tc.generate()

    deps = CMakeDeps(self)
    deps.generate()

  def build(self):
    cmake = CMake(self)
    cmake.configure()
    cmake.build()

  def layout(self):
    cmake_layout(self)

