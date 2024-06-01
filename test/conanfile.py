from conan import ConanFile


class Recipe(ConanFile):
    settings = ["os", "compiler", "build_type", "arch"]
    generators = ["CMakeToolchain", "CMakeDeps"]
    requires = ["boost/1.83.0", "boost-ext-ut/1.1.9", "fmt/10.2.1"]

    def layout(self):
        self.folders.generators = "conan"
