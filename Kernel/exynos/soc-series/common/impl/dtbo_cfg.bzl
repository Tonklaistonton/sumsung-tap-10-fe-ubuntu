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

def _dtbo_cfg_impl(ctx):
    output = ctx.actions.declare_file("{}/dtbo.img".format(ctx.label.name))
    dtbo_staging_dir = output.dirname + "/staging"
    inputs = []
    transitive_inputs = []

    inputs += ctx.files.srcs
    inputs += [ctx.file.config_file]
    hermetic_tools = hermetic_toolchain.get(ctx)
    command = hermetic_tools.setup + """
        mkdir -p {dtbo_staging_dir}
    """.format(
        dtbo_staging_dir = dtbo_staging_dir,
    )

    for dtbo in ctx.files.srcs:
        command += """
            cp -vf {dtbo} {dtbo_staging_dir}
        """.format(
            dtbo = dtbo.path,
            dtbo_staging_dir = dtbo_staging_dir,
        )

    command += """
        # make dtbo w/cfg
        mkdtimg cfg_create {output} ${{MKDTIMG_FLAGS}} {config} -d {dtbo_staging_dir}
    """.format(
        output = output.path,
        config = ctx.file.config_file.path,
        dtbo_staging_dir = dtbo_staging_dir,
    )

    ctx.actions.run_shell(
        mnemonic = "Dtbo",
        inputs = depset(inputs, transitive = transitive_inputs),
        outputs = [output],
        tools = hermetic_tools.deps,
        progress_message = "Building dtbo w/cfg{}".format(ctx.label),
        command = command,
    )
    return DefaultInfo(files = depset([output]))

dtbo_cfg = rule(
    implementation = _dtbo_cfg_impl,
    doc = "Build dtbo w/cfg.",
    attrs = {
        "srcs": attr.label_list(
            mandatory = True,
            allow_files = True,
        ),
        "config_file": attr.label(
            mandatory = True,
            allow_single_file = True,
        ),
    },
    toolchains = [hermetic_toolchain.type],
)
