# Copyright (C) 2022 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


load("//build/kernel/kleaf:hermetic_tools.bzl", "hermetic_toolchain")

def _check_module_list(ctx):
    hermetic_tools = hermetic_toolchain.get(ctx)
    transitive_inputs = []
    modules_install_output_list = []
    output = []
    inputs = []
    pyinput = []
    pyoutput = []

    args = ctx.actions.args()

    out_vendor_boot_list = ctx.actions.declare_file("vendor_boot_modules_list")
    out_vendor_dlkm_list = ctx.actions.declare_file("vendor_dlkm_modules_list")

    modules_install_output = ctx.attr.kernel_modules_install[DefaultInfo].files.to_list()

    modules_dep = ctx.actions.declare_file("modules.dep")
    modules_alias = ctx.actions.declare_file("modules.alias")

    output.append(modules_dep)
    output.append(modules_alias)

    for out in modules_install_output:
        modules_install_output_list.append(out)

    inputs += ctx.attr.kernel_modules_install[DefaultInfo].files.to_list()

    command = hermetic_tools.setup + """
        #echo $(realpath {modules_dep}) >> {modules_dep}
        for f in {mod_files}; do
          if [[ "${{f}}" =~ "modules.dep" ]]; then
            cp -pL ${{f}} {modules_dep}
          elif [[ "${{f}}" =~ "modules.alias" ]]; then
            cp -pL ${{f}} {modules_alias}
          fi
        done
    """.format(
        modules_dep = modules_dep.path,
        modules_alias = modules_alias.path,
        mod_files = " ".join([out.path for out in modules_install_output_list]),
    )

    ctx.actions.run_shell(
        mnemonic = "FindModulesFiles",
        inputs = depset(inputs, transitive = transitive_inputs),
        outputs = output,
        tools = hermetic_tools.deps,
        progress_message = "Find modules.dep and modules.alias {}".format(ctx.label),
        command = command,
    )

    args.add_all("--vendor_boot_modules", [ctx.file.vendor_boot_kernel_modules])
    args.add_all("--vendor_dlkm_modules", [ctx.file.vendor_dlkm_kernel_modules])
    args.add_all("--system_dlkm_modules", [ctx.file.system_dlkm_kernel_modules])
    args.add_all("--modules_dep", [modules_dep])
    args.add_all("--modules_alias", [modules_alias])
    args.add_all("--out_vendor_boot_modules_list", [out_vendor_boot_list])
    args.add_all("--out_vendor_dlkm_modules_list", [out_vendor_dlkm_list])

    pyinput.append(ctx.file.vendor_boot_kernel_modules)
    pyinput.append(ctx.file.system_dlkm_kernel_modules)
    pyinput.append(ctx.file.vendor_dlkm_kernel_modules)
    pyinput += output

    pyoutput.append(out_vendor_boot_list)
    pyoutput.append(out_vendor_dlkm_list)

    ctx.actions.run(
        inputs = depset(pyinput, transitive = transitive_inputs),
        outputs = pyoutput,
        executable = ctx.executable._modules_graph,
        arguments = [args],
        mnemonic = "CreateModulesGraphfile",
        progress_message = "Generate modules graph for {} {}".format(ctx.attr.name, ctx.label),
    )

    return DefaultInfo(files = depset(pyoutput))


check_module_list = rule(
    implementation = _check_module_list,
    doc = "Check whether the modules are in the appropriate location (vendor_boot, vendor_dlkm).",
    attrs =  {
        "_modules_graph": attr.label(
            default = "//exynos/soc-series/common/impl:create_modules_graph",
            executable = True,
            cfg = "exec",
        ),
        "kernel_modules_install": attr.label(
            doc = "A result of kernel_modules_install",
        ),
        "vendor_boot_kernel_modules": attr.label(
            doc = "A list of vendor_boot modules",
            mandatory = True,
            allow_single_file = True,
        ),
        "system_dlkm_kernel_modules": attr.label(
            doc = "A list of system_dlkm modules",
            mandatory = True,
            allow_single_file = True,
        ),
        "vendor_dlkm_kernel_modules": attr.label(
            doc = "A list of vendor_dlkm modules",
            mandatory = True,
            allow_single_file = True,
        ),
    },

    toolchains = [hermetic_toolchain.type],
)
