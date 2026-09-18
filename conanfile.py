import os

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy


class ExpressionEngineRecipe(ConanFile):
    """最小可用配方：无依赖的静态库，构建与打包都交给自带的 CMake 安装规则。

    本库零外部依赖（GoogleTest 已 vendor 在仓库里，且打包时关闭），因此不需要
    requirements()；打包内容完全来自 `cmake --install` 的产物，与手工安装同源，
    避免配方里再抄一份文件清单。
    """

    name = "expressionengine"
    version = "1.0.0"
    description = "从 FreeCAD 抽出的表达式引擎与单位模块，可脱离 FreeCAD 文档模型使用"
    license = "LGPL-2.1-or-later"
    package_type = "static-library"
    settings = "os", "arch", "compiler", "build_type"

    # 只带构建必需的部分：用例与第三方测试框架、基准都不进包，也不参与构建。
    exports_sources = (
        "CMakeLists.txt",
        "cmake/*",
        "src/*",
        "LICENSE",
        "NOTICE",
    )

    def layout(self):
        cmake_layout(self)

    def generate(self):
        toolchain = CMakeToolchain(self)
        # 包内不构建用例与基准：测试依赖 vendor 的 GoogleTest，基准需要 Release 才有意义。
        toolchain.cache_variables["EXPRESSIONENGINE_BUILD_TESTS"] = "OFF"
        toolchain.cache_variables["EXPRESSIONENGINE_BUILD_BENCHMARKS"] = "OFF"
        toolchain.generate()

        # 无依赖，CMakeDeps 只为对齐常规配方结构；下游拿到的仍是本库的导出目标。
        dependencies = CMakeDeps(self)
        dependencies.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        # 与手工安装完全同一条路径，包里的 include/ 布局与 lib/cmake 配置即安装产物。
        cmake = CMake(self)
        cmake.install()

        copy(
            self,
            "LICENSE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        copy(
            self,
            "NOTICE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )

    def package_info(self):
        # 让下游用同一套目标名：find_package(ExpressionEngine) + ExpressionEngine::ExpressionEngine。
        self.cpp_info.set_property("cmake_file_name", "ExpressionEngine")
        self.cpp_info.set_property("cmake_target_name", "ExpressionEngine::ExpressionEngine")
        self.cpp_info.libs = ["ExpressionEngine"]
