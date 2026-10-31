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

def _create_blocklist(ctx):
    output = ctx.actions.declare_file("{}/modules.blocklist".format(ctx.label.name))
    transitive_inputs = []

    inputs = []

    modules_blocklist = ctx.file.modules_blocklist.path
    inputs.append(ctx.file.modules_blocklist)
    hermetic_tools = hermetic_toolchain.get(ctx)

    command = hermetic_tools.setup + """
        touch {output}
        if [ -f {modules_blocklist} ]; then
          for m in $(cat {modules_blocklist}); do
            echo "blocklist $(basename ${{m}})" >> {output}
          done
        else
          : > {output}
        fi
    """.format(
        output = output.path,
        modules_blocklist = modules_blocklist,
    )

    ctx.actions.run_shell(
        mnemonic = "Blocklist",
        inputs = depset(inputs, transitive = transitive_inputs),
        outputs = [output],
        tools = hermetic_tools.deps,
        progress_message = "Creating System or Vendor DLKM Modules Blocklist {}".format(ctx.label),
        command = command,
    )

    return DefaultInfo(files = depset([output]))


create_blocklist = rule(
    implementation = _create_blocklist,
    doc = "Create system_dlkm or vendor_dlkm blocklist",
    attrs =  {
        "modules_blocklist": attr.label(
            doc = "A list of system_dlkm or vendor_dlkm blocklist",
            mandatory = True,
            allow_single_file = True,
        ),
    },
    toolchains = [hermetic_toolchain.type],
)
