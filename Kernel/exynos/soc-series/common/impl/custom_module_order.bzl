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

def _custom_module_order_impl(ctx):
    output = ctx.actions.declare_file("{}/vendor_boot.modules.reorder".format(ctx.label.name))
    staging_dir = output.dirname + "/staging"
    inputs = []
    transitive_inputs = []

    inputs += [ctx.file.early_module_list]
    inputs += [ctx.file.module_list]
    hermetic_tools = hermetic_toolchain.get(ctx)
    command = hermetic_tools.setup + """
        mkdir -p {staging_dir}
    """.format(
        staging_dir = staging_dir,
    )

    command += """
        for m in $(cat {module_early_list}); do
            grep $(basename $m) {module_list} >> {output}
        done
        grep -v -f {output} {module_list} >> {output}
    """.format(
        output = output.path,
        module_early_list = ctx.file.early_module_list.path,
        module_list = ctx.file.module_list.path,
    )

    ctx.actions.run_shell(
        mnemonic = "KoOrder",
        inputs = depset(inputs, transitive = transitive_inputs),
        outputs = [output],
        tools = hermetic_tools.deps,
        progress_message = "{}:Reordering module.order".format(ctx.label),
        command = command,
    )
    return DefaultInfo(files = depset([output]))

custom_module_order = rule(
    implementation = _custom_module_order_impl,
    doc = "Customize module.order..",
    attrs = {
        "early_module_list": attr.label(
            doc = "A list of modules loading early",
            mandatory = True,
            allow_single_file = True,
        ),
        "module_list": attr.label(
            doc = "A list of all modules",
            mandatory = True,
            allow_single_file = True,
        )
    },
    toolchains = [hermetic_toolchain.type],
)
