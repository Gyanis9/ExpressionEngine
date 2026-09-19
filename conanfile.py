import os

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy


class ExpressionEngineRecipe(ConanFile):
    """最小可用配方：静态库本体零外部依赖，构建与打包都交给自带的 CMake 安装规则。

    库本体不需要任何第三方依赖；测试用的 GoogleTest 由 conandata.yml 声明（改由
    Conan 供给，便于消费者复用同一份配置），故 requirements() 只消费那份清单。
    打包内容完全来自 `cmake --install` 的产物，与手工安装同源，避免配方里再抄一份
    文件清单。
    """

    name = "expressionengine"
    version = "1.0.0"
    description = "表达式引擎与单位模块：解析并求值带单位的表达式，可脱离宿主文档模型使用"
    license = "LGPL-2.1-or-later"
    package_type = "static-library"
    settings = "os", "arch", "compiler", "build_type"

    # 只带构建必需的部分：用例与基准都不进包，也不参与构建。
    # conandata.yml 必须一起导出，self.conan_data 才有内容可供 requirements() 读取。
    exports_sources = (
        "CMakeLists.txt",
        "cmake/*",
        "src/*",
        "conandata.yml",
        "LICENSE",
        "NOTICE",
    )

    def requirements(self):
        """按 conandata.yml 声明依赖；该文件由 Conan 插件维护，这里只负责消费。"""
        for requirement in self.conan_data.get("requirements", []):
            self.requires(requirement)

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
